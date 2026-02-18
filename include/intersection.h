#ifndef INTERSECTION_H
#define INTERSECTION_H

/*
 * intersection.h - intersection lifecycle and step execution
 *
 * This module is the glue between roads, traffic lights, and the controller.
 * It owns the simulation loop for a single step:
 *
 *   1. If the current phase has run out of steps, ask the controller for the
 *      next phase and activate it immediately (no separate yellow step).
 *   2. Dequeue the front vehicle from each active green lane.
 *   3. Tick all four traffic lights (for state tracking and display).
 *   4. Decrement phase_steps_remaining and increment step_count.
 *
 * Phase transitions happen at the START of a step (before any vehicle moves),
 * so vehicles added between two step commands are visible to the controller
 * that runs at the beginning of the next step.
 *
 * Yellow is a traffic-light display state only. It does not consume a
 * simulation step and does not block vehicle departure from the new phase.
 */

#include "types.h"

/*
 * Zero-initialise the intersection.
 * Sets phase_steps_remaining to 0 so the first step immediately triggers a
 * controller call to pick the opening phase based on initial queue state.
 */
void intersection_init(Intersection *inter);

/*
 * Add a vehicle entering from 'start' and heading to 'end'.
 * Returns false if:
 *   - the movement is a U-turn (start == end), it'd always be rejected
 *   - the target lane is full (MAX_VEHICLES_PER_LANE reached)
 */
bool intersection_add_vehicle(Intersection *inter,
                               RoadDir       start,
                               RoadDir       end,
                               const char   *id);

/*
 * Execute one simulation step.
 *
 * Vehicles that leave the intersection are written into departed[0..*count-1].
 * 'departed' must have room for at least MAX_DEPARTURES_PER_STEP elements.
 */
void intersection_step(Intersection *inter,
                       Vehicle       departed[MAX_DEPARTURES_PER_STEP],
                       uint8_t      *count);

/* Current light state for a road, useful for display and debugging. */
LightState intersection_light_state(const Intersection *inter, RoadDir road);

/* Total vehicles waiting across all roads and lanes. */
uint8_t intersection_total_waiting(const Intersection *inter);

#endif /* INTERSECTION_H */
