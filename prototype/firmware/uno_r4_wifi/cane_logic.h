#ifndef CANE_LOGIC_H
#define CANE_LOGIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Shared claim, packet, and actuator-output logic for the supervised demo.
 * Obstacle distance, speed, and time-to-contact are not represented.
 * A plane-hit field is optional context and never authorizes a path.
 */

#define CANE_PACKET_VERSION 1u
#define CANE_PACKET_BYTES 18u
#define CANE_STATUS_BYTES 8u
#define CANE_WATCHDOG_MS 500u
#define CANE_PLANE_CM_NONE 0xFFFFu
#define CANE_PLANE_CM_MAX 2000u

#define CANE_PIN_HAZARD 2
#define CANE_PIN_UNAVAILABLE 3
#define CANE_PIN_ROUTE 4
#define CANE_PIN_LINK 5

#define CANE_HAZARD_PERIOD_MS 200u
#define CANE_HAZARD_ON_MS 100u
#define CANE_UNAVAILABLE_PERIOD_MS 500u
#define CANE_UNAVAILABLE_ON_MS 250u
#define CANE_ROUTE_PERIOD_MS 1000u
#define CANE_ROUTE_ON_MS 200u
#define CANE_LINK_OK_PERIOD_MS 2000u
#define CANE_LINK_OK_ON_MS 80u
#define CANE_LINK_OUT_PERIOD_MS 200u
#define CANE_LINK_OUT_ON_MS 160u

enum {
    CANE_SOURCE_CAMERA = 1,
    CANE_SOURCE_SIMULATION = 2
};

enum {
    CANE_HAZARD_NO_CLAIM = 0,
    CANE_HAZARD_ELEVATED = 1
};

enum {
    CANE_ROUTE_NONE = 0,
    CANE_ROUTE_DEMO_LEFT = 1,
    CANE_ROUTE_DEMO_RIGHT = 2
};

enum {
    CANE_TRACKING_UNSUPPORTED = 0,
    CANE_TRACKING_UNAVAILABLE = 1,
    CANE_TRACKING_LIMITED = 2,
    CANE_TRACKING_NORMAL = 3
};

enum {
    CANE_LINK_HEARTBEAT = 0,
    CANE_LINK_OUTAGE = 1
};

#if defined(__cplusplus)
static_assert(CANE_PACKET_BYTES <= 20, "command packet must fit a 20-byte BLE write");
static_assert(CANE_STATUS_BYTES <= 20, "status packet must fit a default BLE payload");
#else
typedef char cane_packet_fits_default_att[(CANE_PACKET_BYTES <= 20) ? 1 : -1];
typedef char cane_status_fits_default_att[(CANE_STATUS_BYTES <= 20) ? 1 : -1];
#endif

typedef struct {
    int source;
    int tracking;
    int camera_authorized;
    int world_tracking_supported;
    int scene_depth_supported;
    int has_plane_hit;
    float plane_hit_meters;
    int simulation_elevated_hazard;
    int simulation_unavailable;
    int route_cue;
} cane_input_t;

typedef struct {
    int source;
    int hazard;
    int system_unavailable;
    int route_cue;
    int scene_depth_supported;
    int plane_hit_valid;
    uint16_t plane_hit_cm;
    int tracking;
} cane_assessment_t;

typedef struct {
    int source;
    int hazard;
    int system_unavailable;
    int route_cue;
    int tracking;
    int scene_depth_supported;
    uint16_t plane_hit_cm;
    uint32_t sequence;
    uint16_t time_ms_low;
} cane_command_t;

typedef struct {
    uint32_t last_valid_ms;
    int have_valid;
    cane_command_t command;
} cane_link_t;

typedef struct {
    int hazard;
    int unavailable;
    int route;
    int link;
    int link_state;
} cane_outputs_t;

typedef struct {
    int link_state;
    int hazard_on;
    int unavailable_on;
    int route_on;
    int link_on;
    uint32_t last_sequence;
} cane_status_t;

uint16_t cane_crc16(const uint8_t *data, size_t len);

void cane_assess(const cane_input_t *in, cane_assessment_t *out);

int cane_camera_displacement_m(
    float x0,
    float y0,
    float z0,
    float x1,
    float y1,
    float z1,
    int tracking_normal,
    float *meters_out);

void cane_command_from_assessment(
    const cane_assessment_t *assessment,
    uint32_t sequence,
    uint16_t time_ms_low,
    cane_command_t *command);

int cane_command_admissible(const cane_command_t *command);
int cane_command_encode(const cane_command_t *command, uint8_t out[CANE_PACKET_BYTES]);
int cane_command_decode(const uint8_t *bytes, size_t len, cane_command_t *command);

void cane_link_init(cane_link_t *link);
int cane_link_accept(cane_link_t *link, const uint8_t *bytes, size_t len, uint32_t now_ms);
void cane_link_outputs(const cane_link_t *link, uint32_t now_ms, cane_outputs_t *out);

int cane_status_encode(
    const cane_outputs_t *out,
    uint32_t last_sequence,
    uint8_t bytes[CANE_STATUS_BYTES]);
int cane_status_decode(const uint8_t *bytes, size_t len, cane_status_t *status);

int cane_pulse(uint32_t now_ms, uint32_t period_ms, uint32_t on_ms);

#ifdef __cplusplus
}
#endif

#endif
