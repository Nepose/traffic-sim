#include <string.h>
#include <stdio.h>
#include "controller.h"
#include "road.h"
#include "test_helpers.h"

/* 
 * Helpers
 */

static Intersection make_empty_intersection(void) {
    Intersection inter;
    memset(&inter, 0, sizeof(inter));
    for (int i = 0; i < ROAD_COUNT; i++) {
        road_init(&inter.roads[i]);
    }
    inter.current_phase = PHASE_NS;
    inter.step_count    = 0;
    return inter;
}

static void add_vehicle(Intersection *inter, RoadDir start, RoadDir end,
                        const char *id) {
    Vehicle v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, id, MAX_VEHICLE_ID_LEN - 1);
    v.end_road     = end;
    v.movement     = movement_type(start, end);
    v.enqueue_step = inter->step_count;
    road_enqueue(&inter->roads[start], &v);
}

/*
 * PHASE_INFO sanity checks
 */

static void test_phase_info_road_counts(void) {
    ASSERT_EQ(PHASE_INFO[PHASE_NS].road_count,      2);
    ASSERT_EQ(PHASE_INFO[PHASE_EW].road_count,      2);
    ASSERT_EQ(PHASE_INFO[PHASE_NS_ARROW].road_count, 2);
    ASSERT_EQ(PHASE_INFO[PHASE_EW_ARROW].road_count, 2);
}

static void test_phase_info_arrow_flag(void) {
    ASSERT(!PHASE_INFO[PHASE_NS].is_arrow);
    ASSERT(!PHASE_INFO[PHASE_EW].is_arrow);
    ASSERT(PHASE_INFO[PHASE_NS_ARROW].is_arrow);
    ASSERT(PHASE_INFO[PHASE_EW_ARROW].is_arrow);
}

/*
 * controller_phase_score
 */

static void test_score_empty_intersection_is_zero(void) {
    Intersection inter = make_empty_intersection();
    for (int p = 0; p < PHASE_COUNT; p++) {
        ASSERT_EQ(controller_phase_score(&inter, (Phase)p), 0u);
    }
}

static void test_score_counts_straight_and_right_for_main_phase(void) {
    Intersection inter = make_empty_intersection();

    /* One straight vehicle on North: should contribute to PHASE_NS score. */
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1"); /* straight */
    ASSERT(controller_phase_score(&inter, PHASE_NS) > 0);
    ASSERT_EQ(controller_phase_score(&inter, PHASE_EW), 0u);

    /* One right-turn vehicle on North: also counted in PHASE_NS. */
    add_vehicle(&inter, ROAD_NORTH, ROAD_WEST, "v2"); /* right */
    uint32_t ns_score = controller_phase_score(&inter, PHASE_NS);
    ASSERT(ns_score > 1); /* two vehicles -> higher score */
}

static void test_score_counts_left_only_for_arrow_phase(void) {
    Intersection inter = make_empty_intersection();

    /* Left-turn vehicle on North: visible to N_ARROW but not to PHASE_NS. */
    add_vehicle(&inter, ROAD_NORTH, ROAD_EAST, "v1"); /* left */

    ASSERT(controller_phase_score(&inter, PHASE_NS_ARROW) > 0);
    ASSERT_EQ(controller_phase_score(&inter, PHASE_NS), 0u);
    ASSERT_EQ(controller_phase_score(&inter, PHASE_EW_ARROW), 0u);
}

static void test_score_increases_with_wait_time(void) {
    Intersection inter = make_empty_intersection();
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1");

    uint32_t score_at_0  = controller_phase_score(&inter, PHASE_NS);

    /* Simulate 5 steps passing without serving this vehicle. */
    inter.step_count = 5;
    uint32_t score_at_5  = controller_phase_score(&inter, PHASE_NS);

    ASSERT(score_at_5 > score_at_0);
}

/*
 * controller_next_phase — phase selection
 */

static void test_selects_busiest_phase(void) {
    Intersection inter = make_empty_intersection();

    /* Load EW heavily, NS lightly. */
    add_vehicle(&inter, ROAD_EAST,  ROAD_WEST,  "e1");
    add_vehicle(&inter, ROAD_EAST,  ROAD_WEST,  "e2");
    add_vehicle(&inter, ROAD_EAST,  ROAD_WEST,  "e3");
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "n1");

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase, PHASE_EW);
}

static void test_selects_arrow_phase_when_only_left_turns(void) {
    Intersection inter = make_empty_intersection();

    add_vehicle(&inter, ROAD_SOUTH, ROAD_WEST, "s1"); /* left turn from south */

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase, PHASE_NS_ARROW);
}

static void test_tie_keeps_current_phase(void) {
    Intersection inter = make_empty_intersection();
    inter.current_phase = PHASE_EW;

    /* One vehicle on each axis — tied score. Current phase (EW) should win. */
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "n1");
    add_vehicle(&inter, ROAD_EAST,  ROAD_WEST,  "e1");

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase, PHASE_EW);
}

static void test_empty_intersection_returns_min_duration(void) {
    Intersection inter = make_empty_intersection();
    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.duration, MIN_GREEN_STEPS);
}

/*
 * controller_next_phase — duration calculation
 */

static void test_duration_clamped_to_min(void) {
    Intersection inter = make_empty_intersection();

    /* One vehicle: vehicle count (1) < MIN_GREEN_STEPS (2) -> clamp to min. */
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1");

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase,    PHASE_NS);
    ASSERT_EQ(d.duration, MIN_GREEN_STEPS);
}

static void test_duration_proportional_to_queue(void) {
    Intersection inter = make_empty_intersection();

    /* Fill NS with exactly MAX_GREEN_STEPS vehicles. */
    for (int i = 0; i < MAX_GREEN_STEPS; i++) {
        char id[8];
        snprintf(id, sizeof(id), "v%d", i);
        add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, id);
    }

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase,    PHASE_NS);
    ASSERT_EQ(d.duration, MAX_GREEN_STEPS);
}

static void test_duration_clamped_to_max(void) {
    Intersection inter = make_empty_intersection();

    /* Overfill: more vehicles than MAX_GREEN_STEPS. */
    for (int i = 0; i < MAX_GREEN_STEPS + 5; i++) {
        char id[8];
        snprintf(id, sizeof(id), "v%d", i);
        add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, id);
    }

    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.duration, MAX_GREEN_STEPS);
}

/*
 * Starvation prevention: wait time shifts selection
 */

static void test_starvation_prevention(void) {
    Intersection inter = make_empty_intersection();
    inter.current_phase = PHASE_NS;

    /* EW vehicle arrives first and has been waiting since step 0. */
    add_vehicle(&inter, ROAD_EAST, ROAD_WEST, "e1"); /* enqueue_step = 0 */

    /* Time passes. NS vehicles arrive freshly at step 20. */
    inter.step_count = 20;
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "n1"); /* enqueue_step = 20 */
    add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "n2"); /* enqueue_step = 20 */

    /* NS: 2 vehicles, wait=0  -> score = 2*(1+0)  = 2
       EW: 1 vehicle,  wait=20 -> score = 1*(1+20) = 21
       EW wins despite having fewer vehicles. */
    PhaseDecision d = controller_next_phase(&inter);
    ASSERT_EQ(d.phase, PHASE_EW);
}

/*
 * Entry point
 */

int main(void) {
    RUN_TEST(test_phase_info_road_counts);
    RUN_TEST(test_phase_info_arrow_flag);
    RUN_TEST(test_score_empty_intersection_is_zero);
    RUN_TEST(test_score_counts_straight_and_right_for_main_phase);
    RUN_TEST(test_score_counts_left_only_for_arrow_phase);
    RUN_TEST(test_score_increases_with_wait_time);
    RUN_TEST(test_selects_busiest_phase);
    RUN_TEST(test_selects_arrow_phase_when_only_left_turns);
    RUN_TEST(test_tie_keeps_current_phase);
    RUN_TEST(test_empty_intersection_returns_min_duration);
    RUN_TEST(test_duration_clamped_to_min);
    RUN_TEST(test_duration_proportional_to_queue);
    RUN_TEST(test_duration_clamped_to_max);
    RUN_TEST(test_starvation_prevention);
    PRINT_RESULTS();
}
