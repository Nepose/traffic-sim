#ifndef CONFIG_H
#define CONFIG_H

/*
 * config.h - compile-time configuration
 *
 * Defined as a set of constants so that each setting can be changed
 * if necessary to recompile for a different embedded target
 *
 * After all, embedded portability <=> no heavy heaps :)
 */

/* Number of lanes per road (left-turn | straight | straight+right). */
#define LANES_PER_ROAD          3

/* Maximum number of vehicles that can queue on a single lane. */
#define MAX_VEHICLES_PER_LANE   64

/* Maximum length of a vehicle ID string, including the null terminator. */
#define MAX_VEHICLE_ID_LEN      32

/* Number of roads at the intersection (fixed: N, S, E, W). */
#define ROAD_COUNT              4

/* Number of distinct traffic-light phases. */
#define PHASE_COUNT             6

/* Maximum number of roads active (green) within a single phase. */
#define MAX_ROADS_PER_PHASE     2

/*
 * Adaptive controller tuning
 *
 * The controller will keep a phase green for at least MIN_GREEN_STEPS and
 * at most MAX_GREEN_STEPS simulation steps before reconsidering.
 */
#define MIN_GREEN_STEPS         2
#define MAX_GREEN_STEPS         8

/* Number of steps a light stays yellow before turning red. */
#define YELLOW_STEPS            1

/* Maximum vehicles that can depart in a single step.
   Main phase: MAX_ROADS_PER_PHASE roads x 2 active lanes (straight + right). */
#define MAX_DEPARTURES_PER_STEP (MAX_ROADS_PER_PHASE * 2)

#endif /* CONFIG_H */
