#include "controller.h"
#include "road.h"

/* Phase metadata table */
const PhaseInfo PHASE_INFO[PHASE_COUNT] = {
    [PHASE_NS]      = { {ROAD_NORTH, ROAD_SOUTH}, 2, false },
    [PHASE_EW]      = { {ROAD_EAST,  ROAD_WEST},  2, false },
    [PHASE_N_ARROW] = { {ROAD_NORTH, ROAD_NONE},  1, true  },
    [PHASE_S_ARROW] = { {ROAD_SOUTH, ROAD_NONE},  1, true  },
    [PHASE_E_ARROW] = { {ROAD_EAST,  ROAD_NONE},  1, true  },
    [PHASE_W_ARROW] = { {ROAD_WEST,  ROAD_NONE},  1, true  },
};


/*
 * Score contribution of a single lane on a single road.
 * Returns 0 for an empty lane.
 */
static uint32_t score_lane(const Road *road, Lane lane, uint32_t step_count) {
    uint8_t count = road_lane_count(road, lane);
    if (count == 0) {
        return 0;
    }

    Vehicle front;
    uint32_t wait = 0;
    if (road_peek_lane(road, lane, &front)) {
        wait = step_count - front.enqueue_step;
    }

    return (uint32_t)count * (1u + wait);
}

/*
 * Total number of vehicles in the lanes that would be served by 'phase'.
 * Used to calculate green duration.
 */
static uint8_t phase_vehicle_count(const Intersection *inter, Phase phase) {
    const PhaseInfo *info = &PHASE_INFO[phase];
    uint8_t total = 0;

    for (int r = 0; r < info->road_count; r++) {
        RoadDir dir       = info->roads[r];
        const Road *road  = &inter->roads[dir];

        if (info->is_arrow) {
            total += road_lane_count(road, LANE_LEFT);
        } else {
            total += road_lane_count(road, LANE_STRAIGHT);
            total += road_lane_count(road, LANE_RIGHT);
        }
    }

    return total;
}


/* Let's name that as API */
uint32_t controller_phase_score(const Intersection *inter, Phase phase) {
    const PhaseInfo *info = &PHASE_INFO[phase];
    uint32_t score = 0;

    for (int r = 0; r < info->road_count; r++) {
        RoadDir dir      = info->roads[r];
        const Road *road = &inter->roads[dir];

        if (info->is_arrow) {
            score += score_lane(road, LANE_LEFT, inter->step_count);
        } else {
            score += score_lane(road, LANE_STRAIGHT, inter->step_count);
            score += score_lane(road, LANE_RIGHT,    inter->step_count);
        }
    }

    return score;
}

PhaseDecision controller_next_phase(const Intersection *inter) {
    Phase    best_phase = inter->current_phase;
    uint32_t best_score = controller_phase_score(inter, inter->current_phase);

    for (int p = 0; p < PHASE_COUNT; p++) {
        uint32_t score = controller_phase_score(inter, (Phase)p);

        /*
         * Strict greater-than: the current phase wins ties, avoiding
         * pointless switching when the intersection is evenly loaded.
         */
        if (score > best_score) {
            best_score = score;
            best_phase = (Phase)p;
        }
    }

    uint8_t count    = phase_vehicle_count(inter, best_phase);
    uint8_t duration = count < MIN_GREEN_STEPS ? MIN_GREEN_STEPS
                     : count > MAX_GREEN_STEPS ? MAX_GREEN_STEPS
                     : count;

    PhaseDecision decision = { best_phase, duration };
    return decision;
}
