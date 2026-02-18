# Traffic Simulation

An intelligent traffic light simulation for a four-way intersection, written in C with embedded portability as a first-class concern.

The simulation accepts a JSON command file, processes `addVehicle` and `step` commands, and produces a JSON report of which vehicles left the intersection at each step.

## Building

```bash
make        # configure + build
make test   # build + run all tests
make clean  # remove build artefacts
```

## Usage

> `main.c` and the Python JSON bridge are not yet implemented (in progress).
> The command below shows the intended interface.

```bash
python3 bridge/bridge.py input.json output.json
```

**Input format**

```json
{
  "commands": [
    { "type": "addVehicle", "vehicleId": "v1", "startRoad": "south", "endRoad": "north" },
    { "type": "step" }
  ]
}
```

**Output format**

```json
{
  "stepStatuses": [
    { "leftVehicles": ["v1"] }
  ]
}
```

U-turns (`startRoad == endRoad`) are rejected as invalid input.

---

## Architecture

```
traffic_sim/
├── include/
│   ├── config.h          # compile-time limits (queue sizes, timing constants)
│   ├── types.h           # all shared enums and structs, no logic
│   ├── road.h            # ring-buffer queue + movement derivation
│   ├── traffic_light.h   # per-road FSM
│   └── controller.h      # adaptive phase scheduling algorithm
├── src/
│   ├── road.c
│   ├── traffic_light.c
│   ├── controller.c
│   ├── intersection.c    # (in progress)
│   ├── simulation.c      # (in progress)
│   ├── hal_desktop.c     # (in progress) — HAL impl for desktop
│   └── main.c            # (in progress)
├── bridge/
│   └── bridge.py         # (in progress) — JSON ↔ line-protocol adapter
└── tests/
    ├── test_helpers.h    # minimal zero-dependency test harness
    ├── test_road.c
    ├── test_traffic_light.c
    └── test_controller.c
```

### HAL layer

`hal.h` (in progress) will define a small interface for output and time:

```c
typedef struct {
    void     (*output_vehicles)(const char **ids, uint8_t count);
    uint32_t (*get_tick)(void);
} HAL;
```

On desktop: writes to stdout, uses the system clock.
On embedded: swap in a UART write and `HAL_GetTick()` — no other changes needed.

---

## Lane model

Each road has **three lanes**:

| Lane | Index | Active in |
|------|-------|-----------|
| Left-turn only | `LANE_LEFT` (0) | Arrow phases |
| Straight | `LANE_STRAIGHT` (1) | Main phases (NS / EW) |
| Straight + right turn | `LANE_RIGHT` (2) | Main phases (NS / EW) |

Right turns share the main phase because they stay in the corner of the intersection and do not cross any opposing vehicle path.

The lane for a vehicle is derived once at `addVehicle` time from `(startRoad, endRoad)` using a compile-time lookup table (right-hand traffic):

| start \ end | N | S | E | W |
|-------------|---|---|---|---|
| **N** | — | straight | left | **right** |
| **S** | straight | — | **right** | left |
| **E** | **right** | left | — | straight |
| **W** | left | **right** | straight | — |

---

## Traffic light FSM

Each of the four roads has an independent traffic light that steps through:

```
set_green(N) / set_green_arrow(N)
       │
┌──────▼──────────────────┐
│  RED                    │◄──────────────────────────┐
└─────────────────────────┘                           │
                                                      │
set_green(N)         set_green_arrow(N)               │
       │                      │                       │
┌──────▼──────┐      ┌────────▼───────┐               │
│   GREEN     │      │  GREEN_ARROW   │               │
│   N steps   │      │  N steps       │               │
└──────┬──────┘      └────────┬───────┘               │
       └──────────┬───────────┘                       │
                  │ steps_remaining == 0              │
           ┌──────▼──────┐   steps_remaining == 0     │
           │   YELLOW    │───────────────────────────►┘
           │ YELLOW_STEPS│
           └─────────────┘
```

`tick()` is called **after** the vehicle check each step, so a light set to `GREEN` with duration `N` allows exactly `N` vehicles through before transitioning. `tick()` on a `RED` light is a no-op, safe to call unconditionally on all four roads every step.

---

## Adaptive scheduling algorithm

The controller is a pure function of the intersection state: it reads queue lengths and wait times, and returns the next phase and its duration. It never modifies any state itself.

### Phase table

| Phase | Green roads | Green lanes |
|-------|-------------|-------------|
| `PHASE_NS` | North + South | STRAIGHT, RIGHT |
| `PHASE_EW` | East + West | STRAIGHT, RIGHT |
| `PHASE_N_ARROW` | North | LEFT |
| `PHASE_S_ARROW` | South | LEFT |
| `PHASE_E_ARROW` | East | LEFT |
| `PHASE_W_ARROW` | West | LEFT |

### Priority score

Each phase is scored as

```
score(phase) = Σ  queue_count(road, lane) × (1 + wait_steps(road, lane))
```

where `wait_steps` is the number of simulation steps the front vehicle in that lane has been waiting. The wait-time multiplier prevents starvation: a road skipped for many steps accumulates a growing score even with few vehicles.

### Phase selection

The phase with the highest score is selected. Ties are broken in favour of the current phase; initialising `best_score` to the current phase's own score means another phase must strictly outweigh it to trigger a switch.

### Green duration

```
duration = clamp(total_vehicles_in_active_lanes, MIN_GREEN_STEPS, MAX_GREEN_STEPS)
```

Roughly one step per waiting vehicle, bounded to avoid micro-oscillation
(too short) and monopolisation (too long). Both limits are set in `config.h`.

---

## Step semantics

On each `step` command:

1. For every road currently on **GREEN** or **GREEN_ARROW**: dequeue the front
   vehicle from each active lane → add to `leftVehicles`
2. Tick all four traffic lights (RED lights are a no-op)
3. If `phase_steps_remaining` reaches 0 → controller selects next phase;
   lights transition through YELLOW before the new phase goes GREEN

A step where the active light is YELLOW produces `leftVehicles: []` for that
road — yellow steps count as simulation steps in the output.

---
