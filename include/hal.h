#ifndef HAL_H
#define HAL_H

/*
 * hal.h - hardware abstraction layer for the embedded traffic controller
 *
 * This interface is the only thing that changes between targets.
 * The entire simulation core (road, traffic_light, controller, intersection,
 * simulation) compiles and runs identically on desktop and on STM32.
 *
 * Implementations:
 *   src/hal_stm32.c  - GPIO inputs (inductive loops / IR sensors) and
 *                      GPIO outputs (traffic light driver pins)
 *
 * To port to a new target, provide a struct that fills both function pointers
 * and pass it to simulation_tick().
 */

#include <stdbool.h>
#include "types.h"

typedef struct {
    /*
     * sense_lane - read one vehicle-detection sensor.
     *
     * Returns true if a vehicle is physically present in the given lane.
     * Called every tick for all 12 lanes (ROAD_COUNT × LANES_PER_ROAD).
     * The simulation uses edge detection internally, so this may return
     * true for multiple consecutive ticks without double-counting.
     */
    bool (*sense_lane)(RoadDir road, Lane lane);

    /*
     * set_light - drive the physical traffic light for one road.
     *
     * Called every tick for all four roads after intersection_step().
     * The implementation must map LightState → GPIO output pattern
     * (e.g. turn the RED pin on and the GREEN/YELLOW/ARROW pins off).
     */
    void (*set_light)(RoadDir road, LightState state);
} EmbeddedHAL;

#endif /* HAL_H */
