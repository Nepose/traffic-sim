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

See `QUICKSTART.md` for a full walkthrough and the `examples/` directory for ready-made scenarios ranging from 4 to 1 650 vehicles.

---

## Architecture

```
traffic_sim/
├── include/
│   ├── config.h          # compile-time limits (queue sizes, timing constants)
│   ├── types.h           # all shared enums and structs, no logic
│   ├── road.h            # ring-buffer queue + movement derivation
│   ├── traffic_light.h   # per-road FSM
│   ├── intersection.h    # simulation loop (step, add vehicle, query)
│   └── controller.h      # adaptive phase scheduling algorithm
├── src/
│   ├── road.c
│   ├── traffic_light.c
│   ├── controller.c
│   ├── intersection.c
│   └── main.c            # stdin line-protocol entry point
├── bridge/
│   └── bridge.py         # JSON ↔ line-protocol adapter
├── examples/
│   ├── 01_spec_example.json … 09_left_turn_siege.json
│   └── gen_examples.py   # generator for large scenarios
├── tests/
│   ├── test_helpers.h    # minimal zero-dependency test harness
│   ├── test_road.c
│   ├── test_traffic_light.c
│   ├── test_controller.c
│   └── test_intersection.c
├── QUICKSTART.md
├── Makefile
└── CMakeLists.txt
```

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

YELLOW is a **display state only**. It appears on the light at the end of the last green step (after `tick()`) but does not consume a simulation step of its own — the new phase is applied at the very start of the following step, overriding YELLOW before any vehicle check occurs.

---

## Adaptive scheduling algorithm

The controller is a pure function of the intersection state: it reads queue lengths and wait times, and returns the next phase and its duration. It never modifies any state itself.

### Phase table

| Phase | Green roads | Green lanes |
|-------|-------------|-------------|
| `PHASE_NS` | North + South | STRAIGHT, RIGHT |
| `PHASE_EW` | East + West | STRAIGHT, RIGHT |
| `PHASE_NS_ARROW` | North + South | LEFT |
| `PHASE_EW_ARROW` | East + West | LEFT |

Opposing left turns (e.g. North and South) do not conflict in right-hand traffic — they pass through the far side of the centre box, so each arrow phase serves both roads simultaneously.

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

Roughly one step per waiting vehicle, bounded to avoid micro-oscillation (too short) and monopolisation (too long). Both limits are set in `config.h`.

---

## Step semantics

On each `step` command:

1. If `phase_steps_remaining == 0`: the controller selects the next phase and activates it immediately (lights set to GREEN / GREEN_ARROW).
2. For every road currently on **GREEN** or **GREEN_ARROW**: dequeue the front vehicle from each active lane → add to `leftVehicles`.
3. Tick all four traffic lights (RED lights are a no-op).
4. Decrement `phase_steps_remaining`; increment `step_count`.

`addVehicle` commands that appear between two `step` commands take effect before the controller runs at the start of the next step, so they influence the phase decision.

---
