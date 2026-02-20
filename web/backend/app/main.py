"""FastAPI entry point for the traffic simulation REST API."""

from __future__ import annotations

import os
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .models import (
    AddVehicleRequest,
    IntersectionState,
    ResetResponse,
    StepResponse,
)
from .simulator import simulator

# ---------------------------------------------------------------------------
# App lifecycle
# ---------------------------------------------------------------------------

@asynccontextmanager
async def lifespan(app: FastAPI):
    simulator.startup()
    yield


app = FastAPI(
    title="Traffic Simulation API",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---------------------------------------------------------------------------
# API routes
# ---------------------------------------------------------------------------

@app.get("/api/state", response_model=IntersectionState)
async def get_state():
    """Return the current intersection state (light phases, queue lengths)."""
    return simulator.state()


@app.post("/api/vehicles", status_code=201)
async def add_vehicle(req: AddVehicleRequest):
    """Add a vehicle to a lane queue.

    - `start_road` and `end_road` must differ.
    - `vehicle_id` must be unique within the simulation.
    """
    if req.start_road == req.end_road:
        raise HTTPException(status_code=422, detail="U-turns are not allowed")
    simulator.add_vehicle(req)
    return {"ok": True, "vehicle_id": f"Car{req.vehicle_id}"}


@app.post("/api/step", response_model=StepResponse)
async def step():
    """Advance the simulation by one step.

    Returns the list of vehicle IDs that departed during this step.
    """
    try:
        return simulator.step()
    except BrokenPipeError:
        raise HTTPException(status_code=503, detail="Simulator process died; call /api/reset")


@app.post("/api/reset", response_model=ResetResponse)
async def reset():
    """Reset the simulation to the initial state."""
    simulator.reset()
    return ResetResponse(ok=True)

# ----
# Frontend mounting
# ----

_frontend = Path(os.getenv("FRONTEND_DIR", str(Path(__file__).resolve().parent.parent.parent / "frontend")))
if _frontend.exists():
    app.mount("/static", StaticFiles(directory=str(_frontend), html=False), name="frontend")

    @app.get("/")
    async def serve_index():
        return FileResponse(_frontend / "index.html")
