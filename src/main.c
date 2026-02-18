#include <stdio.h>
#include <string.h>
#include "intersection.h"
#include "road.h"

static RoadDir parse_road(const char *s) {
    if (strcmp(s, "north") == 0) return ROAD_NORTH;
    if (strcmp(s, "south") == 0) return ROAD_SOUTH;
    if (strcmp(s, "east")  == 0) return ROAD_EAST;
    if (strcmp(s, "west")  == 0) return ROAD_WEST;
    return ROAD_NONE;
}

int main(void) {
    Intersection inter;
    intersection_init(&inter);

    char line[256];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        char cmd[32];
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }

        if (strcmp(cmd, "addVehicle") == 0) {
            char id[MAX_VEHICLE_ID_LEN];
            char start_str[16], end_str[16];
            if (sscanf(line, "%*s %31s %15s %15s", id, start_str, end_str) != 3) {
                continue;
            }
            RoadDir start = parse_road(start_str);
            RoadDir end   = parse_road(end_str);
            intersection_add_vehicle(&inter, start, end, id);

        } else if (strcmp(cmd, "step") == 0) {
            Vehicle departed[MAX_DEPARTURES_PER_STEP];
            uint8_t count = 0;
            intersection_step(&inter, departed, &count);
            for (uint8_t i = 0; i < count; i++) {
                if (i > 0) putchar(' ');
                fputs(departed[i].id, stdout);
            }
            putchar('\n');
            fflush(stdout);
        }
    }

    return 0;
}
