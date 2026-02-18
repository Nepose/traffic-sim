#include "traffic_light.h"

void traffic_light_init(TrafficLight *tl) {
    tl->state           = LIGHT_RED;
    tl->steps_remaining = 0;
}

void traffic_light_set_green(TrafficLight *tl, uint8_t duration) {
    tl->state           = LIGHT_GREEN;
    tl->steps_remaining = duration;
}

void traffic_light_set_green_arrow(TrafficLight *tl, uint8_t duration) {
    tl->state           = LIGHT_GREEN_ARROW;
    tl->steps_remaining = duration;
}

void traffic_light_tick(TrafficLight *tl) {
    if (tl->state == LIGHT_RED) {
        return;
    }

    if (tl->steps_remaining > 0) {
        tl->steps_remaining--;
    }

    if (tl->steps_remaining == 0) {
        switch (tl->state) {
            case LIGHT_GREEN:
            case LIGHT_GREEN_ARROW:
                tl->state           = LIGHT_YELLOW;
                tl->steps_remaining = YELLOW_STEPS;
                break;
            case LIGHT_YELLOW:
                tl->state           = LIGHT_RED;
                break;
            default:
                break;
        }
    }
}

bool traffic_light_is_green(const TrafficLight *tl) {
    return tl->state == LIGHT_GREEN || tl->state == LIGHT_GREEN_ARROW;
}

bool traffic_light_is_yellow(const TrafficLight *tl) {
    return tl->state == LIGHT_YELLOW;
}

bool traffic_light_is_red(const TrafficLight *tl) {
    return tl->state == LIGHT_RED;
}

const char *traffic_light_state_str(const TrafficLight *tl) {
    switch (tl->state) {
        case LIGHT_RED:          return "RED";
        case LIGHT_YELLOW:       return "YELLOW";
        case LIGHT_GREEN:        return "GREEN";
        case LIGHT_GREEN_ARROW:  return "GREEN_ARROW";
        default:                 return "UNKNOWN";
    }
}
