"""Long-running traffic_sim subprocess wrapper.

The C binary reads one command per line from stdin and writes one line per
'step' command to stdout.  We keep the process alive between API calls so
simulation state is preserved across requests.

Thread safety: a single asyncio.Lock serialises all subprocess I/O.
"""

from __future__ import annotations

import os
import subprocess
import threading
import time
from pathlib import Path

from .models import (
    AddVehicleRequest,
    IntersectionState,
    Lane,
    LaneInfo,
    LightInfo,
    LightState,
    Phase,
    RoadDir,
    RoadInfo,
    StepResponse,
)

# ---------------------------------------------------------------------------
# Binary location
# ---------------------------------------------------------------------------

def _find_binary() -> Path:
    env = os.environ.get("TRAFFIC_SIM")
    if env:
        return Path(env)
    # Default: ../../traffic_sim/build/traffic_sim relative to this file
    here = Path(__file__).resolve().parent
    return here.parent.parent.parent / "traffic_sim" / "build" / "traffic_sim"


# ---------------------------------------------------------------------------
# In-memory state mirror
# ---------------------------------------------------------------------------
# The C binary only exposes departures (left_vehicles) per step via stdout.
# We reconstruct the rest of intersection state (queue lengths, light states,
# phase) by tracking it ourselves in Python, mirroring the C logic.
#
# Mirroring rules (simplified but faithful to controller.c / intersection.c):
#   - Phase advances after MAX_GREEN_STEPS or when queues on current roads
#     are empty.  Yellow lasts 1 step between phases.
#   - We don't mirror the adaptive wait-time algorithm exactly; instead we
#     track the raw step count and let the API consumer use /step to drive.
#
# For queue lengths we track enqueue/dequeue ourselves.
#
# Light state mapping per phase:
#   PHASE_NS      -> N+S GREEN,         E+W RED
#   PHASE_EW      -> E+W GREEN,         N+S RED
#   PHASE_NS_ARROW-> N+S GREEN_ARROW,   E+W RED
#   PHASE_EW_ARROW-> E+W GREEN_ARROW,   N+S RED
#   (yellow shown on roads leaving green, all others stay red)

_ROAD_NAMES = {RoadDir.NORTH: "north", RoadDir.SOUTH: "south",
               RoadDir.EAST: "east", RoadDir.WEST: "west"}
_LANE_NAMES = {Lane.LEFT: "left", Lane.STRAIGHT: "straight", Lane.RIGHT: "right"}

# Phases that activate N+S / E+W
_NS_PHASES = {Phase.NS, Phase.NS_ARROW}
_EW_PHASES = {Phase.EW, Phase.EW_ARROW}

# Steps (from config.h)
MIN_GREEN_STEPS = 2
MAX_GREEN_STEPS = 8
YELLOW_STEPS    = 1


def _movement_for(start: RoadDir, end: RoadDir) -> Lane | None:
    """Return which lane the vehicle occupies based on start/end roads."""
    opposites = {RoadDir.NORTH: RoadDir.SOUTH, RoadDir.SOUTH: RoadDir.NORTH,
                 RoadDir.EAST: RoadDir.WEST,   RoadDir.WEST: RoadDir.EAST}
    rights = {RoadDir.NORTH: RoadDir.WEST, RoadDir.SOUTH: RoadDir.EAST,
              RoadDir.EAST: RoadDir.NORTH, RoadDir.WEST: RoadDir.SOUTH}
    if start == end:
        return None  # invalid
    if end == opposites[start]:
        return Lane.STRAIGHT
    if end == rights[start]:
        return Lane.RIGHT
    return Lane.LEFT


class SimulatorState:
    """Pure-Python mirror of intersection state, updated on each API call."""

    def __init__(self) -> None:
        # queues[road][lane] = count
        self.queues: dict[RoadDir, dict[Lane, int]] = {
            road: {lane: 0 for lane in Lane} for road in RoadDir
        }
        self.step_count: int = 0
        self.current_phase: Phase = Phase.NS
        self.phase_steps: int = 0          # steps spent in current phase
        self.in_yellow: bool = False
        self.yellow_from_phase: Phase = Phase.NS

    # ------------------------------------------------------------------
    # Updates
    # ------------------------------------------------------------------

    def add_vehicle(self, req: AddVehicleRequest) -> None:
        lane = _movement_for(req.start_road, req.end_road)
        if lane is None:
            return
        self.queues[req.start_road][lane] += 1

    def apply_step(self, left_vehicles: list[str], departed_meta: list[tuple[RoadDir, Lane]]) -> None:
        """Update state after a step.  departed_meta is computed at step-issue time."""
        self.step_count += 1
        for road, lane in departed_meta:
            if self.queues[road][lane] > 0:
                self.queues[road][lane] -= 1

        # Advance phase mirror
        if self.in_yellow:
            self.in_yellow = False
            # Move to the phase after yellow_from_phase
            self.current_phase = Phase((self.yellow_from_phase + 1) % 4)
            self.phase_steps = 0
        else:
            self.phase_steps += 1
            if self.phase_steps >= MAX_GREEN_STEPS:
                self._start_yellow()
            else:
                # Check if active roads have no vehicles that can move this phase.
                # Only count lanes that are actually served by the current phase;
                # left-turn vehicles wait for the ARROW phase and must not block
                # early termination of the straight/right phase.
                active = self._active_roads(self.current_phase)
                if self.current_phase in {Phase.NS_ARROW, Phase.EW_ARROW}:
                    phase_lanes: list[Lane] = [Lane.LEFT]
                else:
                    phase_lanes = [Lane.STRAIGHT, Lane.RIGHT]
                if self._queues_empty(active, phase_lanes) and self.phase_steps >= MIN_GREEN_STEPS:
                    self._start_yellow()

    def _start_yellow(self) -> None:
        self.in_yellow = True
        self.yellow_from_phase = self.current_phase

    def _active_roads(self, phase: Phase) -> list[RoadDir]:
        if phase in _NS_PHASES:
            return [RoadDir.NORTH, RoadDir.SOUTH]
        return [RoadDir.EAST, RoadDir.WEST]

    def _queues_empty(self, roads: list[RoadDir], lanes: list[Lane] | None = None) -> bool:
        check_lanes = lanes if lanes is not None else list(Lane)
        for road in roads:
            for lane in check_lanes:
                if self.queues[road][lane] > 0:
                    return False
        return True

    # ------------------------------------------------------------------
    # Light state derivation
    # ------------------------------------------------------------------

    def light_for(self, road: RoadDir) -> LightInfo:
        if self.in_yellow:
            active = self._active_roads(self.yellow_from_phase)
            state = LightState.YELLOW if road in active else LightState.RED
        else:
            active = self._active_roads(self.current_phase)
            if road in active:
                if self.current_phase in {Phase.NS_ARROW, Phase.EW_ARROW}:
                    state = LightState.GREEN_ARROW
                else:
                    state = LightState.GREEN
            else:
                state = LightState.RED
        remaining = max(0, MAX_GREEN_STEPS - self.phase_steps)
        return LightInfo(state=state, steps_remaining=remaining)

    # ------------------------------------------------------------------
    # Snapshot
    # ------------------------------------------------------------------

    def snapshot(self) -> IntersectionState:
        roads: list[RoadInfo] = []
        for road in RoadDir:
            lanes = {
                _LANE_NAMES[lane]: LaneInfo(queue_length=self.queues[road][lane])
                for lane in Lane
            }
            roads.append(RoadInfo(
                direction=road,
                light=self.light_for(road),
                lanes=lanes,
            ))
        return IntersectionState(
            step_count=self.step_count,
            current_phase=self.current_phase,
            in_yellow_transition=self.in_yellow,
            roads=roads,
        )


# ---------------------------------------------------------------------------
# Subprocess manager
# ---------------------------------------------------------------------------

class SimulatorProcess:
    """Manages the long-running traffic_sim process."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._proc: subprocess.Popen | None = None
        self._state = SimulatorState()
        self._binary = _find_binary()

    def _start(self) -> None:
        if self._proc and self._proc.poll() is None:
            return
        self._proc = subprocess.Popen(
            [str(self._binary)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,         # line-buffered
        )

    def _send(self, line: str) -> None:
        assert self._proc and self._proc.stdin
        self._proc.stdin.write(line)
        self._proc.stdin.flush()

    def _recv_line(self) -> str:
        assert self._proc and self._proc.stdout
        return self._proc.stdout.readline().rstrip("\n")

    # ------------------------------------------------------------------
    # Public API (thread-safe via lock)
    # ------------------------------------------------------------------

    def startup(self) -> None:
        with self._lock:
            self._start()

    def add_vehicle(self, req: AddVehicleRequest) -> None:
        with self._lock:
            self._start()
            line = f"addVehicle {req.vehicle_id} {req.start_road.name.lower()} {req.end_road.name.lower()}\n"
            self._send(line)
            self._state.add_vehicle(req)

    def step(self) -> StepResponse:
        with self._lock:
            self._start()
            # Snapshot which roads/lanes are active before stepping so we know
            # which queues to decrement when vehicles depart.
            phase = self._state.current_phase
            if self._state.in_yellow:
                # During yellow transition no vehicles depart
                departed_meta: list[tuple[RoadDir, Lane]] = []
            else:
                active_roads = self._state._active_roads(phase)
                if phase in {Phase.NS_ARROW, Phase.EW_ARROW}:
                    active_lanes = [Lane.LEFT]
                else:
                    active_lanes = [Lane.STRAIGHT, Lane.RIGHT]
                departed_meta = [
                    (road, lane)
                    for road in active_roads
                    for lane in active_lanes
                ]

            self._send("step\n")
            raw = self._recv_line()
            left = raw.split() if raw.strip() else []

            self._state.apply_step(left, departed_meta)
            return StepResponse(
                left_vehicles=left,
                step_number=self._state.step_count,
            )

    def state(self) -> IntersectionState:
        with self._lock:
            return self._state.snapshot()

    def reset(self) -> None:
        with self._lock:
            if self._proc:
                try:
                    self._proc.stdin.close()
                    self._proc.wait(timeout=2)
                except Exception:
                    self._proc.kill()
            self._proc = None
            self._state = SimulatorState()
            self._start()


# Singleton instance used by FastAPI

simulator = SimulatorProcess()
