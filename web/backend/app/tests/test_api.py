"""API integration tests.

These tests require the traffic_sim binary to be built.  The binary path can
be overridden via the TRAFFIC_SIM environment variable.

Run with:
    pytest web/backend/app/tests/
"""

from __future__ import annotations

from os import environ

import pytest
from fastapi.testclient import TestClient

# Skip all tests if binary is missing and TRAFFIC_SIM is not set
BINARY_PATH = environ.get(
    "TRAFFIC_SIM",
    str(__file__).replace(
        "web/backend/app/tests/test_api.py",
        "traffic_sim/build/traffic_sim",
    ),
)


def binary_available() -> bool:
    env = environ.get("TRAFFIC_SIM")
    if env:
        import os.path
        return os.path.isfile(env)
    from pathlib import Path
    here = Path(__file__).resolve()
    candidate = here.parents[5] / "traffic_sim" / "build" / "traffic_sim"
    return candidate.is_file()


pytestmark = pytest.mark.skipif(
    not binary_available(),
    reason="traffic_sim binary not built; run cmake --build traffic_sim/build first",
)


@pytest.fixture(autouse=True)
def reset_between_tests(client):
    yield
    client.post("/api/reset")


@pytest.fixture(scope="module")
def client():
    from app.main import app
    with TestClient(app) as c:
        yield c


# ---------------------------------------------------------------------------
# State endpoint
# ---------------------------------------------------------------------------

def test_initial_state(client):
    resp = client.get("/api/state")
    assert resp.status_code == 200
    data = resp.json()
    assert data["step_count"] == 0
    assert data["current_phase"] == 0     # PHASE_NS
    assert not data["in_yellow_transition"]
    assert len(data["roads"]) == 4
    for road in data["roads"]:
        for lane_info in road["lanes"].values():
            assert lane_info["queue_length"] == 0


# ---------------------------------------------------------------------------
# Add vehicle
# ---------------------------------------------------------------------------

def test_add_vehicle_increments_queue(client):
    resp = client.post("/api/vehicles", json={
        "vehicle_id": "v1",
        "start_road": 0,   # NORTH
        "end_road":   1,   # SOUTH  -> straight lane
    })
    assert resp.status_code == 201

    state = client.get("/api/state").json()
    north = next(r for r in state["roads"] if r["direction"] == 0)
    assert north["lanes"]["straight"]["queue_length"] == 1


def test_add_vehicle_left_turn(client):
    client.post("/api/vehicles", json={"vehicle_id": "lt1", "start_road": 2, "end_road": 0})
    state = client.get("/api/state").json()
    east = next(r for r in state["roads"] if r["direction"] == 2)
    assert east["lanes"]["left"]["queue_length"] == 1


def test_add_vehicle_right_turn(client):
    client.post("/api/vehicles", json={"vehicle_id": "rt1", "start_road": 0, "end_road": 3})
    state = client.get("/api/state").json()
    north = next(r for r in state["roads"] if r["direction"] == 0)
    assert north["lanes"]["right"]["queue_length"] == 1


def test_uturn_rejected(client):
    resp = client.post("/api/vehicles", json={"vehicle_id": "u1", "start_road": 0, "end_road": 0})
    assert resp.status_code == 422


def test_vehicle_id_whitespace_rejected(client):
    resp = client.post("/api/vehicles", json={"vehicle_id": "bad id", "start_road": 0, "end_road": 1})
    assert resp.status_code == 422


# ---------------------------------------------------------------------------
# Step
# ---------------------------------------------------------------------------

def test_empty_step_returns_no_departures(client):
    resp = client.post("/api/step")
    assert resp.status_code == 200
    data = resp.json()
    assert data["left_vehicles"] == []
    assert data["step_number"] == 1


def test_step_increments_step_count(client):
    client.post("/api/step")
    client.post("/api/step")
    state = client.get("/api/state").json()
    assert state["step_count"] == 2


def test_vehicles_depart_after_step(client):
    client.post("/api/vehicles", json={"vehicle_id": "dep1", "start_road": 0, "end_road": 1})
    client.post("/api/vehicles", json={"vehicle_id": "dep2", "start_road": 1, "end_road": 0})
    # PHASE_NS is active initially, so North+South straight vehicles depart
    result = client.post("/api/step").json()
    assert "dep1" in result["left_vehicles"]
    assert "dep2" in result["left_vehicles"]


def test_queue_decrements_after_departure(client):
    client.post("/api/vehicles", json={"vehicle_id": "q1", "start_road": 0, "end_road": 1})
    before = client.get("/api/state").json()
    north_before = next(r for r in before["roads"] if r["direction"] == 0)
    assert north_before["lanes"]["straight"]["queue_length"] == 1

    client.post("/api/step")
    after = client.get("/api/state").json()
    north_after = next(r for r in after["roads"] if r["direction"] == 0)
    assert north_after["lanes"]["straight"]["queue_length"] == 0


# ---------------------------------------------------------------------------
# Reset
# ---------------------------------------------------------------------------

def test_reset_clears_state(client):
    client.post("/api/vehicles", json={"vehicle_id": "r1", "start_road": 0, "end_road": 1})
    client.post("/api/step")
    client.post("/api/reset")

    state = client.get("/api/state").json()
    assert state["step_count"] == 0
    for road in state["roads"]:
        for lane_info in road["lanes"].values():
            assert lane_info["queue_length"] == 0
