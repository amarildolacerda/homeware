#include "espnow_handler.h"
#include "config.h"
#include "sensor_registry.h"
#include "mqtt_client.h"
#include "platform.h"
#include "log_buffer.h"
#include <Arduino.h>
#include "common_console.h"

static bool s_pairing_mode = false;
static unsigned long s_pairing_start = 0;
static uint8_t s_gateway_mac[6];
static unsigned long s_last_heartbeat = 0;
static unsigned long s_rx_count = 0;
static unsigned long s_ack_count = 0;
static unsigned long s_crc_errors = 0;

static const uint8_t s_bcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ESP-NOW delivery uses BROADCAST (all clients receive and filter by
   sensor_mac/target_mac). Validated with QuickESPNow (qgw/qclient, both STA on
   the same AP): ESP8266->ESP32 unicast is silently dropped by WiFi/ESP-NOW
   coexistence, while broadcast works in both directions. See AGENTS.md rule 18. */


#define PENDING_PAIR_MAX 5
typedef struct {
    bool active;
    uint8_t mac[6];
    uint8_t sensor_type;
    uint16_t sequence;
    uint8_t client_chip;
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
static void send_gw_announce(const uint8_t *mac);

#ifdef ESP32
extern "C" void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
#else
extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
#endif
    if (!data || len < 1) {
        s_crc_errors++;
        console.printf("Errro crc: %d",s_crc_errors);
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
            console.println("ESPNOW_MSG_PAIR_REQUEST");
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

         // test
         //   if (!s_pairing_mode) { console.printf("[ESP-NOW] New pair request ignored (not pairing)\n"); return; }

            /* Pairing flag check (SPEC_ESPNOW §6) */
            {
                EEPROM.begin(EEPROM_SIZE);
                bool pairing_required = EEPROM.read(EEPROM_PAIRING_EN_OFFSET) == 1;
                EEPROM.end();
                if (pairing_required && !s_pairing_mode) {
                    console.printf("[ESP-NOW] Pair request ignored (pairing disabled, window closed)\n");
                    espnow_nak_t nak;
                    memset(&nak, 0, sizeof(nak));
                    nak.msg_type = ESPNOW_MSG_NAK;
                    nak.sequence = req->sequence;
                    mac_copy(nak.target_mac, req->sensor_mac);
                    nak.reason = NAK_REASON_PAIRING_DISABLED;
                    esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
                    return;
                }
            }

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
                    s_pending_pairs[i].client_chip = req->client_chip;
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
            console.println("ESPNOW_MSG_SENSOR_DATA");
        case ESPNOW_MSG_HEARTBEAT: {
            console.println("ESPNOW_MSG_HEARTBEAT");
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

        case ESPNOW_MSG_GW_DISCOVER: {
            
            console.println("ESPNOW_MSG_GW_DISCOVER");
                char from_str[18];
            mac_to_str(mac, from_str, sizeof(from_str));
            console.printf("[ESP-NOW] GW_DISCOVER from %s\n", from_str);

            int slot = sensor_registry_find_by_mac(mac);
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot >= 0) {
                    sensor_registry_add(mac, SENSOR_TYPE_REPEATER, slot, "Repeater", HW_CHIP_ESP8266);
                    console.printf("[ESP-NOW] Repeater registered on GW_DISCOVER slot=%d\n", slot);
                }
            } else {
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s && s->type != SENSOR_TYPE_REPEATER) {
                    s->type = SENSOR_TYPE_REPEATER;
                    strncpy(s->name, "Repeater", sizeof(s->name) - 1);
                    s->name[sizeof(s->name) - 1] = '\0';
                    sensor_registry_save();
                }
            }

            send_gw_announce(mac);
            break;
        }

        case ESPNOW_MSG_REPEATER_STATUS: {
            console.println("ESPNOW_MSG_REPEATER_STATUS");
            if (len < ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t)) { s_crc_errors++; return; }
            espnow_header_t *hdr = (espnow_header_t*)data;

            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { s_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            char sender_str[18], sensor_str[18];
            mac_to_str(mac, sender_str, sizeof(sender_str));
            mac_to_str(hdr->sensor_mac, sensor_str, sizeof(sensor_str));
            console.printf("[ESP-NOW] REPEATER_STATUS: sender=%s sensor=%s slot=%d\n",
                sender_str, sensor_str, slot);

            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot < 0) {
                    console.printf("[ESP-NOW] Repeater rejected: registry full\n");
                    send_ack(mac, hdr->sequence, PAIR_STATUS_FULL, 0xFF);
                    return;
                }
                sensor_registry_add(hdr->sensor_mac, hdr->sensor_type, slot, "Repeater", HW_CHIP_ESP8266);
            } else {
                // A device may change role (e.g. was a light, now a repeater).
                // Re-type the existing slot so its state is parsed as a repeater.
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s && s->type != SENSOR_TYPE_REPEATER) {
                    s->type = SENSOR_TYPE_REPEATER;
                    strncpy(s->name, "Repeater", sizeof(s->name) - 1);
                    s->name[sizeof(s->name) - 1] = '\0';
                    sensor_registry_save();
                    console.printf("[ESP-NOW] Slot %d retyped to REPEATER (%s)\n",
                        slot, sensor_str);
                }
            }

            sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
            send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
            log_add("info", "Repeater status slot %d seq %d", slot, hdr->sequence);
            s_ack_count++;
            queue_bridge_state(slot);
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

    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&ack, sizeof(ack));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    if (ret == 0)
        console.printf("[ESP-NOW] ACK sent (broadcast) to %s seq=%d status=%d slot=%d\n", mac_str, sequence, status, slot);
    else
        console.printf("[ESP-NOW] ACK send failed to %s ret=%d\n", mac_str, ret);
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

    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&resp, sizeof(resp));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] Pair response sent (broadcast) to %s slot=%d seq=%d ret=%d\n", mac_str, slot, sequence, ret);
}

static void send_gw_announce(const uint8_t *mac) {
    espnow_gw_announce_t ann = {
        .msg_type = ESPNOW_MSG_GW_ANNOUNCE,
        .gateway_mac = {0},
        .fw_version = {0}
    };
    mac_copy(ann.gateway_mac, s_gateway_mac);
    strncpy((char*)ann.fw_version, FW_VERSION, sizeof(ann.fw_version));

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper(s_bcast_addr, ch);
    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&ann, sizeof(ann));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] GW_ANNOUNCE sent (broadcast) to %s gw=%s ret=%d\n", mac_str,
                   FW_VERSION, ret);
}

void espnow_announce() {
    espnow_gw_announce_t ann = {
        .msg_type = ESPNOW_MSG_GW_ANNOUNCE,
        .gateway_mac = {0},
        .fw_version = {0}
    };
    mac_copy(ann.gateway_mac, s_gateway_mac);
    strncpy((char*)ann.fw_version, FW_VERSION, sizeof(ann.fw_version));

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper(s_bcast_addr, ch);
    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&ann, sizeof(ann));
    console.printf("[ESP-NOW] GW_ANNOUNCE broadcast gw=%s ret=%d\n", FW_VERSION, ret);
}

bool espnow_handler_init() {
    memset(s_pending_pairs, 0, sizeof(s_pending_pairs));
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(s_gateway_mac);
    
    if (esp_now_init() != 0) {
        console.println("[ESP-NOW] Init failed");
        return false;
    }
    
#if !defined(ARDUINO_ARCH_ESP32)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#endif
    esp_now_register_recv_cb(espnow_recv_cb);

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espnow_add_peer_wrapper(broadcast_mac, WiFi.channel());
    
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
                                 free_slot, s_pending_pairs[i].name, s_pending_pairs[i].client_chip)) {
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

    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&cmd, sizeof(cmd));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    if (ret == 0) {
        console.printf("[ESP-NOW] Command sent (broadcast) to %s slot=%d state=%d\n", mac_str, slot, state);
        return true;
    }
    console.printf("[ESP-NOW] Command send failed to %s ret=%d\n", mac_str, ret);
    return false;
}

bool espnow_send_restart(const uint8_t *mac) {
    espnow_restart_t rst;
    memset(&rst, 0, sizeof(rst));
    rst.msg_type = ESPNOW_MSG_RESTART;
    rst.sequence = 0;
    mac_copy(rst.target_mac, mac);

    int ret = esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&rst, sizeof(rst));
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    if (ret == 0) {
        console.printf("[ESP-NOW] Restart sent (broadcast) to %s\n", mac_str);
        return true;
    }
    console.printf("[ESP-NOW] Restart send failed to %s ret=%d\n", mac_str, ret);
    return false;
}

static uint8_t s_time_sync_sequence = 0;
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void espnow_broadcast_time_sync(uint32_t epoch_seconds) {
    espnow_time_sync_t ts;
    memset(&ts, 0, sizeof(ts));
    ts.msg_type = ESPNOW_MSG_TIME_SYNC;
    ts.sequence = s_time_sync_sequence++;
    mac_copy(ts.gateway_mac, s_gateway_mac);
    ts.epoch_seconds = epoch_seconds;

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper((uint8_t*)s_broadcast_mac, ch);
    int ret = esp_now_send((uint8_t*)s_broadcast_mac, (uint8_t*)&ts, sizeof(ts));
    if (ret == 0) {
        console.printf("[ESP-NOW] Time sync broadcast sent (epoch=%u seq=%d)\n",
                      (unsigned)epoch_seconds, ts.sequence);
    } else {
        console.printf("[ESP-NOW] Time sync broadcast FAILED ret=%d\n", ret);
    }
}

unsigned long espnow_get_rx_count() { return s_rx_count; }
unsigned long espnow_get_ack_count() { return s_ack_count; }
unsigned long espnow_get_crc_errors() { return s_crc_errors; }
uint8_t* espnow_get_gateway_mac() { return s_gateway_mac; }
