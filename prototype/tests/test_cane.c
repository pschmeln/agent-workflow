#include "cane_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", message);
        failures++;
    }
}

static cane_input_t camera_ready(void) {
    cane_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = CANE_SOURCE_CAMERA;
    input.tracking = CANE_TRACKING_NORMAL;
    input.camera_authorized = 1;
    input.world_tracking_supported = 1;
    return input;
}

static void test_pins(void) {
    expect(CANE_PIN_LEFT == 2, "left buzzer is D2");
    expect(CANE_PIN_STATUS == 3, "status logic is D3");
    expect(CANE_PIN_RIGHT == 4, "right buzzer is D4");
    expect(CANE_PIN_LINK == 5, "link indicator is D5");
    expect(CANE_PACKET_VERSION == 2, "packet version is 2");
}

static void test_crc_vector(void) {
    const uint8_t digits[] = "123456789";
    expect(cane_crc16(digits, 9) == 0x29B1, "CRC-16/CCITT-FALSE vector");
}

static void test_camera_never_sets_sides(void) {
    cane_input_t input = camera_ready();
    cane_assessment_t assessment;
    input.simulation_sides = CANE_SIDE_LEFT;
    input.image_sides = CANE_SIDE_RIGHT;
    input.has_plane_hit = 1;
    input.plane_hit_meters = 0.2f;
    cane_assess(&input, &assessment);
    expect(assessment.sides == 0, "camera path sets no side");
    expect(assessment.system_unavailable == 0, "normal camera pipeline is available");
    expect(assessment.plane_hit_valid == 1, "camera plane context can still be recorded");
    expect(assessment.plane_hit_cm == 20, "0.2 m plane hit is 20 cm of context");
}

static void test_image_sides_and_faults(void) {
    cane_input_t input = camera_ready();
    cane_assessment_t assessment;
    cane_command_t command;
    uint8_t packet[CANE_PACKET_BYTES];
    input.source = CANE_SOURCE_IMAGE_HEURISTIC;
    input.image_pipeline_ready = 1;
    input.image_sides = CANE_SIDE_LEFT | CANE_SIDE_RIGHT | 0x80;
    input.has_plane_hit = 1;
    input.plane_hit_meters = 0.4f;
    cane_assess(&input, &assessment);
    expect(assessment.sides == (CANE_SIDE_LEFT | CANE_SIDE_RIGHT), "both image sides are kept");
    expect((assessment.sides & 0x80) == 0, "extra side bits are dropped");
    expect(assessment.plane_hit_valid == 0, "image path drops a distance");
    expect(assessment.system_unavailable == 0, "fresh image path is available");
    cane_command_from_assessment(&assessment, 4, 50, &command);
    expect(cane_command_encode(&command, packet) == 1, "image packet encodes");
    expect(packet[0] == 2, "image packet version is 2");
    expect(packet[2] == (CANE_SIDE_LEFT | CANE_SIDE_RIGHT), "side bits are byte 2");
    expect(packet[4] == 0, "byte 4 stays zero");
    expect(packet[14] == 0xFF && packet[15] == 0xFF, "image packet has no distance");

    input.image_pipeline_ready = 0;
    cane_assess(&input, &assessment);
    expect(assessment.sides == 0, "stale image path clears both sides");
    expect(assessment.system_unavailable == 1, "stale image path is unavailable");

    command.plane_hit_cm = 30;
    expect(cane_command_encode(&command, packet) == 0, "image distance is rejected");
    command.plane_hit_cm = CANE_PLANE_CM_NONE;
    command.sides = 0x10;
    expect(cane_command_encode(&command, packet) == 0, "unknown side bit is rejected");
}

static void test_version_mismatch_is_rejected(void) {
    uint8_t legacy[CANE_PACKET_BYTES];
    uint16_t crc;
    cane_link_t link;
    cane_outputs_t out;
    memset(legacy, 0, sizeof(legacy));
    legacy[0] = 1;
    legacy[1] = CANE_SOURCE_SIMULATION;
    legacy[2] = 1;
    legacy[4] = 1;
    legacy[5] = CANE_TRACKING_NORMAL;
    legacy[14] = 0xFF;
    legacy[15] = 0xFF;
    crc = cane_crc16(legacy, 16);
    legacy[16] = (uint8_t)(crc & 0xFF);
    legacy[17] = (uint8_t)(crc >> 8);
    cane_link_init(&link);
    expect(cane_link_accept(&link, legacy, sizeof(legacy), 1000) == 0, "version 1 packet is rejected");
    cane_link_outputs(&link, 1000, &out);
    expect(out.link_state == CANE_LINK_OUTAGE, "rejected legacy packet leaves the link in outage");
    expect(out.left == 0 && out.right == 0, "rejected legacy packet does not sound a side");
}

static void fill_sides(cane_command_t *command, int source, int sides, int unavailable) {
    memset(command, 0, sizeof(*command));
    command->source = source;
    command->sides = sides;
    command->system_unavailable = unavailable;
    command->tracking = CANE_TRACKING_NORMAL;
    command->plane_hit_cm = CANE_PLANE_CM_NONE;
    command->sequence = 9;
    command->time_ms_low = 200;
}

static void accept_sides(cane_link_t *link, int sides, int unavailable, uint32_t now_ms) {
    cane_command_t command;
    uint8_t packet[CANE_PACKET_BYTES];
    fill_sides(&command, CANE_SOURCE_SIMULATION, sides, unavailable);
    expect(cane_command_encode(&command, packet) == 1, "side packet encodes");
    expect(cane_link_accept(link, packet, CANE_PACKET_BYTES, now_ms) == 1, "side packet accepted");
}

static void test_patterns_and_watchdog(void) {
    cane_link_t link;
    cane_outputs_t out;
    uint8_t bad[CANE_PACKET_BYTES];
    cane_link_init(&link);
    cane_link_outputs(&link, 0, &out);
    expect(out.link_state == CANE_LINK_OUTAGE && out.left == 0 && out.right == 0, "startup is outage with buzzers off");
    expect(out.link == 1, "outage link pattern is on at t=0");

    accept_sides(&link, CANE_SIDE_LEFT | CANE_SIDE_RIGHT, 0, 1000);
    cane_link_outputs(&link, 1400, &out);
    expect(out.left == 1 && out.right == 1, "both object sides use the object pulse at t=1400");
    expect(out.status == 0, "object warning does not raise the status pin");
    expect(cane_fault_blip(1400) == 0, "fault blip is off at t=1400");

    accept_sides(&link, 0, 1, 1000);
    cane_link_outputs(&link, 1180, &out);
    expect(out.left == 1 && out.right == 1 && out.status == 1, "unavailable uses both buzzers and D3");
    expect(cane_pulse(180, CANE_OBJECT_PERIOD_MS, CANE_OBJECT_ON_MS) == 0, "object pulse is off at phase 180");

    cane_link_outputs(&link, 1000 + CANE_WATCHDOG_MS, &out);
    expect(out.link_state == CANE_LINK_HEARTBEAT, "500 ms is still inside the watchdog");
    cane_link_outputs(&link, 1000 + CANE_WATCHDOG_MS + 1, &out);
    expect(out.link_state == CANE_LINK_OUTAGE && out.left == 0 && out.right == 0, "watchdog silences both buzzers");

    memset(bad, 0, sizeof(bad));
    expect(cane_link_accept(&link, bad, sizeof(bad), 5000) == 0, "corrupt packet is ignored");
}

static void test_displacement(void) {
    float meters = 99.0f;
    expect(cane_camera_displacement_m(0, 0, 0, 3, 4, 0, 1, &meters) == 1, "displacement is valid");
    expect(meters > 4.999f && meters < 5.001f, "displacement magnitude is 5 m");
    expect(cane_camera_displacement_m(0, 0, 0, 3, 4, 0, 0, &meters) == 0, "non-normal tracking has no displacement");
}

int main(void) {
    test_pins();
    test_crc_vector();
    test_camera_never_sets_sides();
    test_image_sides_and_faults();
    test_version_mismatch_is_rejected();
    test_patterns_and_watchdog();
    test_displacement();
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("cane logic tests passed\n");
    return 0;
}
