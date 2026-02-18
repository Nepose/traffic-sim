#ifndef TYPES_H
#define TYPES_H

/*
 * types.h - shared domain types
 *
 * All enums and structs used across modules are defined here.
 */

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// Roads
typedef enum {
    ROAD_NORTH = 0,
    ROAD_SOUTH = 1,
    ROAD_EAST  = 2,
    ROAD_WEST  = 3,
    ROAD_NONE  = 4   /* sentinel, used in phase tables for single-road phases */
} RoadDir;

// Movement types, derived from (startRoad, endRoad) once at addVehicle time.
// U-turns (startRoad == endRoad) are rejected as MOVE_INVALID.
typedef enum {
    MOVE_STRAIGHT = 0,
    MOVE_RIGHT    = 1,
    MOVE_LEFT     = 2,
    MOVE_INVALID  = 3
} MovementType;

/*
 * Lane index within a road.
 *
 *   LANE_LEFT     (0), left-turn only;         active during arrow phases
 *   LANE_STRAIGHT (1), straight only;          active during NS/EW phases
 *   LANE_RIGHT    (2), straight + right turn;  active during NS/EW phases
 *
 * Right turns share the main phase because they don't cross any opposing path.
 */
typedef enum {
    LANE_LEFT     = 0,
    LANE_STRAIGHT = 1,
    LANE_RIGHT    = 2
} Lane;

// Vehicles
typedef struct {
    char         id[MAX_VEHICLE_ID_LEN];
    RoadDir      end_road;
    MovementType movement;     /* derived once at enqueue, never changes  */
    uint32_t     enqueue_step; /* simulation step at which vehicle was added */
} Vehicle;

/*
 * Fixed-size ring-buffer queue for one lane. All storage is inline.
 *
 *   head  - index of the next vehicle to dequeue
 *   tail  - index where the next vehicle will be enqueued
 *   count - number of vehicles currently in the queue
 */
typedef struct {
    Vehicle buf[MAX_VEHICLES_PER_LANE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} VehicleQueue;

// One road = three lane queues, indexed by Lane enum.
typedef struct {
    VehicleQueue lanes[LANES_PER_ROAD];
} Road;


// Traffic lights
typedef enum {
    LIGHT_RED          = 0,
    LIGHT_YELLOW       = 1,
    LIGHT_GREEN        = 2,
    LIGHT_GREEN_ARROW  = 3   /* protected turn phase */
} LightState;

typedef struct {
    LightState state;
    uint8_t    steps_remaining;   /* steps left in the current state */
} TrafficLight;

/*
 * Phases
 *
 * PHASE_NS / PHASE_EW  -> LANE_STRAIGHT + LANE_RIGHT green on both axis roads
 * PHASE_*_ARROW        -> LANE_LEFT green on a single road only
 *
 */
typedef enum {
    PHASE_NS      = 0,   /* North + South: straight & right lanes */
    PHASE_EW      = 1,   /* East  + West:  straight & right lanes */
    PHASE_N_ARROW = 2,   /* North only: left-turn lane             */
    PHASE_S_ARROW = 3,   /* South only: left-turn lane             */
    PHASE_E_ARROW = 4,   /* East  only: left-turn lane             */
    PHASE_W_ARROW = 5    /* West  only: left-turn lane             */
} Phase;

/*
 * Intersection 
 *
 * A complete, self-contained simulation state.
 */
typedef struct {
    Road         roads[ROAD_COUNT];
    TrafficLight lights[ROAD_COUNT];
    Phase        current_phase;
    uint8_t      phase_steps_remaining;
    bool         in_yellow_transition;
    uint32_t     step_count;
} Intersection;

#endif /* TYPES_H */
