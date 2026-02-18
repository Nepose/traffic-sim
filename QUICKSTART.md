# Quickstart

## Prerequisites

| Tool | Minimum version |
|------|----------------|
| CMake | 3.16 |
| C compiler (gcc / clang) | C11 support |
| Python | 3.8 |
| make | any |

## Build

```bash
make
```

This configures CMake, compiles the core library, the `traffic_sim` binary,
and all test executables.  To verify everything works:

```bash
make test   # should print "100% tests passed"
```

## Run an example

```bash
python3 bridge/bridge.py examples/01_spec_example.json output.json
cat output.json
```

The bridge feeds the JSON input to the simulator binary and writes the
result to `output.json`.

## Input format

```json
{
  "commands": [
    { "type": "addVehicle", "vehicleId": "car1", "startRoad": "north", "endRoad": "south" },
    { "type": "step" }
  ]
}
```

- **`addVehicle`** — adds one vehicle to the queue.  `startRoad` and `endRoad`
  are one of `north`, `south`, `east`, `west`.  U-turns (`start == end`) are
  silently rejected.
- **`step`** — advances the simulation by one tick.  The controller picks the
  best phase, active lanes dequeue one vehicle each, and the lights tick.
  Produces exactly one entry in `stepStatuses`.

Commands can be freely interleaved — `addVehicle` calls between two `step`
commands are visible to the controller that runs at the start of the next step.

## Output format

```json
{
  "stepStatuses": [
    { "leftVehicles": ["car1"] },
    { "leftVehicles": [] }
  ]
}
```

One `stepStatuses` entry per `step` command.  `leftVehicles` lists the IDs of
every vehicle that cleared the intersection during that step.

## Movement → lane mapping (right-hand traffic)

| start \ end | north | south | east | west |
|-------------|-------|-------|------|------|
| **north** | — | straight | **left** | right |
| **south** | straight | — | right | **left** |
| **east** | right | **left** | — | straight |
| **west** | **left** | right | straight | — |

Left turns are served by the protected arrow phases (e.g. `PHASE_NS_ARROW`
gives green arrows to the left-turn lanes on both North and South
simultaneously).  Straight and right-turn lanes share the main phases.

## Examples

| File | Steps | Vehicles | What it shows |
|------|-------|----------|---------------|
| `01_spec_example.json` | 4 | 4 | Canonical README scenario |
| `02_left_turns_paired.json` | 1 | 2 | N+S left turns depart together |
| `03_straight_and_right_together.json` | 1 | 2 | Straight + right from same road in one step |
| `04_min_phase_duration.json` | 2 | 1 | Light stays green for MIN_GREEN_STEPS after queue empties |
| `05_starvation_prevention.json` | 6 | 6 | Wait-time score overrides queue size |
| `06_morning_rush.json` | 300 | ~988 | NS-heavy rush then EW surge |
| `07_all_directions_chaos.json` | 400 | ~1355 | Random arrivals, all phases competing |
| `08_wave_attack.json` | 500 | 800 | Burst waves of 40 vehicles every 25 steps |
| `09_left_turn_siege.json` | 350 | ~1650 | Heavy straight load, left-turners fighting for arrow phases |
