# Traffic Simulation

An intelligent traffic light simulation for a four-way intersection, written in C with embedded portability as a main concern.

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
│   ├── intersection.h    # intersection lifecycle, step, add-vehicle
│   ├── controller.h      # adaptive phase scheduling algorithm
│   ├── hal.h             # EmbeddedHAL interface (sense_lane / set_light)
│   └── simulation.h      # SimulationContext + simulation_tick (embedded loop)
├── src/
│   ├── road.c
│   ├── traffic_light.c
│   ├── controller.c
│   ├── intersection.c
│   ├── simulation.c      # platform-neutral embedded tick loop
│   ├── hal_stm32.c       # STM32 GPIO + timer implementation (embedded only)
│   └── main.c            # stdin line-protocol entry point (desktop)
├── bridge/
│   └── bridge.py         # JSON to line-protocol adapter
├── examples/
│   ├── 01_spec_example.json ... 09_left_turn_siege.json
│   └── gen_examples.py   # generator for large scenarios
├── tests/
│   ├── test_helpers.h    # minimal zero-dependency test harness
│   ├── test_road.c
│   ├── test_traffic_light.c
│   ├── test_controller.c
│   └── test_intersection.c
├── web/
│   ├── backend/
│   │   └── app/
│   │       ├── main.py       # FastAPI app (4 REST endpoints)
│   │       ├── models.py     # Pydantic request/response models
│   │       ├── simulator.py  # long-running subprocess wrapper + state mirror
│   │       └── tests/        # pytest integration tests
│   ├── frontend/
│   │   ├── index.html        # layout: canvas + control sidebar
│   │   ├── style.css         # dark theme, responsive
│   │   └── app.js            # canvas renderer + API client
│   ├── Dockerfile            # multi-stage: cmake build → Python runtime
│   └── docker-compose.yml
├── QUICKSTART.md
├── Makefile
└── CMakeLists.txt
```

---

## Web visualiser

A browser-based front end and REST API live under `web/`. The FastAPI backend wraps the `traffic_sim` binary as a long-running subprocess; the canvas frontend renders the intersection live.

### Running locally

```bash
# Build the C binary first (if not already done)
make

# Install Python dependencies
pip install -r web/backend/requirements.txt

# Start the server (serves both API and frontend)
cd web/backend
TRAFFIC_SIM=../../build/traffic_sim uvicorn app.main:app --host 127.0.0.1 --port 8000
```

Open `http://127.0.0.1:8000` in a browser.

### Running with Docker

```bash
docker compose -f web/docker-compose.yml up
```

The Dockerfile builds the C binary in a first stage, so no local toolchain is needed.

### REST API

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/api/state` | Full intersection state: lights, queue lengths, phase |
| `POST` | `/api/vehicles` | Add a vehicle `{vehicle_id, start_road, end_road}` |
| `POST` | `/api/step` | Advance one step; returns `{left_vehicles, step_number}` |
| `POST` | `/api/reset` | Restart the simulation |

Interactive API docs are available at `http://127.0.0.1:8000/docs`.

---

## Broader explanation

in docs folder! :)
