#include <string.h>
#include <stdio.h>
#include "intersection.h"
#include "road.h"
#include "test_helpers.h"

/* 
 * Helpers 
 */

/* Returns true if 'id' is present anywhere in departed[0..count-1]. */
static bool departed_contains(const Vehicle *departed, uint8_t count,
                               const char *id) {
    for (uint8_t i = 0; i < count; i++) {
        if (strcmp(departed[i].id, id) == 0) return true;
    }
    return false;
}

static Vehicle dep[MAX_DEPARTURES_PER_STEP];
static uint8_t dep_count;

static void step(Intersection *inter) {
    intersection_step(inter, dep, &dep_count);
}

/* Init tests. */
static void test_init_empty(void) {
    Intersection inter;
    intersection_init(&inter);

    ASSERT_EQ(intersection_total_waiting(&inter), 0);
    ASSERT_EQ(inter.step_count, 0);

    for (int i = 0; i < ROAD_COUNT; i++) {
        ASSERT_EQ(intersection_light_state(&inter, (RoadDir)i), LIGHT_RED);
    }
}

/* Intersection, add vehicle. */
static void test_add_vehicle_valid(void) {
    Intersection inter;
    intersection_init(&inter);

    ASSERT(intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1"));
    ASSERT_EQ(intersection_total_waiting(&inter), 1);
}

static void test_add_vehicle_rejects_uturn(void) {
    Intersection inter;
    intersection_init(&inter);

    ASSERT(!intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_NORTH, "v1"));
    ASSERT_EQ(intersection_total_waiting(&inter), 0);
}

static void test_add_vehicle_routes_to_correct_lane(void) {
    Intersection inter;
    intersection_init(&inter);

    /* North -> South: straight -> LANE_STRAIGHT */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "straight");
    /* North -> West:  right    -> LANE_RIGHT    */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_WEST,  "right");
    /* North -> East:  left     -> LANE_LEFT     */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_EAST,  "left");

    ASSERT_EQ(road_lane_count(&inter.roads[ROAD_NORTH], LANE_STRAIGHT), 1);
    ASSERT_EQ(road_lane_count(&inter.roads[ROAD_NORTH], LANE_RIGHT),    1);
    ASSERT_EQ(road_lane_count(&inter.roads[ROAD_NORTH], LANE_LEFT),     1);
}

/* 
 * Spec example — the canonical end-to-end test
 *
 *   addVehicle v1 south north
 *   addVehicle v2 north south
 *   step  ->  leftVehicles: [v1, v2]
 *   step  ->  leftVehicles: []
 *   addVehicle v3 west south
 *   addVehicle v4 west south
 *   step  ->  leftVehicles: [v3]
 *   step  ->  leftVehicles: [v4]
 */

static void test_spec_example(void) {
    Intersection inter;
    intersection_init(&inter);

    intersection_add_vehicle(&inter, ROAD_SOUTH, ROAD_NORTH, "vehicle1");
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "vehicle2");

    /* Step 1: both NS vehicles depart. */
    step(&inter);
    ASSERT_EQ(dep_count, 2);
    ASSERT(departed_contains(dep, dep_count, "vehicle1"));
    ASSERT(departed_contains(dep, dep_count, "vehicle2"));

    /* Step 2: NS still green but queues empty. */
    step(&inter);
    ASSERT_EQ(dep_count, 0);

    intersection_add_vehicle(&inter, ROAD_WEST, ROAD_SOUTH, "vehicle3");
    intersection_add_vehicle(&inter, ROAD_WEST, ROAD_SOUTH, "vehicle4");

    /* Step 3: controller switches to EW, vehicle3 departs. */
    step(&inter);
    ASSERT_EQ(dep_count, 1);
    ASSERT(departed_contains(dep, dep_count, "vehicle3"));

    /* Step 4: EW still green, vehicle4 departs. */
    step(&inter);
    ASSERT_EQ(dep_count, 1);
    ASSERT(departed_contains(dep, dep_count, "vehicle4"));
}

/* Phase behaviour. */
static void test_first_step_activates_phase(void) {
    Intersection inter;
    intersection_init(&inter);

    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1");

    /* Before first step, all lights are RED. */
    ASSERT_EQ(intersection_light_state(&inter, ROAD_NORTH), LIGHT_RED);

    step(&inter);

    /* After first step the NS phase should have been active (v1 departed). */
    ASSERT(departed_contains(dep, dep_count, "v1"));
}

static void test_phase_persists_for_min_steps(void) {
    Intersection inter;
    intersection_init(&inter);

    /* One NS vehicle: duration = clamp(1, MIN, MAX) = MIN_GREEN_STEPS. */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1");

    step(&inter); /* step 1: v1 departs, one green step consumed */
    ASSERT_EQ(dep_count, 1);
    /* Light still GREEN after step 1 — duration=MIN (2), only 1 step used. */
    ASSERT_EQ(intersection_light_state(&inter, ROAD_NORTH), LIGHT_GREEN);

    step(&inter); /* step 2: NS still green, queue empty, nothing departs */
    ASSERT_EQ(dep_count, 0);
    /* After step 2's tick the light transitions to YELLOW (steps exhausted). */
    ASSERT_EQ(intersection_light_state(&inter, ROAD_NORTH), LIGHT_YELLOW);
}

static void test_phase_switches_to_busier_road(void) {
    Intersection inter;
    intersection_init(&inter);

    /* Load EW much heavier than NS. */
    intersection_add_vehicle(&inter, ROAD_EAST, ROAD_WEST, "e1");
    intersection_add_vehicle(&inter, ROAD_EAST, ROAD_WEST, "e2");
    intersection_add_vehicle(&inter, ROAD_EAST, ROAD_WEST, "e3");
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "n1");

    /* Run enough steps to exhaust the NS phase and let EW activate. */
    bool ew_activated = false;
    for (int i = 0; i < 20 && !ew_activated; i++) {
        step(&inter);
        if (departed_contains(dep, dep_count, "e1")) {
            ew_activated = true;
        }
    }
    ASSERT(ew_activated);
}

static void test_arrow_phase_serves_left_turns(void) {
    Intersection inter;
    intersection_init(&inter);

    /* Only left-turn vehicle on North: N->E */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_EAST, "left1");

    bool left_departed = false;
    for (int i = 0; i < 20 && !left_departed; i++) {
        step(&inter);
        if (departed_contains(dep, dep_count, "left1")) {
            left_departed = true;
        }
    }
    ASSERT(left_departed);
}

static void test_step_count_increments(void) {
    Intersection inter;
    intersection_init(&inter);

    ASSERT_EQ(inter.step_count, 0);
    step(&inter);
    ASSERT_EQ(inter.step_count, 1);
    step(&inter);
    ASSERT_EQ(inter.step_count, 2);
}

static void test_vehicle_count_decreases_after_departure(void) {
    Intersection inter;
    intersection_init(&inter);

    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "v1");
    ASSERT_EQ(intersection_total_waiting(&inter), 1);

    step(&inter);
    ASSERT_EQ(intersection_total_waiting(&inter), 0);
}

static void test_multiple_lanes_depart_in_same_step(void) {
    Intersection inter;
    intersection_init(&inter);

    /* One straight and one right-turn vehicle on North — both served by NS. */
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_SOUTH, "straight");
    intersection_add_vehicle(&inter, ROAD_NORTH, ROAD_WEST,  "right");

    step(&inter);

    ASSERT_EQ(dep_count, 2);
    ASSERT(departed_contains(dep, dep_count, "straight"));
    ASSERT(departed_contains(dep, dep_count, "right"));
}


int main(void) {
    RUN_TEST(test_init_empty);
    RUN_TEST(test_add_vehicle_valid);
    RUN_TEST(test_add_vehicle_rejects_uturn);
    RUN_TEST(test_add_vehicle_routes_to_correct_lane);
    RUN_TEST(test_spec_example);
    RUN_TEST(test_first_step_activates_phase);
    RUN_TEST(test_phase_persists_for_min_steps);
    RUN_TEST(test_phase_switches_to_busier_road);
    RUN_TEST(test_arrow_phase_serves_left_turns);
    RUN_TEST(test_step_count_increments);
    RUN_TEST(test_vehicle_count_decreases_after_departure);
    RUN_TEST(test_multiple_lanes_depart_in_same_step);
    PRINT_RESULTS();
}
