#include "../include/lora_avr.h"
#include "../include/config.h"
#include "../include/lora_uart_driver.h"
#include <SoftwareSerial.h>

// ── UART transport for Seeed Grove LoRa (factory firmware) ──
static SoftwareSerial s_lora_serial(LORA_RX_PIN, LORA_TX_PIN);
static LoraUartDriver s_radio(s_lora_serial);

static uint8_t s_my_mac[6];
static char s_device_name[16];
static uint16_t s_sequence = 0;
static uint8_t s_slot = 0;
static bool s_paired = false;
static int8_t s_last_rssi = 0;
static uint32_t s_rx_count = 0;
static uint32_t s_tx_count = 0;

static lora_command_callback_t s_command_cb = nullptr;

// ── Frame Builders ──

static uint8_t build_frame(uint8_t msg_type, uint8_t *buf, uint8_t max_len,
                           const uint8_t *payload, uint8_t payload_len) {
    if (max_len < LORA_HEADER_SIZE + payload_len) return 0;

    lora_frame_t *f = (lora_frame_t *)buf;
    f->msg_type = msg_type;
    f->sequence = s_sequence++;
    memcpy(f->sensor_id, s_my_mac, 6);
    f->rssi = 0;
    f->payload_len = payload_len;
    if (payload && payload_len > 0) {
        memcpy(f->payload, payload, payload_len);
    }
    return LORA_HEADER_SIZE + payload_len;
}

// ── Public Functions ──

void lora_init(const uint8_t *my_mac, const char *device_name) {
    memcpy(s_my_mac, my_mac, 6);
    strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
    s_device_name[sizeof(s_device_name) - 1] = '\0';

    if (!s_radio.init(LORA_FREQ, LORA_SF, 125000, LORA_CR, LORA_TX_POWER)) {
        Serial.println("LoRa init failed");
        return;
    }

    Serial.println("LoRa initialized (raw UART bridge)");
}

bool lora_send_frame(const uint8_t *data, uint8_t len) {
    if (!s_radio.send(data, len)) return false;
    s_tx_count++;
    return true;
}

bool lora_send_pair_request(uint8_t sensor_type) {
    uint8_t payload[1 + 16];
    payload[0] = sensor_type;
    memset(payload + 1, 0, 16);
    strncpy((char *)(payload + 1), s_device_name, 15);

    uint8_t buf[LORA_HEADER_SIZE + 17];
    uint8_t len = build_frame(MSG_PAIR_REQUEST, buf, sizeof(buf), payload, 17);
    return lora_send_frame(buf, len);
}

bool lora_send_sensor_data(uint8_t relay_state) {
    uint8_t buf[LORA_HEADER_SIZE + 1];
    uint8_t len = build_frame(MSG_SENSOR_DATA, buf, sizeof(buf), &relay_state, 1);
    return lora_send_frame(buf, len);
}

bool lora_send_heartbeat() {
    uint8_t buf[LORA_HEADER_SIZE];
    uint8_t len = build_frame(MSG_HEARTBEAT, buf, sizeof(buf), nullptr, 0);
    return lora_send_frame(buf, len);
}

void lora_set_command_callback(lora_command_callback_t cb) {
    s_command_cb = cb;
}

void lora_loop() {
    uint8_t buf[255];
    uint8_t len = 0;
    int8_t rssi = 0;

    if (!s_radio.recv(buf, &len, &rssi)) return;
    if (len < LORA_HEADER_SIZE) return;

    s_last_rssi = rssi;
    s_rx_count++;

    const lora_frame_t *frame = (const lora_frame_t *)buf;
    if (memcmp(frame->sensor_id, s_my_mac, 6) != 0) return;

    switch (frame->msg_type) {
        case MSG_PAIR_RESPONSE: {
            if (frame->payload_len >= 1) {
                const lora_pair_response_t *resp = (const lora_pair_response_t *)buf;
                s_slot = resp->assigned_slot;
                s_paired = true;
            }
            break;
        }

        case MSG_COMMAND: {
            if (frame->payload_len >= 1 && s_command_cb) {
                const lora_command_t *cmd = (const lora_command_t *)buf;
                s_command_cb(s_slot, cmd->command);
            }
            break;
        }

        case MSG_NAK:
            break;
    }
}

bool lora_is_paired() { return s_paired; }
uint8_t lora_get_slot() { return s_slot; }
int8_t lora_get_last_rssi() { return s_last_rssi; }
uint32_t lora_rx_count() { return s_rx_count; }
uint32_t lora_tx_count() { return s_tx_count; }
