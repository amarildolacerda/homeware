#include "espnow_handler.h"
#include "config.h"
#include "sensor_registry.h"
#include "mqtt_client.h"
#include "platform.h"
#include "log_buffer.h"
#include <Arduino.h>
#include <EEPROM.h>
#include "common_console.h"

#ifdef ESPNOW_ENABLED

/* ESP-NOW delivery uses BROADCAST (all clients receive and filter by
   sensor_mac/target_mac). Validated with QuickESPNow (qgw/qclient, both STA on
   the same AP): ESP8266->ESP32 unicast is silently dropped by WiFi/ESP-NOW
   coexistence, while broadcast works in both directions. See AGENTS.md rule 18. */

// Forward declaration of C-linkage callback (used in init())
#ifdef ESP32
extern "C" void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len);
#else
extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len);
#endif

EspnowHandler* EspnowHandler::s_self = nullptr;
const uint8_t EspnowHandler::s_bcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

EspnowHandler::EspnowHandler() {
    memset(m_pending_pairs, 0, sizeof(m_pending_pairs));
    memset(m_pending_state_slots, 0, sizeof(m_pending_state_slots));
}

int EspnowHandler::init() {
    s_self = this;
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(m_gateway_mac);

    if (esp_now_init() != 0) {
        console.println("[ESP-NOW] Init failed");
        return -1;
    }

#if !defined(ARDUINO_ARCH_ESP32)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#endif
    esp_now_register_recv_cb(espnow_recv_cb);

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espnow_add_peer_wrapper(broadcast_mac, WiFi.channel());

    char mac_str[18];
    mac_to_str(m_gateway_mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] Initialized, MAC: %s WiFi ch=%d\n",
                  mac_str, WiFi.channel());
    return 0;
}

int EspnowHandler::send(const uint8_t* data, size_t len) {
    return espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)data, len, "ESP-NOW") ? 0 : -1;
}

bool EspnowHandler::is_ready() const {
    return true;
}

void EspnowHandler::loop() {
    if (m_pairing_mode && millis() - m_pairing_start > PAIRING_WINDOW_MS) {
        m_pairing_mode = false;
        digitalWrite(STATUS_LED_GPIO, HIGH);
        console.println("[ESP-NOW] Pairing mode timeout");
    }

    for (int i = 0; i < PENDING_PAIR_MAX; i++) {
        if (!m_pending_pairs[i].active) continue;

        m_pending_pairs[i].active = false;
        int free_slot = sensor_registry_find_free_slot();

        if (free_slot < 0) {
            send_ack(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, PAIR_STATUS_FULL, 0xFF);
            continue;
        }

        if (!sensor_registry_add(m_pending_pairs[i].mac, m_pending_pairs[i].sensor_type,
                                 free_slot, m_pending_pairs[i].name, m_pending_pairs[i].client_chip)) {
            int existing = sensor_registry_find_by_mac(m_pending_pairs[i].mac);
            if (existing >= 0)
                send_pair_response(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, existing);
            continue;
        }
        send_pair_response(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, free_slot);
        {
            char mac_str[18];
            mac_to_str(m_pending_pairs[i].mac, mac_str, sizeof(mac_str));
            log_add("info", "Sensor %s pareado slot %d", mac_str, free_slot);
        }
        if (mqtt_client_is_connected())
            mqtt_client_publish_discovery(sensor_registry_get(free_slot));

        char pending_mac_str[18];
        mac_to_str(m_pending_pairs[i].mac, pending_mac_str, sizeof(pending_mac_str));
        console.printf("[ESP-NOW] Paired sensor slot %d: %s type=%d\n",
                      free_slot, pending_mac_str, m_pending_pairs[i].sensor_type);
    }

    process_bridge_queue();
    process_pending_commands();

    if (millis() - m_last_heartbeat > HEARTBEAT_INTERVAL_MS) {
        m_last_heartbeat = millis();

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

bool EspnowHandler::start_pairing() {
    if (sensor_registry_count_paired() >= MAX_VIRTUAL_SENSORS) {
        console.println("[ESP-NOW] Max sensors reached");
        return false;
    }
    m_pairing_mode = true;
    m_pairing_start = millis();
    digitalWrite(STATUS_LED_GPIO, LOW);
    console.printf("[ESP-NOW] Pairing mode started (%us)\n", PAIRING_WINDOW_MS / 1000);
    return true;
}

void EspnowHandler::stop_pairing() {
    m_pairing_mode = false;
    digitalWrite(STATUS_LED_GPIO, HIGH);
    console.println("[ESP-NOW] Pairing mode stopped");
}

bool EspnowHandler::is_pairing() const {
    return m_pairing_mode;
}

unsigned long EspnowHandler::pairing_remaining_ms() const {
    if (!m_pairing_mode) return 0;
    unsigned long elapsed = millis() - m_pairing_start;
    if (elapsed >= PAIRING_WINDOW_MS) return 0;
    return PAIRING_WINDOW_MS - elapsed;
}

void EspnowHandler::queue_bridge_state(int slot) {
    int next = (m_pending_state_head + 1) % PENDING_STATE_MAX;
    if (next == m_pending_state_tail) return;
    m_pending_state_slots[m_pending_state_head] = slot;
    m_pending_state_head = next;
}

void EspnowHandler::process_bridge_queue() {
    while (m_pending_state_tail != m_pending_state_head) {
        int slot = m_pending_state_slots[m_pending_state_tail];
        m_pending_state_tail = (m_pending_state_tail + 1) % PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected())
            mqtt_client_publish_state(s);
    }
}

void EspnowHandler::send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot) {
    if (!mac || mac_equal(mac, s_bcast_addr)) return;
    espnow_ack_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.msg_type = MSG_ACK;
    ack.sequence = sequence;
    ack.status = status;
    ack.assigned_slot = slot;
    mac_copy(ack.sensor_mac, mac);

    espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
    espnow_send_wrapper((uint8_t*)mac, (uint8_t*)&ack, sizeof(ack), "ESP-NOW");
}

void EspnowHandler::send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot) {
    if (!mac || mac_equal(mac, s_bcast_addr)) return;
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    resp.sequence = sequence;
    resp.status = PAIR_STATUS_OK;
    resp.assigned_slot = slot;
    mac_copy(resp.sensor_mac, mac);
    mac_copy(resp.gateway_mac, m_gateway_mac);

    espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
    espnow_send_wrapper((uint8_t*)mac, (uint8_t*)&resp, sizeof(resp), "ESP-NOW");
}

void EspnowHandler::send_gw_announce(const uint8_t *mac) {
    espnow_gw_announce_t ann;
    memset(&ann, 0, sizeof(ann));
    ann.msg_type = MSG_GW_ANNOUNCE;
    mac_copy(ann.gateway_mac, m_gateway_mac);
    strncpy((char*)ann.fw_version, FW_VERSION, sizeof(ann.fw_version));

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper(s_bcast_addr, ch);
    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&ann, sizeof(ann), "ESP-NOW");
}

void EspnowHandler::announce() {
    send_gw_announce(s_bcast_addr);
}

const uint8_t* EspnowHandler::dest_for_chip(const uint8_t *mac, uint8_t client_chip) {
    if (client_chip == HW_CHIP_ESP_1) {
        espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
        return mac;
    }
    return s_bcast_addr;
}

bool EspnowHandler::send_command(const uint8_t *mac, uint8_t state) {
    uint8_t chip = HW_CHIP_UNKNOWN;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && mac_equal(s->mac, mac)) {
            chip = s->client_chip;
            break;
        }
    }
    espnow_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.sequence = 0;
    mac_copy(cmd.target_mac, mac);
    cmd.command = state;

    const uint8_t *dest = dest_for_chip(mac, chip);
    int slot = -1;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *vs = sensor_registry_get(i);
        if (vs && vs->paired && mac_equal(vs->mac, mac)) { slot = i; break; }
    }
    // Enqueue for reliable delivery: on/off commands are idempotent, so the
    // hop-based retry never toggles the relay twice to the same state.
    enqueue_cmd(dest, mac_equal(dest, s_bcast_addr), slot,
                (uint8_t*)&cmd, sizeof(cmd));
    return true;  // command queued for (re)sending; see AGENTS.md rule 18
}

bool EspnowHandler::send_restart(const uint8_t *mac) {
    uint8_t chip = HW_CHIP_UNKNOWN;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && mac_equal(s->mac, mac)) {
            chip = s->client_chip;
            break;
        }
    }
    espnow_restart_t rst;
    memset(&rst, 0, sizeof(rst));
    rst.msg_type = MSG_RESTART;
    rst.sequence = 0;
    mac_copy(rst.target_mac, mac);

    const uint8_t *dest = dest_for_chip(mac, chip);
    enqueue_cmd(dest, mac_equal(dest, s_bcast_addr), -1,
                (uint8_t*)&rst, sizeof(rst));
    return true;
}

void EspnowHandler::enqueue_cmd(const uint8_t* dest, bool is_bcast, int slot,
                                const uint8_t* frame, uint8_t len) {
    if (len > sizeof(PendingCmd::frame) || len == 0) return;

    for (int i = 0; i < PENDING_CMD_MAX; i++) {
        PendingCmd &pc = m_pending_cmds[i];
        if (!pc.active) {
            pc.active = true;
            mac_copy(pc.dest, dest);
            pc.is_bcast = is_bcast;
            pc.slot = (slot < 0) ? 0xFF : (uint8_t)slot;
            memcpy(pc.frame, frame, len);
            pc.len = len;
            pc.hops = 0;
            pc.created_at = millis();
            pc.next_retry_ms = pc.created_at;  // dispatch immediately in loop()
            return;
        }
    }
    // Queue full: drop the oldest pending command to make room.
    unsigned long oldest = millis();
    int victim = 0;
    for (int i = 0; i < PENDING_CMD_MAX; i++) {
        if (m_pending_cmds[i].created_at < oldest) {
            oldest = m_pending_cmds[i].created_at;
            victim = i;
        }
    }
    m_pending_cmds[victim].active = false;
    console.println("[ESP-NOW] command queue full; dropping oldest pending cmd");
}

void EspnowHandler::process_pending_commands() {
    unsigned long now = millis();
    for (int i = 0; i < PENDING_CMD_MAX; i++) {
        PendingCmd &pc = m_pending_cmds[i];
        if (!pc.active) continue;

        unsigned long age = now - pc.created_at;
        if (age > CMD_TTL_MS) {
            pc.active = false;
            char mac_str[18];
            mac_to_str(pc.dest, mac_str, sizeof(mac_str));
            console.printf("[ESP-NOW] cmd to %s dropped after %u hops (TTL)\n",
                           mac_str, pc.hops);
            continue;
        }

        if (pc.hops >= CMD_MAX_HOPS) {
            pc.active = false;
            char mac_str[18];
            mac_to_str(pc.dest, mac_str, sizeof(mac_str));
            console.printf("[ESP-NOW] cmd to %s dropped after %u hops (max)\n",
                           mac_str, pc.hops);
            continue;
        }

        if (now < pc.next_retry_ms) continue;

        pc.hops++;
        espnow_send_wrapper(pc.dest, pc.frame, pc.len, "ESP-NOW");
        pc.next_retry_ms = now + CMD_HOP_INTERVAL_MS;
        if (pc.hops == 1) {
            char mac_str[18];
            mac_to_str(pc.dest, mac_str, sizeof(mac_str));
            console.printf("[ESP-NOW] cmd to %s hop %u/%u\n",
                           mac_str, pc.hops, CMD_MAX_HOPS);
        }
    }
}

void EspnowHandler::broadcast_time_sync(uint32_t epoch_seconds) {
    espnow_time_sync_t ts;
    memset(&ts, 0, sizeof(ts));
    ts.msg_type = MSG_TIME_SYNC;
    ts.sequence = m_time_sync_sequence++;
    mac_copy(ts.gateway_mac, m_gateway_mac);
    ts.epoch_seconds = epoch_seconds;

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper((uint8_t*)s_bcast_addr, ch);
    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&ts, sizeof(ts), "ESP-NOW");
}

void EspnowHandler::handle_rx(const uint8_t *mac, const uint8_t *data, int len) {
    if (!data || len < 1) { m_crc_errors++; console.printf("[ESPNOW] RX null/empty, len=%d\n", len); return; }
    m_rx_count++;
    uint8_t msg_type = data[0];

    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    console.printf("[ESPNOW] RX msg_type=0x%02X len=%d from %s\n", msg_type, len, mac_str);

    switch (msg_type) {
        case MSG_PAIR_REQUEST: {
            if (len < (int)sizeof(espnow_pair_request_t)) { m_crc_errors++; return; }
            const espnow_pair_request_t *req = (const espnow_pair_request_t*)data;
            char sensor_mac_str[18];
            mac_to_str(req->sensor_mac, sensor_mac_str, sizeof(sensor_mac_str));

            int existing_slot = sensor_registry_find_by_mac(req->sensor_mac);
            if (existing_slot >= 0) {
                send_pair_response(mac, req->sequence, existing_slot);
                return;
            }
            {
                EEPROM.begin(EEPROM_SIZE);
                bool pairing_required = EEPROM.read(EEPROM_PAIRING_EN_OFFSET) == 1;
                EEPROM.end();
                if (pairing_required && !m_pairing_mode) {
                    espnow_nak_t nak;
                    memset(&nak, 0, sizeof(nak));
                    nak.msg_type = MSG_NAK;
                    nak.sequence = req->sequence;
                    mac_copy(nak.target_mac, req->sensor_mac);
                    nak.reason = NAK_REASON_PAIRING_DISABLED;
                    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&nak, sizeof(nak), "ESP-NOW");
                    return;
                }
            }
            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (m_pending_pairs[i].active && mac_equal(m_pending_pairs[i].mac, req->sensor_mac)) {
                    return;
                }
            }
            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (!m_pending_pairs[i].active) {
                    mac_copy(m_pending_pairs[i].mac, req->sensor_mac);
                    m_pending_pairs[i].sensor_type = req->sensor_type;
                    m_pending_pairs[i].client_chip = req->client_chip;
                    m_pending_pairs[i].sequence = req->sequence;
                    strncpy(m_pending_pairs[i].name, req->device_name, sizeof(m_pending_pairs[i].name) - 1);
                    m_pending_pairs[i].name[sizeof(m_pending_pairs[i].name) - 1] = '\0';
                    m_pending_pairs[i].active = true;
                    break;
                }
            }
            break;
        }

        case MSG_SENSOR_DATA:
        case MSG_HEARTBEAT: {
            if (len < (int)ESPNOW_HEADER_FIXED_SIZE) { m_crc_errors++; return; }
            const espnow_header_t *hdr = (const espnow_header_t*)data;
            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { m_crc_errors++; return; }
            if (len < (int)(ESPNOW_HEADER_FIXED_SIZE + hdr->payload_len)) { m_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            if (msg_type == MSG_SENSOR_DATA) {
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_DENIED, 0xFF);
                    return;
                }
                sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
                send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                log_add("info", "Dados recebidos slot %d seq %d", slot, hdr->sequence);
                m_ack_count++;
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

        case MSG_GW_ANNOUNCE: {
            // Another gateway/extender announcing — log only, no action needed
            const espnow_gw_announce_t *ann = (const espnow_gw_announce_t*)data;
            char ann_mac[18];
            mac_to_str(ann->gateway_mac, ann_mac, sizeof(ann_mac));
            console.printf("[ESPNOW] GW_ANNOUNCE from extender %s (src %s)\n", ann_mac, mac_str);
            break;
        }

        case MSG_GW_DISCOVER: {
            int slot = sensor_registry_find_by_mac(mac);
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot >= 0) {
                    sensor_registry_add(mac, SENSOR_TYPE_REPEATER, slot, "Repeater", HW_CHIP_ESP_1);
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

        case MSG_REPEATER_STATUS: {
            if (len < (int)(ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t))) { m_crc_errors++; return; }
            const espnow_header_t *hdr = (const espnow_header_t*)data;
            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { m_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_FULL, 0xFF);
                    return;
                }
                sensor_registry_add(hdr->sensor_mac, hdr->sensor_type, slot, "Repeater", HW_CHIP_ESP_1);
            } else {
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s && s->type != SENSOR_TYPE_REPEATER) {
                    s->type = SENSOR_TYPE_REPEATER;
                    strncpy(s->name, "Repeater", sizeof(s->name) - 1);
                    s->name[sizeof(s->name) - 1] = '\0';
                    sensor_registry_save();
                }
            }
            sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
            send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
            log_add("info", "Repeater status slot %d seq %d", slot, hdr->sequence);
            m_ack_count++;
            queue_bridge_state(slot);
            break;
        }

        case MSG_COMMAND: {
            if (len < 10) { m_crc_errors++; return; }
            const espnow_command_t *cmd = (const espnow_command_t *)data;
            uint8_t target[6];
            mac_copy(target, cmd->target_mac);
            bool mac_is_zero = (target[0] == 0 && target[1] == 0 && target[2] == 0 &&
                                target[3] == 0 && target[4] == 0 && target[5] == 0);
            if (mac_is_zero && len >= (int)sizeof(espnow_command_t) && cmd->target_device_id[0] != '\0') {
                int slot = sensor_registry_find_by_name(cmd->target_device_id);
                if (slot < 0) return;
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (!s || !s->paired) return;
                mac_copy(target, s->mac);
            }
            if (mac_is_zero) return;
            espnow_command_t fwd;
            memset(&fwd, 0, sizeof(fwd));
            fwd.msg_type = MSG_COMMAND;
            fwd.sequence = cmd->sequence;
            mac_copy(fwd.target_mac, target);
            fwd.command = cmd->command;
            espnow_send_wrapper((uint8_t *)s_bcast_addr, (uint8_t *)&fwd, sizeof(fwd), "ESP-NOW");
            break;
        }

        default:
            console.printf("[ESPNOW] UNKNOWN msg_type=0x%02X len=%d from %s\n", msg_type, len, mac_str);
            m_crc_errors++;
            break;
    }
}

// C-linkage callback registered with ESP-NOW
#ifdef ESP32
extern "C" void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
#else
extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
#endif
    if (EspnowHandler::s_self) {
        EspnowHandler::s_self->handle_rx(mac, data, len);
    }
}

// ---- Compatibility wrappers ----
static EspnowHandler s_espnow_handler;

bool espnow_handler_init() { return s_espnow_handler.init() == 0; }
void espnow_handler_loop() { s_espnow_handler.loop(); }
bool espnow_start_pairing() { return s_espnow_handler.start_pairing(); }
void espnow_stop_pairing() { s_espnow_handler.stop_pairing(); }
bool espnow_is_pairing() { return s_espnow_handler.is_pairing(); }
unsigned long espnow_pairing_remaining_ms() { return s_espnow_handler.pairing_remaining_ms(); }
unsigned long espnow_get_rx_count() { return s_espnow_handler.get_rx_count(); }
unsigned long espnow_get_ack_count() { return s_espnow_handler.get_ack_count(); }
unsigned long espnow_get_crc_errors() { return s_espnow_handler.get_crc_errors(); }
uint8_t* espnow_get_gateway_mac() { return s_espnow_handler.get_radio_mac(); }
void espnow_announce() { s_espnow_handler.announce(); }
void espnow_broadcast_time_sync(uint32_t epoch) { s_espnow_handler.broadcast_time_sync(epoch); }
bool espnow_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) { (void)slot; return s_espnow_handler.send_command(mac, state); }
bool espnow_send_restart(const uint8_t *mac, uint8_t slot) { (void)slot; return s_espnow_handler.send_restart(mac); }

#endif // ESPNOW_ENABLED
