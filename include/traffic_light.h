#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

/*
 * traffic_light.h - per-road traffic light FSM
 *
 * State machine:
 *
 *   set_green(N) / set_green_arrow(N)
 *          │
 *   ┌──────▼──────────────────┐
 *   │  RED                    │◄─────────────────────────────┐
 *   └─────────────────────────┘                              │
 *                                                            │
 *   set_green(N)          set_green_arrow(N)                 │
 *          │                       │                         │
 *   ┌──────▼──────┐       ┌────────▼───────┐                 │
 *   │  GREEN      │       │  GREEN_ARROW   │                 │
 *   │  N steps    │       │  N steps       │                 │
 *   └──────┬──────┘       └────────┬───────┘                 │
 *          │ steps_remaining == 0  │                         │
 *          └──────────┬────────────┘                         │
 *                     │                                      │
 *              ┌──────▼──────┐   steps_remaining == 0        │
 *              │   YELLOW    │──────────────────────────────►┘
 *              │ YELLOW_STEPS│
 *              └─────────────┘
 *
 * Callers must check the light state before deciding whether to let vehicles
 * through. Only GREEN and GREEN_ARROW allow vehicles to pass.
 * tick() is called after the vehicles check, advancing the FSM by one step.
 */

#include "types.h"

/* Initialise to RED with steps_remaining = 0. */
void traffic_light_init(TrafficLight *tl);

/*
 * Transition immediately to GREEN / GREEN_ARROW with the given duration.
 * 'duration' is the number of simulation steps this light will stay green.
 * Should only be called when the light is RED.
 */
void traffic_light_set_green(TrafficLight *tl, uint8_t duration);
void traffic_light_set_green_arrow(TrafficLight *tl, uint8_t duration);

/*
 * Advance the FSM by one simulation step.
 *
 * Transitions:
 *   GREEN / GREEN_ARROW: decrement steps_remaining;
 *                        when it reaches 0 -> YELLOW (steps = YELLOW_STEPS)
 *   YELLOW:              decrement steps_remaining;
 *                        when it reaches 0 -> RED
 *   RED:                 no-op
 */
void traffic_light_tick(TrafficLight *tl);

/* Query helpers */
bool traffic_light_is_green(const TrafficLight *tl);  /* GREEN or GREEN_ARROW */
bool traffic_light_is_yellow(const TrafficLight *tl);
bool traffic_light_is_red(const TrafficLight *tl);

/* Human-readable state string, useful for debugging and the README log. */
const char *traffic_light_state_str(const TrafficLight *tl);

#endif /* TRAFFIC_LIGHT_H */
