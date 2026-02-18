#include <string.h>
#include <stdio.h>
#include "traffic_light.h"
#include "test_helpers.h"


/* Tick the light n times. */
static void tick_n(TrafficLight *tl, int n) {
    for (int i = 0; i < n; i++) {
        traffic_light_tick(tl);
    }
}

/* Initialize a traffic light. */
static void test_init_is_red(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    ASSERT(traffic_light_is_red(&tl));
    ASSERT(!traffic_light_is_green(&tl));
    ASSERT(!traffic_light_is_yellow(&tl));
    ASSERT_EQ(tl.steps_remaining, 0);
}

static void test_set_green_transitions_from_red(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green(&tl, 3);
    ASSERT(traffic_light_is_green(&tl));
    ASSERT_EQ(tl.state, LIGHT_GREEN);
    ASSERT_EQ(tl.steps_remaining, 3);
}

static void test_set_green_arrow_transitions_from_red(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green_arrow(&tl, 2);
    ASSERT(traffic_light_is_green(&tl));
    ASSERT_EQ(tl.state, LIGHT_GREEN_ARROW);
    ASSERT_EQ(tl.steps_remaining, 2);
}

/* Full tick cycle. */
static void test_green_stays_green_for_exact_duration(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green(&tl, 3);

    /* Should be green before each of the 3 ticks. */
    for (int i = 0; i < 3; i++) {
        ASSERT(traffic_light_is_green(&tl));
        traffic_light_tick(&tl);
    }
    /* After 3 ticks the light must have left the green state. */
    ASSERT(!traffic_light_is_green(&tl));
}

static void test_green_transitions_to_yellow_after_duration(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green(&tl, 2);

    tick_n(&tl, 2);

    ASSERT(traffic_light_is_yellow(&tl));
    ASSERT_EQ(tl.steps_remaining, YELLOW_STEPS);
}

static void test_yellow_transitions_to_red_after_yellow_steps(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green(&tl, 1);

    tick_n(&tl, 1);                  /* GREEN -> YELLOW */
    ASSERT(traffic_light_is_yellow(&tl));

    tick_n(&tl, YELLOW_STEPS);       /* YELLOW -> RED */
    ASSERT(traffic_light_is_red(&tl));
    ASSERT_EQ(tl.steps_remaining, 0);
}

static void test_full_cycle_green(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green(&tl, MIN_GREEN_STEPS);

    tick_n(&tl, MIN_GREEN_STEPS);
    ASSERT(traffic_light_is_yellow(&tl));

    tick_n(&tl, YELLOW_STEPS);
    ASSERT(traffic_light_is_red(&tl));
}


/* A similar scenario, but with a "green arrow" simulation. */
static void test_full_cycle_green_arrow(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    traffic_light_set_green_arrow(&tl, 2);

    tick_n(&tl, 2);
    ASSERT(traffic_light_is_yellow(&tl));

    tick_n(&tl, YELLOW_STEPS);
    ASSERT(traffic_light_is_red(&tl));
}

/* -------------------------------------------------------------------------
 * Tick on RED is a no-op
 * ---------------------------------------------------------------------- */

static void test_tick_on_red_is_noop(void) {
    TrafficLight tl;
    traffic_light_init(&tl);

    tick_n(&tl, 10);

    ASSERT(traffic_light_is_red(&tl));
    ASSERT_EQ(tl.steps_remaining, 0);
}


/* Duration accuracy: light is green for exactly N steps, not N-1 or N+1. */
static void test_duration_accuracy(void) {
    for (uint8_t dur = 1; dur <= MAX_GREEN_STEPS; dur++) {
        TrafficLight tl;
        traffic_light_init(&tl);
        traffic_light_set_green(&tl, dur);

        int green_steps = 0;
        /* Run until RED, counting how many steps the light was green. */
        while (!traffic_light_is_red(&tl)) {
            if (traffic_light_is_green(&tl)) green_steps++;
            traffic_light_tick(&tl);
        }
        ASSERT_EQ((uint8_t)green_steps, dur);
    }
}

static void test_state_str(void) {
    TrafficLight tl;
    traffic_light_init(&tl);
    ASSERT_STR_EQ(traffic_light_state_str(&tl), "RED");

    traffic_light_set_green(&tl, 2);
    ASSERT_STR_EQ(traffic_light_state_str(&tl), "GREEN");

    tick_n(&tl, 2);
    ASSERT_STR_EQ(traffic_light_state_str(&tl), "YELLOW");

    tick_n(&tl, YELLOW_STEPS);
    ASSERT_STR_EQ(traffic_light_state_str(&tl), "RED");

    traffic_light_set_green_arrow(&tl, 1);
    ASSERT_STR_EQ(traffic_light_state_str(&tl), "GREEN_ARROW");
}

int main(void) {
    RUN_TEST(test_init_is_red);
    RUN_TEST(test_set_green_transitions_from_red);
    RUN_TEST(test_set_green_arrow_transitions_from_red);
    RUN_TEST(test_green_stays_green_for_exact_duration);
    RUN_TEST(test_green_transitions_to_yellow_after_duration);
    RUN_TEST(test_yellow_transitions_to_red_after_yellow_steps);
    RUN_TEST(test_full_cycle_green);
    RUN_TEST(test_full_cycle_green_arrow);
    RUN_TEST(test_tick_on_red_is_noop);
    RUN_TEST(test_duration_accuracy);
    RUN_TEST(test_state_str);
    PRINT_RESULTS();
}
