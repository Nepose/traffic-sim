#ifndef ROAD_H
#define ROAD_H

/*
 * road.h - ring-buffer queue operations and road-level helpers
 *
 * Provides:
 *   movement_type()     - pure lookup: (startRoad, endRoad) -> MovementType
 *   lane_for_movement() - maps MovementType to the lane it belongs to
 *   queue_*             - operations on a single VehicleQueue (one lane)
 *   road_*              - operations on a Road (all three lanes)
 */

#include "types.h"

/* -------------------------------------------------------------------------
 * Movement / lane derivation
 * ---------------------------------------------------------------------- */

/*
 * Derive the movement type from start and end road.
 * Returns MOVE_INVALID for U-turns (start == end) or ROAD_NONE inputs.
 *
 * The mapping assumes right-hand traffic:
 *
 *   start\end   N          S          E          W
 *   N        INVALID    STRAIGHT    LEFT       RIGHT
 *   S        STRAIGHT   INVALID     RIGHT      LEFT
 *   E        RIGHT      LEFT        INVALID    STRAIGHT
 *   W        LEFT       RIGHT       STRAIGHT   INVALID
 */
MovementType movement_type(RoadDir start, RoadDir end);

/* Map a MovementType to the lane index it belongs to. */
Lane lane_for_movement(MovementType movement);

/* -------------------------------------------------------------------------
 * VehicleQueue operations (single lane)
 * ---------------------------------------------------------------------- */

/* Zero-initialise a queue. Must be called before first use. */
void queue_init(VehicleQueue *q);

/* Returns true if the queue has no vehicles. */
bool queue_is_empty(const VehicleQueue *q);

/* Returns true if the queue has no room for more vehicles. */
bool queue_is_full(const VehicleQueue *q);

/*
 * Add a vehicle to the back of the queue.
 * Returns false (and leaves the queue unchanged) if the queue is full.
 */
bool queue_enqueue(VehicleQueue *q, const Vehicle *v);

/*
 * Remove the front vehicle and write it to *out.
 * Returns false (and leaves *out unchanged) if the queue is empty.
 * Passing NULL for out is valid when the caller only needs the side-effect.
 */
bool queue_dequeue(VehicleQueue *q, Vehicle *out);

/*
 * Read the front vehicle without removing it.
 * Returns false if the queue is empty.
 */
bool queue_peek(const VehicleQueue *q, Vehicle *out);

/* -------------------------------------------------------------------------
 * Road operations (all three lanes)
 * ---------------------------------------------------------------------- */

/* Zero-initialise all three lane queues. */
void road_init(Road *r);

/*
 * Enqueue a vehicle onto the lane determined by v->movement.
 * Returns false if that lane is full.
 */
bool road_enqueue(Road *r, const Vehicle *v);

/*
 * Dequeue the front vehicle from the given lane.
 * Returns false if that lane is empty.
 */
bool road_dequeue_lane(Road *r, Lane lane, Vehicle *out);

/*
 * Peek at the front vehicle in the given lane without removing it.
 * Returns false if that lane is empty.
 */
bool road_peek_lane(const Road *r, Lane lane, Vehicle *out);

/* Number of vehicles waiting in a specific lane. */
uint8_t road_lane_count(const Road *r, Lane lane);

/* Total number of vehicles waiting across all lanes of this road. */
uint8_t road_total_count(const Road *r);

#endif /* ROAD_H */
