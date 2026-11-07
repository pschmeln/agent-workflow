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

static cane_input_t simulation_input(void) {
    cane_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = CANE_SOURCE_SIMULATION;
    input.tracking = CANE_TRACKING_NORMAL;
    input.camera_authorized = 1;
    input.world_tracking_supported = 1;
    return input;
}

static void test_crc_vector(void) {
    const uint8_t digits[] = "123456789";
    expect(cane_crc16(digits, 9) == 0x29B1, "CRC-16/CCITT-FALSE vector");
}

static void test_camera_makes_no_hazard_or_route(void) {
    cane_input_t input = camera_ready();
    cane_assessment_t assessment;
    input.simulation_elevated_hazard = 1;
    input.route_cue = CANE_ROUTE_DEMO_LEFT;
    input.has_plane_hit = 1;
    input.plane_hit_meters = 0.2f;
    input.scene_depth_supported = 0;
    cane_assess(&input, &assessment);
    expect(assessment.source == CANE_SOURCE_CAMERA, "camera source preserved");
    expect(assessment.hazard == CANE_HAZARD_NO_CLAIM, "camera path makes no hazard claim");
    expect(assessment.route_cue == CANE_ROUTE_NONE, "camera path clears route cues");
    expect(assessment.system_unavailable == 0, "normal authorized tracking is available");
    expect(assessment.plane_hit_valid == 1, "close plane hit is recorded as context");
    expect(assessment.plane_hit_cm == 20, "0.2 m plane hit is 20 cm of context");
    expect(assessment.hazard == CANE_HAZARD_NO_CLAIM, "plane hit does not raise a hazard");
}

static void test_camera_unavailable_conditions(void) {
    cane_assessment_t assessment;
    cane_input_t input = camera_ready();
    input.tracking = CANE_TRACKING_LIMITED;
    cane_assess(&input, &assessment);
    expect(assessment.system_unavailable == 1, "limited tracking is unavailable");
    expect(assessment.hazard == CANE_HAZARD_NO_CLAIM, "limited tracking is not a hazard claim");

    input = camera_ready();
    input.tracking = CANE_TRACKING_UNAVAILABLE;
    cane_assess(&input, &assessment);
    expect(assessment.system_unavailable == 1, "unavailable tracking stays unavailable");

    input = camera_ready();
    input.camera_authorized = 0;
    cane_assess(&input, &assessment);
    expect(assessment.system_unavailable == 1, "denied camera is unavailable");

    input = camera_ready();
    input.world_tracking_supported = 0;
    input.tracking = CANE_TRACKING_UNSUPPORTED;
    cane_assess(&input, &assessment);
    expect(assessment.system_unavailable == 1, "unsupported world tracking is unavailable");

    input = camera_ready();
    input.scene_depth_supported = 0;
    cane_assess(&input, &assessment);
    expect(assessment.system_unavailable == 0, "missing scene depth alone is not unavailable");
    expect(assessment.hazard == CANE_HAZARD_NO_CLAIM, "missing scene depth is not a hazard");
}

static void test_plane_hit_bounds(void) {
    cane_assessment_t assessment;
    cane_input_t input = camera_ready();
    input.has_plane_hit = 1;
    input.plane_hit_meters = 0.0f;
    cane_assess(&input, &assessment);
    expect(assessment.plane_hit_valid == 0, "zero plane hit is not valid");

    input.plane_hit_meters = -1.0f;
    cane_assess(&input, &assessment);
    expect(assessment.plane_hit_valid == 0, "negative plane hit is not valid");

    input.plane_hit_meters = nanf("");
    cane_assess(&input, &assessment);
    expect(assessment.plane_hit_valid == 0, "NaN plane hit is not valid");

    input.plane_hit_meters = 25.0f;
    cane_assess(&input, &assessment);
    expect(assessment.plane_hit_valid == 0, "plane hit beyond 20 m is not kept");

    input.tracking = CANE_TRACKING_LIMITED;
    input.plane_hit_meters = 1.5f;
    cane_assess(&input, &assessment);
    expect(assessment.plane_hit_valid == 0, "plane hit requires normal tracking");
}

static void test_simulation_channels_stay_separate(void) {
    cane_assessment_t assessment;
    cane_input_t input = simulation_input();
    input.simulation_elevated_hazard = 1;
    input.simulation_unavailable = 1;
    input.route_cue = CANE_ROUTE_DEMO_RIGHT;
    input.has_plane_hit = 1;
    input.plane_hit_meters = 0.3f;
    cane_assess(&input, &assessment);
    expect(assessment.source == CANE_SOURCE_SIMULATION, "simulation source preserved");
    expect(assessment.hazard == CANE_HAZARD_ELEVATED, "simulation can request a hazard");
    expect(assessment.system_unavailable == 1, "simulation unavailable is independent");
    expect(assessment.route_cue == CANE_ROUTE_DEMO_RIGHT, "demo route is kept");
    expect(assessment.hazard == CANE_HAZARD_ELEVATED, "route cue does not clear a hazard");

    input.route_cue = 9;
    input.simulation_elevated_hazard = 0;
    cane_assess(&input, &assessment);
    expect(assessment.route_cue == CANE_ROUTE_NONE, "unknown route value is dropped");
    expect(assessment.hazard == CANE_HAZARD_NO_CLAIM, "simulation without the flag makes no claim");
}

static void test_displacement_is_not_velocity(void) {
    float meters = 99.0f;
    expect(cane_camera_displacement_m(0, 0, 0, 3, 4, 0, 1, &meters) == 1, "3-4-5 displacement is valid");
    expect(meters > 4.999f && meters < 5.001f, "displacement magnitude is 5 m");
    meters = 99.0f;
    expect(cane_camera_displacement_m(0, 0, 0, 3, 4, 0, 0, &meters) == 0, "non-normal tracking has no displacement");
    expect(meters == 99.0f, "rejected displacement does not write an output");
    expect(cane_camera_displacement_m(0, 0, 0, 0.0f / 0.0f, 0, 0, 1, &meters) == 0, "NaN pose is rejected");
}

static void fill_command(cane_command_t *command, int source, int hazard, int route) {
    memset(command, 0, sizeof(*command));
    command->source = source;
    command->hazard = hazard;
    command->route_cue = route;
    command->tracking = CANE_TRACKING_NORMAL;
    command->plane_hit_cm = CANE_PLANE_CM_NONE;
    command->sequence = 7;
    command->time_ms_low = 1234;
}

static void test_packet_roundtrip_and_rejection(void) {
    cane_command_t command;
    cane_command_t decoded;
    uint8_t packet[CANE_PACKET_BYTES];
    uint8_t altered[CANE_PACKET_BYTES];
    fill_command(&command, CANE_SOURCE_SIMULATION, CANE_HAZARD_ELEVATED, CANE_ROUTE_DEMO_LEFT);
    command.system_unavailable = 1;
    command.plane_hit_cm = 150;
    command.scene_depth_supported = 1;
    expect(cane_command_encode(&command, packet) == 1, "simulation command encodes");
    expect(cane_command_decode(packet, CANE_PACKET_BYTES, &decoded) == 1, "simulation command decodes");
    expect(decoded.source == CANE_SOURCE_SIMULATION, "decoded source");
    expect(decoded.hazard == CANE_HAZARD_ELEVATED, "decoded hazard");
    expect(decoded.system_unavailable == 1, "decoded unavailable");
    expect(decoded.route_cue == CANE_ROUTE_DEMO_LEFT, "decoded route");
    expect(decoded.sequence == 7, "decoded sequence");
    expect(decoded.time_ms_low == 1234, "decoded sender time");
    expect(decoded.plane_hit_cm == 150, "decoded plane context");
    expect(decoded.scene_depth_supported == 1, "decoded scene-depth flag");

    memcpy(altered, packet, sizeof(packet));
    altered[2] ^= 0x01;
    expect(cane_command_decode(altered, CANE_PACKET_BYTES, &decoded) == 0, "flipped hazard fails CRC");
    expect(cane_command_decode(packet, CANE_PACKET_BYTES - 1, &decoded) == 0, "short packet is rejected");

    fill_command(&command, CANE_SOURCE_CAMERA, CANE_HAZARD_ELEVATED, CANE_ROUTE_NONE);
    expect(cane_command_admissible(&command) == 0, "camera hazard is not admissible");
    expect(cane_command_encode(&command, packet) == 0, "encoder refuses a camera hazard");

    fill_command(&command, CANE_SOURCE_CAMERA, CANE_HAZARD_NO_CLAIM, CANE_ROUTE_DEMO_RIGHT);
    expect(cane_command_encode(&command, packet) == 0, "encoder refuses a camera route cue");
}

static void accept_simulation(cane_link_t *link, int hazard, int unavailable, int route, uint16_t plane_cm, uint32_t now_ms) {
    cane_command_t command;
    uint8_t packet[CANE_PACKET_BYTES];
    fill_command(&command, CANE_SOURCE_SIMULATION, hazard, route);
    command.system_unavailable = unavailable;
    command.plane_hit_cm = plane_cm;
    expect(cane_command_encode(&command, packet) == 1, "fixture packet encodes");
    expect(cane_link_accept(link, packet, CANE_PACKET_BYTES, now_ms) == 1, "fixture packet accepted");
}

static void test_watchdog_and_outputs(void) {
    cane_link_t link;
    cane_outputs_t at_hazard;
    cane_outputs_t at_other_plane;
    cane_outputs_t later;
    uint8_t bad[CANE_PACKET_BYTES];
    cane_link_init(&link);
    cane_link_outputs(&link, 0, &at_hazard);
    expect(at_hazard.link_state == CANE_LINK_OUTAGE, "no packet starts in link outage");
    expect(at_hazard.hazard == 0 && at_hazard.unavailable == 0 && at_hazard.route == 0, "outage clears commanded pins");
    expect(at_hazard.link == 1, "outage pattern is on at t=0");

    accept_simulation(&link, CANE_HAZARD_ELEVATED, 1, CANE_ROUTE_DEMO_LEFT, 80, 1000);
    cane_link_outputs(&link, 1000, &at_hazard);
    expect(at_hazard.link_state == CANE_LINK_HEARTBEAT, "fresh packet is a heartbeat");
    expect(at_hazard.hazard == 1, "hazard pattern is on at the start of its period");
    expect(at_hazard.unavailable == 1, "unavailable pattern is on at the start of its period");
    expect(at_hazard.route == 1, "route pattern is on at the start of its period");
    expect(at_hazard.link == 0, "healthy link blip is off 1000 ms into its 2000 ms period");

    accept_simulation(&link, CANE_HAZARD_ELEVATED, 1, CANE_ROUTE_DEMO_LEFT, 400, 1000);
    cane_link_outputs(&link, 1000, &at_other_plane);
    expect(at_other_plane.hazard == at_hazard.hazard, "plane centimeters do not change hazard output");
    expect(at_other_plane.unavailable == at_hazard.unavailable, "plane centimeters do not change unavailable");
    expect(at_other_plane.route == at_hazard.route, "plane centimeters do not change route");
    expect(at_other_plane.link == at_hazard.link, "plane centimeters do not change link");

    cane_link_outputs(&link, 1000 + CANE_WATCHDOG_MS, &later);
    expect(later.link_state == CANE_LINK_HEARTBEAT, "elapsed watchdog limit is still a heartbeat");
    cane_link_outputs(&link, 1000 + CANE_WATCHDOG_MS + 1, &later);
    expect(later.link_state == CANE_LINK_OUTAGE, "one millisecond past the watchdog is an outage");
    expect(later.hazard == 0 && later.unavailable == 0 && later.route == 0, "outage drops the last command");

    memset(bad, 0, sizeof(bad));
    expect(cane_link_accept(&link, bad, sizeof(bad), 5000) == 0, "corrupt packet is ignored");
    cane_link_outputs(&link, 1000 + CANE_WATCHDOG_MS + 1, &later);
    expect(later.link_state == CANE_LINK_OUTAGE, "rejected packet does not refresh the watchdog");
}

static void test_patterns(void) {
    expect(cane_pulse(0, CANE_LINK_OUT_PERIOD_MS, CANE_LINK_OUT_ON_MS) == 1, "outage pulse starts on");
    expect(cane_pulse(CANE_LINK_OUT_ON_MS, CANE_LINK_OUT_PERIOD_MS, CANE_LINK_OUT_ON_MS) == 0, "outage pulse turns off");
    expect(cane_pulse(0, CANE_LINK_OK_PERIOD_MS, CANE_LINK_OK_ON_MS) == 1, "heartbeat starts with a blip");
    expect(cane_pulse(CANE_LINK_OK_ON_MS, CANE_LINK_OK_PERIOD_MS, CANE_LINK_OK_ON_MS) == 0, "heartbeat blip ends");
    expect(cane_pulse(CANE_HAZARD_ON_MS, CANE_HAZARD_PERIOD_MS, CANE_HAZARD_ON_MS) == 0, "hazard pulse is half period");
}

static void test_status_roundtrip(void) {
    cane_outputs_t outputs;
    cane_status_t status;
    uint8_t bytes[CANE_STATUS_BYTES];
    memset(&outputs, 0, sizeof(outputs));
    outputs.link_state = CANE_LINK_OUTAGE;
    outputs.link = 1;
    expect(cane_status_encode(&outputs, 42, bytes) == 1, "status encodes");
    expect(cane_status_decode(bytes, CANE_STATUS_BYTES, &status) == 1, "status decodes");
    expect(status.link_state == CANE_LINK_OUTAGE, "status link state");
    expect(status.link_on == 1, "status link pin");
    expect(status.hazard_on == 0, "status hazard pin");
    expect(status.last_sequence == 42, "status sequence");
}

static void test_assessment_command_is_sendable(void) {
    cane_input_t input = camera_ready();
    cane_assessment_t assessment;
    cane_command_t command;
    uint8_t packet[CANE_PACKET_BYTES];
    input.has_plane_hit = 1;
    input.plane_hit_meters = 1.25f;
    cane_assess(&input, &assessment);
    cane_command_from_assessment(&assessment, 3, 9, &command);
    expect(cane_command_encode(&command, packet) == 1, "camera assessment encodes");
    expect(packet[1] == CANE_SOURCE_CAMERA, "packet source is camera");
    expect(packet[2] == CANE_HAZARD_NO_CLAIM, "packet hazard is no claim");
    expect(packet[4] == CANE_ROUTE_NONE, "packet route is none");
}

int main(void) {
    test_crc_vector();
    test_camera_makes_no_hazard_or_route();
    test_camera_unavailable_conditions();
    test_plane_hit_bounds();
    test_simulation_channels_stay_separate();
    test_displacement_is_not_velocity();
    test_packet_roundtrip_and_rejection();
    test_watchdog_and_outputs();
    test_patterns();
    test_status_roundtrip();
    test_assessment_command_is_sendable();
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("cane logic tests passed\n");
    return 0;
}
