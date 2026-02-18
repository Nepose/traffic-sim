#include "road.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Movement / lane derivation
 * ---------------------------------------------------------------------- */

MovementType movement_type(RoadDir start, RoadDir end) {
    if (start >= ROAD_NONE || end >= ROAD_NONE || start == end) {
        return MOVE_INVALID;
    }

    /*
     * Lookup table indexed by [start][end].
     * Assumes right-hand traffic; see road.h for the full mapping.
     */
    static const MovementType table[4][4] = {
        /*             N              S              E              W      */
        /* N */ { MOVE_INVALID,  MOVE_STRAIGHT, MOVE_LEFT,     MOVE_RIGHT  },
        /* S */ { MOVE_STRAIGHT, MOVE_INVALID,  MOVE_RIGHT,    MOVE_LEFT   },
        /* E */ { MOVE_RIGHT,    MOVE_LEFT,     MOVE_INVALID,  MOVE_STRAIGHT},
        /* W */ { MOVE_LEFT,     MOVE_RIGHT,    MOVE_STRAIGHT, MOVE_INVALID },
    };

    return table[start][end];
}

Lane lane_for_movement(MovementType movement) {
    switch (movement) {
        case MOVE_LEFT:     return LANE_LEFT;
        case MOVE_STRAIGHT: return LANE_STRAIGHT;
        case MOVE_RIGHT:    return LANE_RIGHT;
        default:            return LANE_LEFT; /* unreachable for valid input */
    }
}

/* -------------------------------------------------------------------------
 * VehicleQueue operations
 * ---------------------------------------------------------------------- */

void queue_init(VehicleQueue *q) {
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
}

bool queue_is_empty(const VehicleQueue *q) {
    return q->count == 0;
}

bool queue_is_full(const VehicleQueue *q) {
    return q->count >= MAX_VEHICLES_PER_LANE;
}

bool queue_enqueue(VehicleQueue *q, const Vehicle *v) {
    if (queue_is_full(q)) {
        return false;
    }
    q->buf[q->tail] = *v;
    q->tail = (uint8_t)((q->tail + 1) % MAX_VEHICLES_PER_LANE);
    q->count++;
    return true;
}

bool queue_dequeue(VehicleQueue *q, Vehicle *out) {
    if (queue_is_empty(q)) {
        return false;
    }
    if (out != NULL) {
        *out = q->buf[q->head];
    }
    q->head = (uint8_t)((q->head + 1) % MAX_VEHICLES_PER_LANE);
    q->count--;
    return true;
}

bool queue_peek(const VehicleQueue *q, Vehicle *out) {
    if (queue_is_empty(q)) {
        return false;
    }
    *out = q->buf[q->head];
    return true;
}

/* -------------------------------------------------------------------------
 * Road operations
 * ---------------------------------------------------------------------- */

void road_init(Road *r) {
    for (int i = 0; i < LANES_PER_ROAD; i++) {
        queue_init(&r->lanes[i]);
    }
}

bool road_enqueue(Road *r, const Vehicle *v) {
    Lane lane = lane_for_movement(v->movement);
    return queue_enqueue(&r->lanes[lane], v);
}

bool road_dequeue_lane(Road *r, Lane lane, Vehicle *out) {
    return queue_dequeue(&r->lanes[lane], out);
}

bool road_peek_lane(const Road *r, Lane lane, Vehicle *out) {
    return queue_peek(&r->lanes[lane], out);
}

uint8_t road_lane_count(const Road *r, Lane lane) {
    return r->lanes[lane].count;
}

uint8_t road_total_count(const Road *r) {
    uint8_t total = 0;
    for (int i = 0; i < LANES_PER_ROAD; i++) {
        total += r->lanes[i].count;
    }
    return total;
}
