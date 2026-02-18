#include <string.h>
#include <stdio.h>
#include "road.h"
#include "test_helpers.h"


/*
 * A set of tests for different types of movement
 *
 * PS: I've decided that you can't do the U-turn, sorry
 */

static void test_movement_type_straight(void) {
    ASSERT_EQ(movement_type(ROAD_NORTH, ROAD_SOUTH), MOVE_STRAIGHT);
    ASSERT_EQ(movement_type(ROAD_SOUTH, ROAD_NORTH), MOVE_STRAIGHT);
    ASSERT_EQ(movement_type(ROAD_EAST,  ROAD_WEST),  MOVE_STRAIGHT);
    ASSERT_EQ(movement_type(ROAD_WEST,  ROAD_EAST),  MOVE_STRAIGHT);
}

static void test_movement_type_right(void) {
    /* Heading south (from north): right = west */
    ASSERT_EQ(movement_type(ROAD_NORTH, ROAD_WEST),  MOVE_RIGHT);
    /* Heading north (from south): right = east */
    ASSERT_EQ(movement_type(ROAD_SOUTH, ROAD_EAST),  MOVE_RIGHT);
    /* Heading west  (from east):  right = north */
    ASSERT_EQ(movement_type(ROAD_EAST,  ROAD_NORTH), MOVE_RIGHT);
    /* Heading east  (from west):  right = south */
    ASSERT_EQ(movement_type(ROAD_WEST,  ROAD_SOUTH), MOVE_RIGHT);
}

static void test_movement_type_left(void) {
    /* Heading south (from north): left = east */
    ASSERT_EQ(movement_type(ROAD_NORTH, ROAD_EAST),  MOVE_LEFT);
    /* Heading north (from south): left = west */
    ASSERT_EQ(movement_type(ROAD_SOUTH, ROAD_WEST),  MOVE_LEFT);
    /* Heading west  (from east):  left = south */
    ASSERT_EQ(movement_type(ROAD_EAST,  ROAD_SOUTH), MOVE_LEFT);
    /* Heading east  (from west):  left = north */
    ASSERT_EQ(movement_type(ROAD_WEST,  ROAD_NORTH), MOVE_LEFT);
}

static void test_movement_type_invalid(void) {
    /* U-turns, I'd assumed them to be invalid */
    ASSERT_EQ(movement_type(ROAD_NORTH, ROAD_NORTH), MOVE_INVALID);
    ASSERT_EQ(movement_type(ROAD_SOUTH, ROAD_SOUTH), MOVE_INVALID);
    ASSERT_EQ(movement_type(ROAD_EAST,  ROAD_EAST),  MOVE_INVALID);
    ASSERT_EQ(movement_type(ROAD_WEST,  ROAD_WEST),  MOVE_INVALID);
    /* Sentinel values */
    ASSERT_EQ(movement_type(ROAD_NONE,  ROAD_NORTH), MOVE_INVALID);
    ASSERT_EQ(movement_type(ROAD_NORTH, ROAD_NONE),  MOVE_INVALID);
}


/* Lane for movement */
static void test_lane_for_movement(void) {
    ASSERT_EQ(lane_for_movement(MOVE_LEFT),     LANE_LEFT);
    ASSERT_EQ(lane_for_movement(MOVE_STRAIGHT), LANE_STRAIGHT);
    ASSERT_EQ(lane_for_movement(MOVE_RIGHT),    LANE_RIGHT);
}


/* VehicleQueue operations */
static Vehicle make_vehicle(const char *id, RoadDir end, MovementType mv) {
    Vehicle v;
    memset(&v, 0, sizeof(v));
    strncpy(v.id, id, MAX_VEHICLE_ID_LEN - 1);
    v.end_road     = end;
    v.movement     = mv;
    v.enqueue_step = 0;
    return v;
}

static void test_queue_init(void) {
    VehicleQueue q;
    queue_init(&q);
    ASSERT(queue_is_empty(&q));
    ASSERT(!queue_is_full(&q));
    ASSERT_EQ(q.count, 0);
}

static void test_queue_enqueue_dequeue(void) {
    VehicleQueue q;
    queue_init(&q);

    Vehicle a = make_vehicle("v1", ROAD_SOUTH, MOVE_STRAIGHT);
    Vehicle b = make_vehicle("v2", ROAD_SOUTH, MOVE_STRAIGHT);

    ASSERT(queue_enqueue(&q, &a));
    ASSERT(queue_enqueue(&q, &b));
    ASSERT_EQ(q.count, 2);

    Vehicle out;
    /* FIFO order */
    ASSERT(queue_dequeue(&q, &out));
    ASSERT_STR_EQ(out.id, "v1");
    ASSERT(queue_dequeue(&q, &out));
    ASSERT_STR_EQ(out.id, "v2");
    ASSERT(queue_is_empty(&q));
}

static void test_queue_peek_does_not_remove(void) {
    VehicleQueue q;
    queue_init(&q);

    Vehicle v = make_vehicle("v1", ROAD_SOUTH, MOVE_STRAIGHT);
    queue_enqueue(&q, &v);

    Vehicle out;
    ASSERT(queue_peek(&q, &out));
    ASSERT_STR_EQ(out.id, "v1");
    ASSERT_EQ(q.count, 1); /* still in queue */
}

static void test_queue_dequeue_empty_returns_false(void) {
    VehicleQueue q;
    queue_init(&q);
    Vehicle out;
    ASSERT(!queue_dequeue(&q, &out));
}

static void test_queue_enqueue_full_returns_false(void) {
    VehicleQueue q;
    queue_init(&q);
    Vehicle v = make_vehicle("x", ROAD_SOUTH, MOVE_STRAIGHT);

    for (int i = 0; i < MAX_VEHICLES_PER_LANE; i++) {
        ASSERT(queue_enqueue(&q, &v));
    }
    ASSERT(queue_is_full(&q));
    ASSERT(!queue_enqueue(&q, &v));
    ASSERT_EQ(q.count, MAX_VEHICLES_PER_LANE);
}

static void test_queue_wraps_around(void) {
    VehicleQueue q;
    queue_init(&q);
    Vehicle v = make_vehicle("x", ROAD_SOUTH, MOVE_STRAIGHT);

    /* Fill, then drain half, then refill - exercises the ring wrap. */
    for (int i = 0; i < MAX_VEHICLES_PER_LANE; i++) {
        queue_enqueue(&q, &v);
    }
    for (int i = 0; i < MAX_VEHICLES_PER_LANE / 2; i++) {
        queue_dequeue(&q, NULL);
    }
    for (int i = 0; i < MAX_VEHICLES_PER_LANE / 2; i++) {
        ASSERT(queue_enqueue(&q, &v));
    }
    ASSERT(queue_is_full(&q));
}

/* Road operations */
static void test_road_init(void) {
    Road r;
    road_init(&r);
    ASSERT_EQ(road_total_count(&r), 0);
    ASSERT_EQ(road_lane_count(&r, LANE_LEFT),     0);
    ASSERT_EQ(road_lane_count(&r, LANE_STRAIGHT), 0);
    ASSERT_EQ(road_lane_count(&r, LANE_RIGHT),    0);
}

static void test_road_enqueue_routes_to_correct_lane(void) {
    Road r;
    road_init(&r);

    /* 
     * North road: straight goes to LANE_STRAIGHT, right to LANE_RIGHT,
     * left to LANE_LEFT 
     */
    Vehicle vl = make_vehicle("left",  ROAD_EAST, MOVE_LEFT);
    Vehicle vs = make_vehicle("str",   ROAD_SOUTH, MOVE_STRAIGHT);
    Vehicle vr = make_vehicle("right", ROAD_WEST, MOVE_RIGHT);

    ASSERT(road_enqueue(&r, &vl));
    ASSERT(road_enqueue(&r, &vs));
    ASSERT(road_enqueue(&r, &vr));

    ASSERT_EQ(road_lane_count(&r, LANE_LEFT),     1);
    ASSERT_EQ(road_lane_count(&r, LANE_STRAIGHT), 1);
    ASSERT_EQ(road_lane_count(&r, LANE_RIGHT),    1);
    ASSERT_EQ(road_total_count(&r), 3);
}

static void test_road_dequeue_lane(void) {
    Road r;
    road_init(&r);

    Vehicle vs = make_vehicle("s1", ROAD_SOUTH, MOVE_STRAIGHT);
    road_enqueue(&r, &vs);

    Vehicle out;
    ASSERT(road_dequeue_lane(&r, LANE_STRAIGHT, &out));
    ASSERT_STR_EQ(out.id, "s1");
    ASSERT_EQ(road_lane_count(&r, LANE_STRAIGHT), 0);

    /* Dequeue from empty lane returns false */
    ASSERT(!road_dequeue_lane(&r, LANE_STRAIGHT, &out));
}

static void test_road_peek_lane(void) {
    Road r;
    road_init(&r);

    Vehicle vr = make_vehicle("r1", ROAD_WEST, MOVE_RIGHT);
    road_enqueue(&r, &vr);

    Vehicle out;
    ASSERT(road_peek_lane(&r, LANE_RIGHT, &out));
    ASSERT_STR_EQ(out.id, "r1");
    ASSERT_EQ(road_lane_count(&r, LANE_RIGHT), 1); /* still there */
}


int main(void) {
    RUN_TEST(test_movement_type_straight);
    RUN_TEST(test_movement_type_right);
    RUN_TEST(test_movement_type_left);
    RUN_TEST(test_movement_type_invalid);
    RUN_TEST(test_lane_for_movement);

    RUN_TEST(test_queue_init);
    RUN_TEST(test_queue_enqueue_dequeue);
    RUN_TEST(test_queue_peek_does_not_remove);
    RUN_TEST(test_queue_dequeue_empty_returns_false);
    RUN_TEST(test_queue_enqueue_full_returns_false);
    RUN_TEST(test_queue_wraps_around);

    RUN_TEST(test_road_init);
    RUN_TEST(test_road_enqueue_routes_to_correct_lane);
    RUN_TEST(test_road_dequeue_lane);
    RUN_TEST(test_road_peek_lane);

    PRINT_RESULTS();
}
