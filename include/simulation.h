#ifndef SIMULATION_H
#define SIMULATION_H

/*
 * simulation.h - platform-neutral embedded simulation loop
 *
 * Sits between the HAL and the intersection engine.
 * The caller (a timer ISR or RTOS task) calls simulation_tick() at a fixed
 * interval; everything else is handled internally.
 *
 * Typical setup on STM32:
 *
 *   SimulationContext ctx;
 *   simulation_init(&ctx);
 *   HAL_TIM_Base_Start_IT(&htim2);   // start the step timer
 *
 *   // in HAL_TIM_PeriodElapsedCallback:
 *   if (htim->Instance == TIM2) simulation_tick(&ctx, &HAL_STM32);
 */

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "hal.h"

/*
 * SimulationContext - all mutable state for one intersection instance.
 *
 * Intended to be a single static or global on embedded targets.
 * Contains the intersection state, per-lane edge-detection flags,
 * and a monotonic vehicle ID counter.
 */
typedef struct {
    Intersection inter;
    bool         prev_sense[ROAD_COUNT][LANES_PER_ROAD];
    uint32_t     vehicle_counter;
} SimulationContext;

/* Zero-initialise ctx and call intersection_init(). */
void simulation_init(SimulationContext *ctx);

/*
 * Advance the simulation by one step.
 *
 * Must be called at a fixed real-time interval (e.g. every 2 seconds).
 * Steps:
 *   1. Poll all lane sensors; enqueue one vehicle per rising edge.
 *   2. Call intersection_step() to advance the controller and lights.
 *   3. Call hal->set_light() for every road to update physical outputs.
 */
void simulation_tick(SimulationContext *ctx, const EmbeddedHAL *hal);

#endif /* SIMULATION_H */
