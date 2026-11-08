/*
 * UNO R4 WiFi peripheral for the supervised cane demo.
 * The phone is the BLE central. This board is the peripheral.
 * Wi-Fi is not started. Arduino documents that the ESP32-S3 antenna
 * is shared, so this sketch uses Bluetooth LE only.
 *
 * D2 is the left Grove socket and D4 is the right Grove socket.
 * D3 is an internal status output. D5 is the link indicator.
 * Do not put a third buzzer on D3 or D5.
 * Confirm the Grove Buzzer module's SIG pin is a logic input before
 * connecting it. Do not power a bare motor from a GPIO.
 * See docs/WEB_DEMO.md.
 */

#include <ArduinoBLE.h>

#include "cane_logic.h"

static BLEService caneService("8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1001");
static BLECharacteristic commandCharacteristic(
    "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1002",
    static_cast<uint16_t>(BLEWrite | BLEWriteWithoutResponse),
    CANE_PACKET_BYTES);
static BLECharacteristic statusCharacteristic(
    "8f3a1c20-7b6e-4d5a-9c11-6e2f0a9b1003",
    static_cast<uint16_t>(BLERead | BLENotify),
    CANE_STATUS_BYTES);

static cane_link_t link;
static cane_outputs_t previous;
static int have_previous = 0;
static int ble_ready = 0;
static uint32_t last_status_ms = 0;

static void set_pin_low(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

static const char *source_name(int source) {
    if (source == CANE_SOURCE_CAMERA) {
        return "camera";
    }
    if (source == CANE_SOURCE_SIMULATION) {
        return "simulation";
    }
    if (source == CANE_SOURCE_IMAGE_HEURISTIC) {
        return "image-heuristic";
    }
    return "unknown";
}

static void print_outputs(uint32_t now_ms, const cane_outputs_t *out) {
    Serial.print("ms=");
    Serial.print(now_ms);
    Serial.print(" source=");
    Serial.print(source_name(link.command.source));
    Serial.print(" link=");
    Serial.print(out->link_state == CANE_LINK_OUTAGE ? "outage" : "heartbeat");
    Serial.print(" pins left=");
    Serial.print(out->left);
    Serial.print(" right=");
    Serial.print(out->right);
    Serial.print(" status=");
    Serial.print(out->status);
    Serial.print(" linkpin=");
    Serial.println(out->link);
}

static void apply_outputs(uint32_t now_ms) {
    cane_outputs_t out;
    cane_link_outputs(&link, now_ms, &out);
    digitalWrite(CANE_PIN_LEFT, out.left ? HIGH : LOW);
    digitalWrite(CANE_PIN_STATUS, out.status ? HIGH : LOW);
    digitalWrite(CANE_PIN_RIGHT, out.right ? HIGH : LOW);
    digitalWrite(CANE_PIN_LINK, out.link ? HIGH : LOW);
    if (!have_previous || out.left != previous.left || out.right != previous.right ||
        out.status != previous.status || out.link_state != previous.link_state) {
        print_outputs(now_ms, &out);
        previous = out;
        have_previous = 1;
    }
    if ((uint32_t)(now_ms - last_status_ms) >= 200u) {
        uint8_t status[CANE_STATUS_BYTES];
        uint32_t sequence = link.have_valid ? link.command.sequence : 0u;
        cane_status_encode(&out, sequence, status);
        statusCharacteristic.writeValue(status, static_cast<int>(CANE_STATUS_BYTES));
        last_status_ms = now_ms;
    }
}

void setup() {
    set_pin_low(CANE_PIN_LEFT);
    set_pin_low(CANE_PIN_STATUS);
    set_pin_low(CANE_PIN_RIGHT);
    set_pin_low(CANE_PIN_LINK);
    cane_link_init(&link);
    Serial.begin(115200);
    Serial.println("Cane demo controller");
    Serial.println("D2 left buzzer, D4 right buzzer, D3 status logic, D5 link logic.");
    Serial.println("No packet yet, so the link output uses the outage pattern.");

    if (!BLE.begin()) {
        Serial.println("BLE.begin failed. Actuator enables stay in the link-outage pattern.");
        return;
    }
    BLE.setLocalName("CaneDemo");
    BLE.setAdvertisedService(caneService);
    caneService.addCharacteristic(commandCharacteristic);
    caneService.addCharacteristic(statusCharacteristic);
    BLE.addService(caneService);
    BLE.advertise();
    ble_ready = 1;
    Serial.println("Advertising CaneDemo as a BLE peripheral.");
}

void loop() {
    uint32_t now_ms = millis();
    if (ble_ready) {
        BLE.poll();
        if (commandCharacteristic.written()) {
            const uint8_t *incoming = commandCharacteristic.value();
            int length = commandCharacteristic.valueLength();
            int accepted = 0;
            if (incoming != nullptr && length > 0) {
                accepted = cane_link_accept(&link, incoming, (size_t)length, now_ms);
            }
            if (!accepted) {
                Serial.print("ms=");
                Serial.print(now_ms);
                Serial.println(" rejected packet");
            } else {
                Serial.print("ms=");
                Serial.print(now_ms);
                Serial.print(" accepted source=");
                Serial.print(source_name(link.command.source));
                Serial.print(" seq=");
                Serial.println(link.command.sequence);
            }
        }
    }
    apply_outputs(now_ms);
}
