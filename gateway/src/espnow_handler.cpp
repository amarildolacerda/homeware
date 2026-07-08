#include "espnow_handler.h"
#include "config.h"
#include "sensor_registry.h"
#include "mqtt_client.h"
#include <espnow.h>
#include <ESP8266WiFi.h>
#include "log_buffer.h"
#include <Arduino.h>
#include "console.h"

static bool s_pairing_mode = false;
static unsigned long s_pairing_start = 0;
static uint8_t s_gateway_mac[6];
static unsigned long s_last_heartbeat = 0;
static unsigned long s_rx_count = 0;
static unsigned long s_ack_count = 0;
static unsigned long s_crc_errors = 0;

#define PENDING_PAIR_MAX 5
typedef struct {
    bool active;
    uint8_t mac[6];
    uint8_t sensor_type;
    uint16_t sequence;
    char name[32];
} pending_pair_t;
static pending_pair_t s_pending_pairs[PENDING_PAIR_MAX];

#define PENDING_STATE_MAX 5
static uint8_t s_pending_state_slots[PENDING_STATE_MAX];
static int s_pending_state_head = 0;
static int s_pending_state_tail = 0;

static void queue_bridge_state(int slot) {
    int next = (s_pending_state_head + 1) % PENDING_STATE_MAX;
    if (next == s_pending_state_tail) return;
    s_pending_state_slots[s_pending_state_head] = slot;
    s_pending_state_head = next;
}

static void send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot);
static void send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot);

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
    if (!data || len < 2) {
        s_crc_errors++;
        return;
    }

    s_rx_count++;
    uint8_t msg_type = data[0];
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] RX msg_type=%d len=%d from=%s pairing=%d\n",
                  msg_type, len, mac_str, s_pairing_mode);

    switch (msg_type) {
        case ESPNOW_MSG_PAIR_REQUEST: {
            if (len < sizeof(espnow_pair_request_t)) { s_crc_errors++; return; }
            espnow_pair_request_t *req = (espnow_pair_request_t*)data;
            char sensor_mac_str[18];
            mac_to_str(req->sensor_mac, sensor_mac_str, sizeof(sensor_mac_str));

            int existing_slot = sensor_registry_find_by_mac(req->sensor_mac);
            if (existing_slot >= 0) {
                send_pair_response(mac, req->sequence, existing_slot);
                console.printf("[ESP-NOW] Re-paired known sensor %s slot=%d\n", sensor_mac_str, existing_slot);
                return;
            }

            if (!s_pairing_mode) { console.printf("[ESP-NOW] New pair request ignored (not pairing)\n"); return; }

            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (s_pending_pairs[i].active && mac_equal(s_pending_pairs[i].mac, req->sensor_mac)) {
                    console.printf("[ESP-NOW] Pair request already pending for %s\n", sensor_mac_str);
                    return;
                }
            }

            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (!s_pending_pairs[i].active) {
                    mac_copy(s_pending_pairs[i].mac, req->sensor_mac);
                    s_pending_pairs[i].sensor_type = req->sensor_type;
                    s_pending_pairs[i].sequence = req->sequence;
                    strncpy(s_pending_pairs[i].name, req->device_name, sizeof(s_pending_pairs[i].name) - 1);
                    s_pending_pairs[i].name[sizeof(s_pending_pairs[i].name) - 1] = '\0';
                    s_pending_pairs[i].active = true;
                    console.printf("[ESP-NOW] Pair request queued from %s type=%d seq=%d slot=%d\n",
                                  sensor_mac_str, req->sensor_type, req->sequence, i);
                    break;
                }
            }

            break;
        }

        case ESPNOW_MSG_SENSOR_DATA:
        case ESPNOW_MSG_HEARTBEAT: {
            if (len < ESPNOW_HEADER_FIXED_SIZE) { s_crc_errors++; return; }
            espnow_header_t *hdr = (espnow_header_t*)data;

            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { s_crc_errors++; return; }
            if (len < ESPNOW_HEADER_FIXED_SIZE + hdr->payload_len) { s_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            {
                char sender_str[18], sensor_str[18];
                mac_to_str(mac, sender_str, sizeof(sender_str));
                mac_to_str(hdr->sensor_mac, sensor_str, sizeof(sensor_str));
                console.printf("[ESP-NOW] %s: sender=%s sensor=%s type=%d len=%d slot=%d\n",
                    msg_type == ESPNOW_MSG_SENSOR_DATA ? "DATA" : "HB",
                    sender_str, sensor_str, hdr->sensor_type, len, slot);
            }

            if (msg_type == ESPNOW_MSG_SENSOR_DATA) {
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_DENIED, 0xFF);
                    return;
                }
                sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
                send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                log_add("info", "Dados recebidos slot %d seq %d", slot, hdr->sequence);
                s_ack_count++;
                queue_bridge_state(slot);
            } else {
                if (slot >= 0) {
                    sensor_registry_get(slot)->last_seen = millis();
                    sensor_registry_get(slot)->online = true;
                    send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                }
            }
            break;
        }

        default:
            s_crc_errors++;
            break;
    }
}

void send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot) {
    espnow_ack_t ack = {
        .msg_type = ESPNOW_MSG_ACK,
        .sequence = sequence,
        .sensor_mac = {0},
        .status = status,
        .assigned_slot = slot
    };
    mac_copy(ack.sensor_mac, mac);

    esp_now_del_peer((uint8_t*)mac);
    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    int ret = esp_now_send((uint8_t*)mac, (uint8_t*)&ack, sizeof(ack));
    if (ret != 0) {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[ESP-NOW] ACK send failed to %s ret=%d\n", mac_str, ret);
    } else {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[ESP-NOW] ACK sent to %s seq=%d status=%d slot=%d\n", mac_str, sequence, status, slot);
    }
}

void send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot) {
    espnow_pair_response_t resp = {
        .msg_type = ESPNOW_MSG_PAIR_RESPONSE,
        .sequence = sequence,
        .sensor_mac = {0},
        .gateway_mac = {0},
        .status = PAIR_STATUS_OK,
        .assigned_slot = slot
    };
    mac_copy(resp.sensor_mac, mac);
    mac_copy(resp.gateway_mac, s_gateway_mac);

    esp_now_del_peer((uint8_t*)mac);
    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    int ret = esp_now_send((uint8_t*)mac, (uint8_t*)&resp, sizeof(resp));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] Pair response sent to %s slot=%d seq=%d ret=%d\n", mac_str, slot, sequence, ret);
}

bool espnow_handler_init() {
    memset(s_pending_pairs, 0, sizeof(s_pending_pairs));
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(s_gateway_mac);
    
    if (esp_now_init() != 0) {
        console.println("[ESP-NOW] Init failed");
        return false;
    }
    
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_recv_cb(espnow_recv_cb);
    
    console.printf("[ESP-NOW] Initialized, MAC: %02X:%02X:%02X:%02X:%02X:%02X WiFi ch=%d\n",
                  s_gateway_mac[0], s_gateway_mac[1], s_gateway_mac[2],
                  s_gateway_mac[3], s_gateway_mac[4], s_gateway_mac[5],
                  WiFi.channel());
    return true;
}

void espnow_handler_loop() {
    if (s_pairing_mode && millis() - s_pairing_start > PAIRING_WINDOW_MS) {
        s_pairing_mode = false;
        digitalWrite(STATUS_LED_GPIO, HIGH);
        console.println("[ESP-NOW] Pairing mode timeout");
    }

    for (int i = 0; i < PENDING_PAIR_MAX; i++) {
        if (!s_pending_pairs[i].active) continue;

        s_pending_pairs[i].active = false;
        int free_slot = sensor_registry_find_free_slot();

        if (free_slot < 0) {
            send_ack(s_pending_pairs[i].mac, s_pending_pairs[i].sequence, PAIR_STATUS_FULL, 0xFF);
            continue;
        }

        if (!sensor_registry_add(s_pending_pairs[i].mac, s_pending_pairs[i].sensor_type,
                                 free_slot, s_pending_pairs[i].name)) {
            int existing = sensor_registry_find_by_mac(s_pending_pairs[i].mac);
            if (existing >= 0)
                send_pair_response(s_pending_pairs[i].mac, s_pending_pairs[i].sequence, existing);
            continue;
        }
        send_pair_response(s_pending_pairs[i].mac, s_pending_pairs[i].sequence, free_slot);
        {
            char mac_str[18];
            mac_to_str(s_pending_pairs[i].mac, mac_str, sizeof(mac_str));
            log_add("info", "Sensor %s pareado slot %d", mac_str, free_slot);
        }
        if (mqtt_client_is_connected())
            mqtt_client_publish_discovery(sensor_registry_get(free_slot));

        console.printf("[ESP-NOW] Paired sensor slot %d: %02X:%02X:%02X:%02X:%02X:%02X type=%d\n",
                      free_slot,
                      s_pending_pairs[i].mac[0], s_pending_pairs[i].mac[1],
                      s_pending_pairs[i].mac[2], s_pending_pairs[i].mac[3],
                      s_pending_pairs[i].mac[4], s_pending_pairs[i].mac[5],
                      s_pending_pairs[i].sensor_type);
    }

    while (s_pending_state_tail != s_pending_state_head) {
        int slot = s_pending_state_slots[s_pending_state_tail];
        s_pending_state_tail = (s_pending_state_tail + 1) % PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected())
            mqtt_client_publish_state(s);
    }

    if (millis() - s_last_heartbeat > HEARTBEAT_INTERVAL_MS) {
        s_last_heartbeat = millis();

        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired) {
                unsigned long elapsed = millis() - s->last_seen;
                if (s->online && elapsed > SENSOR_TIMEOUT_MS) {
                    s->online = false;
                    log_add("warn", "Sensor slot %d offline", i);
                    console.printf("[ESP-NOW] Sensor slot %d offline (last seen %lu ms ago)\n", i, elapsed);
                }
            }
        }
    }
}

bool espnow_start_pairing() {
    if (sensor_registry_count_paired() >= MAX_VIRTUAL_SENSORS) {
        console.println("[ESP-NOW] Max sensors reached");
        return false;
    }
    s_pairing_mode = true;
    s_pairing_start = millis();
    digitalWrite(STATUS_LED_GPIO, LOW);
    console.printf("[ESP-NOW] Pairing mode started (%us)\n", PAIRING_WINDOW_MS / 1000);
    return true;
}

void espnow_stop_pairing() {
    s_pairing_mode = false;
    digitalWrite(STATUS_LED_GPIO, HIGH);
    console.println("[ESP-NOW] Pairing mode stopped");
}

bool espnow_is_pairing() {
    return s_pairing_mode;
}

unsigned long espnow_pairing_remaining_ms() {
    if (!s_pairing_mode) return 0;
    unsigned long elapsed = millis() - s_pairing_start;
    if (elapsed >= PAIRING_WINDOW_MS) return 0;
    return PAIRING_WINDOW_MS - elapsed;
}

bool espnow_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    espnow_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = ESPNOW_MSG_COMMAND;
    cmd.sequence = 0;
    mac_copy(cmd.target_mac, mac);
    cmd.command = state;

    esp_now_del_peer((uint8_t*)mac);
    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    int ret = esp_now_send((uint8_t*)mac, (uint8_t*)&cmd, sizeof(cmd));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    if (ret == 0) {
        console.printf("[ESP-NOW] Command sent to %s slot=%d state=%d\n", mac_str, slot, state);
        return true;
    }
    console.printf("[ESP-NOW] Command send failed to %s ret=%d\n", mac_str, ret);
    return false;
}

unsigned long espnow_get_rx_count() { return s_rx_count; }
unsigned long espnow_get_ack_count() { return s_ack_count; }
unsigned long espnow_get_crc_errors() { return s_crc_errors; }
uint8_t* espnow_get_gateway_mac() { return s_gateway_mac; }