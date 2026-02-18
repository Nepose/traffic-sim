#ifndef CONTROLLER_H
#define CONTROLLER_H

/*
 * controller.h - adaptive traffic light scheduling algorithm
 *
 * The controller is a pure function of the intersection state: it reads
 * queue lengths and vehicle wait times, then decides which phase to run
 * next and for how many steps. It never modifies any state itself.
 *
 * Scoring
 * -------
 * Each phase is assigned a priority score:
 *
 *   score(phase) = sum over every active (road, lane):
 *       queue_count * (1 + steps_waited_by_front_vehicle)
 *
 * The wait-time multiplier prevents starvation: a road that has been
 * skipped for many steps accumulates a growing score even with few vehicles.
 *
 * Duration
 * --------
 * The selected phase runs for:
 *
 *   clamp(total_vehicles_in_active_lanes, MIN_GREEN_STEPS, MAX_GREEN_STEPS)
 *
 * This gives roughly one step per waiting vehicle, bounded to avoid both
 * micro-oscillation (too short) and monopolisation (too long).
 *
 * Tie-breaking
 * ------------
 * If two phases share the highest score, the current phase wins. This avoids
 * unnecessary switching when the intersection is balanced.
 */

#include "types.h"

/*
 * Phase metadata
 *
 * Exposed publicly so the simulation can apply the phase decision
 * (activate the correct lights) without duplicating this table.
 */

typedef struct {
    RoadDir roads[MAX_ROADS_PER_PHASE];
    uint8_t road_count;
    bool    is_arrow; /* false -> STRAIGHT + RIGHT lanes active
                         true  -> LEFT lane only                */
} PhaseInfo;

extern const PhaseInfo PHASE_INFO[PHASE_COUNT];

/*
 * Controller API
 */

typedef struct {
    Phase   phase;
    uint8_t duration; /* number of green steps for the chosen phase */
} PhaseDecision;

/*
 * Select the next phase and calculate its duration.
 * Reads intersection state, returns a decision.
 */
PhaseDecision controller_next_phase(const Intersection *inter);

/*
 * Calculate the priority score for a single phase.
 * Exposed for unit testing and for README documentation of the algorithm.
 */
uint32_t controller_phase_score(const Intersection *inter, Phase phase);

#endif /* CONTROLLER_H */
