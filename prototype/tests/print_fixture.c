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

static cane_command_t command_for(int source, int sides, int unavailable) {
    cane_command_t command;
    memset(&command, 0, sizeof(command));
    command.source = source;
    command.sides = sides;
    command.system_unavailable = unavailable;
    command.tracking = CANE_TRACKING_NORMAL;
    command.plane_hit_cm = CANE_PLANE_CM_NONE;
    command.sequence = 9;
    command.time_ms_low = 200;
    return command;
}

int main(void) {
    cane_command_t left = command_for(CANE_SOURCE_SIMULATION, CANE_SIDE_LEFT, 0);
    cane_command_t both = command_for(CANE_SOURCE_IMAGE_HEURISTIC, CANE_SIDE_LEFT | CANE_SIDE_RIGHT, 0);
    cane_command_t none = command_for(CANE_SOURCE_CAMERA, 0, 1);
    printf("[\n");
    emit("left", &left, 0);
    emit("both", &both, 0);
    emit("none", &none, 1);
    printf("]\n");
    return 0;
}
