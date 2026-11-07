#include "cane_logic.h"

#include <stdio.h>
#include <string.h>

static void emit(const char *name, const cane_command_t *command, int last) {
    uint8_t bytes[CANE_PACKET_BYTES];
    int ok = cane_command_encode(command, bytes);
    size_t i;
    printf("  {\"name\":\"%s\",\"ok\":%d,\"bytes\":[", name, ok);
    if (ok) {
        for (i = 0; i < CANE_PACKET_BYTES; i++) {
            printf("%s%u", i == 0 ? "" : ",", bytes[i]);
        }
    }
    printf("]}%s\n", last ? "" : ",");
}

static cane_command_t command_for(int source, int hazard, int unavailable, int route, uint16_t plane_cm) {
    cane_command_t command;
    memset(&command, 0, sizeof(command));
    command.source = source;
    command.hazard = hazard;
    command.system_unavailable = unavailable;
    command.route_cue = route;
    command.tracking = CANE_TRACKING_NORMAL;
    command.plane_hit_cm = plane_cm;
    command.sequence = 9;
    command.time_ms_low = 200;
    return command;
}

int main(void) {
    cane_command_t simulation = command_for(
        CANE_SOURCE_SIMULATION, CANE_HAZARD_ELEVATED, 0, CANE_ROUTE_DEMO_LEFT, CANE_PLANE_CM_NONE);
    cane_command_t image = command_for(
        CANE_SOURCE_IMAGE_HEURISTIC, CANE_HAZARD_ELEVATED, 0, CANE_ROUTE_NONE, CANE_PLANE_CM_NONE);
    cane_command_t camera = command_for(
        CANE_SOURCE_CAMERA, CANE_HAZARD_NO_CLAIM, 1, CANE_ROUTE_NONE, CANE_PLANE_CM_NONE);
    printf("[\n");
    emit("simulation", &simulation, 0);
    emit("image", &image, 0);
    emit("camera", &camera, 1);
    printf("]\n");
    return 0;
}
