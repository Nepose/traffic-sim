#include "intersection.h"
#include "controller.h"
#include "road.h"
#include "traffic_light.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

/*
 * Apply a phase decision: activate the relevant lights and update state.
 * Called both at init time and whenever phase_steps_remaining hits 0.
 */
static void apply_phase(Intersection *inter, PhaseDecision decision) {
    const PhaseInfo *info = &PHASE_INFO[decision.phase];

    for (int r = 0; r < info->road_count; r++) {
        RoadDir dir = info->roads[r];
        if (info->is_arrow) {
            traffic_light_set_green_arrow(&inter->lights[dir], decision.duration);
        } else {
            traffic_light_set_green(&inter->lights[dir], decision.duration);
        }
    }

    inter->current_phase         = decision.phase;
    inter->phase_steps_remaining = decision.duration;
    inter->in_yellow_transition  = false;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

void intersection_init(Intersection *inter) {
    for (int i = 0; i < ROAD_COUNT; i++) {
        road_init(&inter->roads[i]);
        traffic_light_init(&inter->lights[i]);
    }
    inter->current_phase         = PHASE_NS;
    inter->phase_steps_remaining = 0;
    inter->in_yellow_transition  = false;
    inter->step_count            = 0;
}

bool intersection_add_vehicle(Intersection *inter,
                               RoadDir       start,
                               RoadDir       end,
                               const char   *id) {
    MovementType mv = movement_type(start, end);
    if (mv == MOVE_INVALID) {
        return false;
    }

    Vehicle v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, id, MAX_VEHICLE_ID_LEN - 1);
    v.end_road     = end;
    v.movement     = mv;
    v.enqueue_step = inter->step_count;

    return road_enqueue(&inter->roads[start], &v);
}

bool intersection_add_vehicle_by_lane(Intersection *inter,
                                      RoadDir       road,
                                      Lane          lane,
                                      const char   *id) {
    if (road >= ROAD_COUNT || lane >= LANES_PER_ROAD) {
        return false;
    }

    /* Map lane index back to a canonical movement type.
     * The destination road is unknown from a sensor reading, so end_road
     * is set to ROAD_NONE. The controller and departure logic never read
     * end_road, so this is safe. */
    static const MovementType lane_to_movement[LANES_PER_ROAD] = {
        [LANE_LEFT]     = MOVE_LEFT,
        [LANE_STRAIGHT] = MOVE_STRAIGHT,
        [LANE_RIGHT]    = MOVE_RIGHT,
    };

    Vehicle v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, id, MAX_VEHICLE_ID_LEN - 1);
    v.end_road     = ROAD_NONE;
    v.movement     = lane_to_movement[lane];
    v.enqueue_step = inter->step_count;

    return queue_enqueue(&inter->roads[road].lanes[lane], &v);
}

void intersection_step(Intersection *inter,
                       Vehicle       departed[MAX_DEPARTURES_PER_STEP],
                       uint8_t      *count) {
    *count = 0;

    /* 1. If current phase is exhausted, pick the next one. */
    if (inter->phase_steps_remaining == 0) {
        PhaseDecision decision = controller_next_phase(inter);
        apply_phase(inter, decision);
    }

    /* 2. Dequeue the front vehicle from each active green lane. */
    const PhaseInfo *info = &PHASE_INFO[inter->current_phase];
    for (int r = 0; r < info->road_count; r++) {
        RoadDir dir = info->roads[r];
        if (!traffic_light_is_green(&inter->lights[dir])) {
            continue;
        }

        Vehicle v;
        if (info->is_arrow) {
            if (road_dequeue_lane(&inter->roads[dir], LANE_LEFT, &v)) {
                departed[(*count)++] = v;
            }
        } else {
            if (road_dequeue_lane(&inter->roads[dir], LANE_STRAIGHT, &v)) {
                departed[(*count)++] = v;
            }
            if (road_dequeue_lane(&inter->roads[dir], LANE_RIGHT, &v)) {
                departed[(*count)++] = v;
            }
        }
    }

    /* 3. Tick all traffic lights. */
    for (int i = 0; i < ROAD_COUNT; i++) {
        traffic_light_tick(&inter->lights[i]);
    }

    /* 4. Advance counters. */
    if (inter->phase_steps_remaining > 0) {
        inter->phase_steps_remaining--;
    }
    inter->step_count++;
}

LightState intersection_light_state(const Intersection *inter, RoadDir road) {
    return inter->lights[road].state;
}

uint8_t intersection_total_waiting(const Intersection *inter) {
    uint8_t total = 0;
    for (int i = 0; i < ROAD_COUNT; i++) {
        total += road_total_count(&inter->roads[i]);
    }
    return total;
}
