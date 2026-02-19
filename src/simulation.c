#include "simulation.h"
#include "intersection.h"
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

void simulation_init(SimulationContext *ctx) {
    memset(ctx, 0, sizeof(*ctx));
    intersection_init(&ctx->inter);
}

void simulation_tick(SimulationContext *ctx, const EmbeddedHAL *hal) {
    /* 1. Edge-detect new arrivals.
     *
     * A vehicle is enqueued exactly once: on the step where the sensor
     * transitions from clear to occupied. Subsequent steps where the
     * sensor remains active are ignored, so the same physical vehicle
     * cannot be counted twice. */
    for (int r = 0; r < ROAD_COUNT; r++) {
        for (int l = 0; l < LANES_PER_ROAD; l++) {
            bool now = hal->sense_lane((RoadDir)r, (Lane)l);
            if (now && !ctx->prev_sense[r][l]) {
                char id[MAX_VEHICLE_ID_LEN];
                snprintf(id, sizeof(id), "v%"PRIu32, ++ctx->vehicle_counter);
                intersection_add_vehicle_by_lane(&ctx->inter,
                                                 (RoadDir)r, (Lane)l, id);
            }
            ctx->prev_sense[r][l] = now;
        }
    }

    /* 2. Advance the intersection by one simulation step. */
    Vehicle departed[MAX_DEPARTURES_PER_STEP];
    uint8_t count;
    intersection_step(&ctx->inter, departed, &count);

    /* 3. Reflect the new light states on the physical hardware. */
    for (int r = 0; r < ROAD_COUNT; r++) {
        hal->set_light((RoadDir)r, ctx->inter.lights[r].state);
    }
}
