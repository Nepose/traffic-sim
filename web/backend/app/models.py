"""Pydantic models for the traffic simulation REST API."""

from __future__ import annotations

from enum import IntEnum
from typing import Annotated

from pydantic import BaseModel, Field, field_validator

# ---------------------------------------------------------------------------
# Domain enums (mirror types.h)
# ---------------------------------------------------------------------------

class RoadDir(IntEnum):
    NORTH = 0
    SOUTH = 1
    EAST  = 2
    WEST  = 3


class Lane(IntEnum):
    LEFT     = 0
    STRAIGHT = 1
    RIGHT    = 2


class LightState(IntEnum):
    RED          = 0
    YELLOW       = 1
    GREEN        = 2
    GREEN_ARROW  = 3


class Phase(IntEnum):
    NS       = 0
    EW       = 1
    NS_ARROW = 2
    EW_ARROW = 3


# ---------------------------------------------------------------------------
# API request / response models
# ---------------------------------------------------------------------------

class AddVehicleRequest(BaseModel):
    vehicle_id: Annotated[str, Field(min_length=1, max_length=31)]
    start_road: RoadDir
    end_road: RoadDir

    @field_validator("vehicle_id")
    @classmethod
    def no_whitespace(cls, v: str) -> str:
        if any(c.isspace() for c in v):
            raise ValueError("vehicle_id must not contain whitespace")
        return v


class StepResponse(BaseModel):
    left_vehicles: list[str]
    step_number: int


class LightInfo(BaseModel):
    state: LightState
    steps_remaining: int


class LaneInfo(BaseModel):
    queue_length: int


class RoadInfo(BaseModel):
    direction: RoadDir
    light: LightInfo
    lanes: dict[str, LaneInfo]   # "left" | "straight" | "right"


class IntersectionState(BaseModel):
    step_count: int
    current_phase: Phase
    in_yellow_transition: bool
    roads: list[RoadInfo]


class ResetResponse(BaseModel):
    ok: bool
