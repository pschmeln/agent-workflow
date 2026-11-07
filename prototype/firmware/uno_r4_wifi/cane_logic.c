#include "cane_logic.h"

#include <math.h>

static int finite_f(float value) {
    return value == value && value < 1000000.0f && value > -1000000.0f;
}

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void write_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)((value >> 8) & 0xFFu);
    bytes[2] = (uint8_t)((value >> 16) & 0xFFu);
    bytes[3] = (uint8_t)((value >> 24) & 0xFFu);
}

uint16_t cane_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    size_t i;
    int bit;
    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000u) != 0) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

static void clear_plane(cane_assessment_t *out) {
    out->plane_hit_valid = 0;
    out->plane_hit_cm = CANE_PLANE_CM_NONE;
}

static void record_plane(const cane_input_t *in, cane_assessment_t *out) {
    float centimeters;
    if (!in->has_plane_hit || in->tracking != CANE_TRACKING_NORMAL || !finite_f(in->plane_hit_meters)) {
        clear_plane(out);
        return;
    }
    if (!(in->plane_hit_meters > 0.0f) || in->plane_hit_meters > (CANE_PLANE_CM_MAX / 100.0f)) {
        clear_plane(out);
        return;
    }
    centimeters = in->plane_hit_meters * 100.0f;
    if (!finite_f(centimeters)) {
        clear_plane(out);
        return;
    }
    out->plane_hit_valid = 1;
    out->plane_hit_cm = (uint16_t)(centimeters + 0.5f);
    if (out->plane_hit_cm == 0 || out->plane_hit_cm > CANE_PLANE_CM_MAX) {
        clear_plane(out);
    }
}

void cane_assess(const cane_input_t *in, cane_assessment_t *out) {
    out->source = in->source;
    out->tracking = in->tracking;
    out->scene_depth_supported = in->scene_depth_supported ? 1 : 0;
    out->hazard = CANE_HAZARD_NO_CLAIM;
    out->system_unavailable = 1;
    out->route_cue = CANE_ROUTE_NONE;
    record_plane(in, out);

    if (in->source == CANE_SOURCE_CAMERA) {
        int pipeline_ready = in->camera_authorized && in->world_tracking_supported &&
                             in->tracking == CANE_TRACKING_NORMAL;
        out->system_unavailable = pipeline_ready ? 0 : 1;
        out->hazard = CANE_HAZARD_NO_CLAIM;
        out->route_cue = CANE_ROUTE_NONE;
        return;
    }

    if (in->source == CANE_SOURCE_SIMULATION) {
        out->hazard = in->simulation_elevated_hazard ? CANE_HAZARD_ELEVATED : CANE_HAZARD_NO_CLAIM;
        out->system_unavailable = in->simulation_unavailable ? 1 : 0;
        if (in->route_cue == CANE_ROUTE_DEMO_LEFT || in->route_cue == CANE_ROUTE_DEMO_RIGHT) {
            out->route_cue = in->route_cue;
        }
    }
}

int cane_camera_displacement_m(
    float x0,
    float y0,
    float z0,
    float x1,
    float y1,
    float z1,
    int tracking_normal,
    float *meters_out) {
    float dx;
    float dy;
    float dz;
    float squared;
    if (!tracking_normal || meters_out == NULL) {
        return 0;
    }
    if (!finite_f(x0) || !finite_f(y0) || !finite_f(z0) || !finite_f(x1) || !finite_f(y1) || !finite_f(z1)) {
        return 0;
    }
    dx = x1 - x0;
    dy = y1 - y0;
    dz = z1 - z0;
    if (!finite_f(dx) || !finite_f(dy) || !finite_f(dz)) {
        return 0;
    }
    squared = (dx * dx) + (dy * dy) + (dz * dz);
    if (!finite_f(squared) || squared < 0.0f) {
        return 0;
    }
    *meters_out = sqrtf(squared);
    return finite_f(*meters_out);
}

void cane_command_from_assessment(
    const cane_assessment_t *assessment,
    uint32_t sequence,
    uint16_t time_ms_low,
    cane_command_t *command) {
    command->source = assessment->source;
    command->hazard = assessment->hazard;
    command->system_unavailable = assessment->system_unavailable ? 1 : 0;
    command->route_cue = assessment->route_cue;
    command->tracking = assessment->tracking;
    command->scene_depth_supported = assessment->scene_depth_supported ? 1 : 0;
    command->plane_hit_cm = assessment->plane_hit_valid ? assessment->plane_hit_cm : CANE_PLANE_CM_NONE;
    command->sequence = sequence;
    command->time_ms_low = time_ms_low;
}

int cane_command_admissible(const cane_command_t *command) {
    if (command->source != CANE_SOURCE_CAMERA && command->source != CANE_SOURCE_SIMULATION) {
        return 0;
    }
    if (command->hazard != CANE_HAZARD_NO_CLAIM && command->hazard != CANE_HAZARD_ELEVATED) {
        return 0;
    }
    if (command->system_unavailable != 0 && command->system_unavailable != 1) {
        return 0;
    }
    if (command->route_cue != CANE_ROUTE_NONE && command->route_cue != CANE_ROUTE_DEMO_LEFT &&
        command->route_cue != CANE_ROUTE_DEMO_RIGHT) {
        return 0;
    }
    if (command->tracking < CANE_TRACKING_UNSUPPORTED || command->tracking > CANE_TRACKING_NORMAL) {
        return 0;
    }
    if (command->source == CANE_SOURCE_CAMERA) {
        if (command->hazard != CANE_HAZARD_NO_CLAIM || command->route_cue != CANE_ROUTE_NONE) {
            return 0;
        }
    }
    return 1;
}

int cane_command_encode(const cane_command_t *command, uint8_t out[CANE_PACKET_BYTES]) {
    uint16_t crc;
    if (!cane_command_admissible(command)) {
        return 0;
    }
    out[0] = CANE_PACKET_VERSION;
    out[1] = (uint8_t)command->source;
    out[2] = (uint8_t)command->hazard;
    out[3] = (uint8_t)command->system_unavailable;
    out[4] = (uint8_t)command->route_cue;
    out[5] = (uint8_t)command->tracking;
    out[6] = command->scene_depth_supported ? 1u : 0u;
    out[7] = 0;
    write_le32(&out[8], command->sequence);
    write_le16(&out[12], command->time_ms_low);
    write_le16(&out[14], command->plane_hit_cm);
    crc = cane_crc16(out, 16);
    write_le16(&out[16], crc);
    return 1;
}

int cane_command_decode(const uint8_t *bytes, size_t len, cane_command_t *command) {
    uint16_t expected;
    uint16_t actual;
    if (len != CANE_PACKET_BYTES || bytes[0] != CANE_PACKET_VERSION || bytes[7] != 0) {
        return 0;
    }
    expected = cane_crc16(bytes, 16);
    actual = read_le16(&bytes[16]);
    if (expected != actual) {
        return 0;
    }
    command->source = bytes[1];
    command->hazard = bytes[2];
    command->system_unavailable = bytes[3];
    command->route_cue = bytes[4];
    command->tracking = bytes[5];
    command->scene_depth_supported = bytes[6] ? 1 : 0;
    command->sequence = read_le32(&bytes[8]);
    command->time_ms_low = read_le16(&bytes[12]);
    command->plane_hit_cm = read_le16(&bytes[14]);
    return cane_command_admissible(command);
}

void cane_link_init(cane_link_t *link) {
    link->last_valid_ms = 0;
    link->have_valid = 0;
    link->command.source = CANE_SOURCE_CAMERA;
    link->command.hazard = CANE_HAZARD_NO_CLAIM;
    link->command.system_unavailable = 1;
    link->command.route_cue = CANE_ROUTE_NONE;
    link->command.tracking = CANE_TRACKING_UNAVAILABLE;
    link->command.scene_depth_supported = 0;
    link->command.plane_hit_cm = CANE_PLANE_CM_NONE;
    link->command.sequence = 0;
    link->command.time_ms_low = 0;
}

int cane_link_accept(cane_link_t *link, const uint8_t *bytes, size_t len, uint32_t now_ms) {
    cane_command_t command;
    if (!cane_command_decode(bytes, len, &command)) {
        return 0;
    }
    link->command = command;
    link->have_valid = 1;
    link->last_valid_ms = now_ms;
    return 1;
}

int cane_pulse(uint32_t now_ms, uint32_t period_ms, uint32_t on_ms) {
    if (period_ms == 0 || on_ms == 0) {
        return 0;
    }
    return (now_ms % period_ms) < on_ms;
}

static int link_outage(const cane_link_t *link, uint32_t now_ms) {
    if (!link->have_valid) {
        return 1;
    }
    return (uint32_t)(now_ms - link->last_valid_ms) > CANE_WATCHDOG_MS;
}

void cane_link_outputs(const cane_link_t *link, uint32_t now_ms, cane_outputs_t *out) {
    int outage = link_outage(link, now_ms);
    out->link_state = outage ? CANE_LINK_OUTAGE : CANE_LINK_HEARTBEAT;
    out->link = cane_pulse(
        now_ms,
        outage ? CANE_LINK_OUT_PERIOD_MS : CANE_LINK_OK_PERIOD_MS,
        outage ? CANE_LINK_OUT_ON_MS : CANE_LINK_OK_ON_MS);
    if (outage) {
        out->hazard = 0;
        out->unavailable = 0;
        out->route = 0;
        return;
    }
    out->hazard = link->command.hazard == CANE_HAZARD_ELEVATED &&
                  cane_pulse(now_ms, CANE_HAZARD_PERIOD_MS, CANE_HAZARD_ON_MS);
    out->unavailable = link->command.system_unavailable &&
                       cane_pulse(now_ms, CANE_UNAVAILABLE_PERIOD_MS, CANE_UNAVAILABLE_ON_MS);
    out->route = link->command.route_cue != CANE_ROUTE_NONE &&
                 cane_pulse(now_ms, CANE_ROUTE_PERIOD_MS, CANE_ROUTE_ON_MS);
}

int cane_status_encode(
    const cane_outputs_t *out,
    uint32_t last_sequence,
    uint8_t bytes[CANE_STATUS_BYTES]) {
    bytes[0] = CANE_PACKET_VERSION;
    bytes[1] = (uint8_t)out->link_state;
    bytes[2] = (uint8_t)((out->hazard ? 1u : 0u) | (out->unavailable ? 2u : 0u) | (out->route ? 4u : 0u) |
                         (out->link ? 8u : 0u));
    bytes[3] = 0;
    write_le32(&bytes[4], last_sequence);
    return 1;
}

int cane_status_decode(const uint8_t *bytes, size_t len, cane_status_t *status) {
    if (len != CANE_STATUS_BYTES || bytes[0] != CANE_PACKET_VERSION || bytes[3] != 0) {
        return 0;
    }
    if (bytes[1] != CANE_LINK_HEARTBEAT && bytes[1] != CANE_LINK_OUTAGE) {
        return 0;
    }
    status->link_state = bytes[1];
    status->hazard_on = (bytes[2] & 1u) != 0;
    status->unavailable_on = (bytes[2] & 2u) != 0;
    status->route_on = (bytes[2] & 4u) != 0;
    status->link_on = (bytes[2] & 8u) != 0;
    status->last_sequence = read_le32(&bytes[4]);
    return 1;
}
