#ifdef TCP_ENABLED

#include "tcp_radio_handler.h"
#include "sensor_registry.h"
#include "config.h"
#include "common_console.h"
#include "log_buffer.h"
#include "web_server.h"
#include "platform.h"
#include "mqtt_client.h"
#include "AsyncJson.h"
#include <algorithm>

extern MyWebServer s_server;

TcpRadioHandler* TcpRadioHandler::s_self = nullptr;

TcpRadioHandler::TcpRadioHandler() {
    s_self = this;
}

int TcpRadioHandler::init() {
    m_udp.begin(TCP_UDP_PORT);
    console.printf("[tcp] UDP server started on port %d\n", TCP_UDP_PORT);

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/node/register", [this](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json.as<String>());
        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        const char* device_name = doc["device_name"];
        const char* fw_version = doc["fw_version"];
        uint8_t sensor_type = doc["sensor_type"];

        if (!device_id || !device_name) {
            request->send(400, "application/json", "{\"error\":\"missing fields\"}");
            return;
        }

        for (int i = 0; device_id[i]; i++) {
            if (device_id[i] < 0x20 || device_id[i] > 0x7E) {
                request->send(400, "application/json", "{\"error\":\"invalid device_id\"}");
                return;
            }
        }

        if (sensor_type < 1 || sensor_type > 10) {
            request->send(400, "application/json", "{\"error\":\"invalid sensor_type\"}");
            return;
        }

        uint8_t mac[6];
        const char* mac_str = doc["mac"];
        bool mac_ok = false;
        if (mac_str && strlen(mac_str) >= 17) {
            // Use node's reported MAC. mac_to_str() (shared) uses '-' separators,
            // so normalize to ':' before parsing (older nodes may send either).
            char norm[18];
            size_t n = strlen(mac_str);
            if (n >= sizeof(norm)) n = sizeof(norm) - 1;
            memcpy(norm, mac_str, n);
            norm[n] = '\0';
            for (char *p = norm; *p; p++) if (*p == '-') *p = ':';
            int a, b, c, d, e, f;
            if (sscanf(norm, "%02X:%02X:%02X:%02X:%02X:%02X", &a, &b, &c, &d, &e, &f) == 6) {
                mac[0] = (uint8_t)a; mac[1] = (uint8_t)b; mac[2] = (uint8_t)c;
                mac[3] = (uint8_t)d; mac[4] = (uint8_t)e; mac[5] = (uint8_t)f;
                mac_ok = true;
            }
        }
        if (!mac_ok) {
            // Fallback: derive unique pseudo-MAC from client IP to avoid all
            // TCP nodes sharing the hub's MAC (which causes slot collision).
            IPAddress client_ip = request->client()->remoteIP();
            mac[0] = 0x02;  // locally-administered, unicast
            mac[1] = 0x00;
            mac[2] = 0x00;
            mac[3] = client_ip[0];
            mac[4] = client_ip[1];
            mac[5] = client_ip[2] | (client_ip[3] << 4);  // pack IP bytes
            char fallback_mac[18];
            snprintf(fallback_mac, sizeof(fallback_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            console.printf("[tcp] No MAC from node %s, derived pseudo-MAC %s from IP %s\n",
                           device_id, fallback_mac, client_ip.toString().c_str());
        }

        uint8_t client_chip = doc["client_chip"] | HW_CHIP_UNKNOWN;

        handle_register(mac, device_id, sensor_type, device_name, fw_version, client_chip);

        int slot = find_slot_by_device_id(device_id);

        JsonDocument response;
        response["status"] = "ok";
        response["assigned_slot"] = slot;
        response["device_id"] = device_id;

        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    }));

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/node/state", [this](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json.as<String>());
        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        if (!device_id) {
            request->send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        JsonObject state = doc.as<JsonObject>();
        handle_state(device_id, state);

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/node/heartbeat", [this](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json.as<String>());
        if (error) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        if (!device_id) {
            request->send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        handle_heartbeat(device_id);

        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    s_server.on("/node/command/*", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // Parse /node/command/{device_id} from URL
        String device_id = request->url().substring(14); // after "/node/command/" (14 chars)

        if (device_id.length() == 0) {
            request->send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        JsonDocument response;
        JsonObject resObj = response.to<JsonObject>();
        handle_command_get(device_id.c_str(), resObj);

        // Piggyback epoch on every command poll response so TCP nodes stay
        // time-synced without a separate push endpoint.
        if (m_time_sync_epoch > 0) {
            resObj["epoch"] = m_time_sync_epoch;
        }

        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr);
    });

    console.println("[tcp] HTTP endpoints registered");
    return 0;
}

int TcpRadioHandler::send(const uint8_t* data, size_t len) {
    return 0;
}

void TcpRadioHandler::loop() {
    handle_udp_discover();
    cleanup_expired_commands();
    process_bridge_queue();

    // Check for TCP nodes that haven't reported in and mark them offline.
    // TCP nodes poll for commands every ~1s and send heartbeat/state every
    // 30-60s, so SENSOR_TIMEOUT_MS (300s) is a safe threshold.
    static unsigned long s_last_timeout_check = 0;
    unsigned long now = millis();
    if (now - s_last_timeout_check > 30000) {
        s_last_timeout_check = now;
        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired && s->radio_type == RADIO_TCP && s->online) {
                unsigned long elapsed = now - s->last_seen;
                if (elapsed > SENSOR_TIMEOUT_MS) {
                    s->online = false;
                    log_add("warn", "TCP sensor slot %d offline", i);
                    console.printf("[tcp] Sensor slot %d offline (last seen %lu ms ago)\n", i, elapsed);
                    if (mqtt_client_is_connected()) {
                        mqtt_client_publish_availability(s, false);
                    }
                }
            }
        }
    }
}

bool TcpRadioHandler::is_ready() const {
    return true;
}

bool TcpRadioHandler::start_pairing() {
    m_pairing_mode = true;
    m_pairing_start = millis();
    return true;
}

void TcpRadioHandler::stop_pairing() {
    m_pairing_mode = false;
}

bool TcpRadioHandler::is_pairing() const {
    return m_pairing_mode;
}

unsigned long TcpRadioHandler::pairing_remaining_ms() const {
    if (!m_pairing_mode) return 0;
    unsigned long elapsed = millis() - m_pairing_start;
    if (elapsed >= 60000) return 0;
    return 60000 - elapsed;
}

void TcpRadioHandler::announce() {
}

void TcpRadioHandler::broadcast_time_sync(uint32_t epoch_seconds) {
    // TCP nodes poll for commands every ~1s; the epoch is delivered via
    // piggyback in the command poll response (handle_command_get).
    // Store it here so the next poll response carries the fresh epoch.
    m_time_sync_epoch = epoch_seconds;
    console.printf("[tcp] Time sync stored: %lu (delivered via command poll)\n", epoch_seconds);
}

void TcpRadioHandler::broadcast_device_list() {
    // TCP nodes poll for commands; device list is delivered via HTTP GET /api/sensors.
    // Nothing to broadcast here — the lamp fetches it directly.
}

bool TcpRadioHandler::send_command(const uint8_t* mac, uint8_t state) {
    int slot = sensor_registry_find_by_mac(mac);
    if (slot < 0) return false;

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor || !sensor->paired) return false;

    PendingCommand cmd;
    cmd.command = (state == 1) ? "on" : "off";
    cmd.slot = slot;
    cmd.created_at = millis();

    m_pending_commands[sensor->bridge_device_id].push_back(cmd);

    console.printf("[tcp] Command queued for %s: %s\n", sensor->bridge_device_id, cmd.command.c_str());
    log_add("info", "[tcp] Command queued for %s: %s (radio_type=%d)", sensor->bridge_device_id, cmd.command.c_str(), sensor->radio_type);
    return true;
}

bool TcpRadioHandler::send_restart(const uint8_t* mac) {
    int slot = sensor_registry_find_by_mac(mac);
    if (slot < 0) return false;

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor || !sensor->paired) return false;

    PendingCommand cmd;
    cmd.command = "restart";
    cmd.slot = slot;
    cmd.created_at = millis();

    m_pending_commands[sensor->bridge_device_id].push_back(cmd);

    console.printf("[tcp] Restart queued for %s\n", sensor->bridge_device_id);
    return true;
}

void TcpRadioHandler::handle_register(const uint8_t* mac, const char* device_id, uint8_t sensor_type, const char* device_name, const char* fw_version, uint8_t client_chip) {
    // 1) Already registered (by device_id) → refresh liveness only. Do NOT
    //    clobber a user-assigned name from the dashboard rename.
    int slot = find_slot_by_device_id(device_id);
    if (slot >= 0) {
        console.printf("[tcp] Device %s already registered at slot %d, fw=%s\n", device_id, slot, fw_version ? fw_version : "unknown");
        virtual_sensor_t* sensor = sensor_registry_get(slot);
        if (sensor) {
            bool was_offline = !sensor->online;
            sensor->online = true;
            sensor->last_seen = millis();
            if (was_offline && mqtt_client_is_connected()) {
                mqtt_client_publish_availability(sensor, true);
            }
            // refresh type/mac/radio/name in place
            sensor->type = sensor_type;
            if (device_name && strlen(device_name) > 0) {
                strncpy(sensor->name, device_name, sizeof(sensor->name) - 1);
                sensor->name[sizeof(sensor->name) - 1] = '\0';
            }
            mac_copy(sensor->mac, mac);
            sensor->radio_type = RADIO_TCP;
            sensor->client_chip = client_chip;
            // fw_version is node-reported (not user-set) → always refresh
            if (fw_version && strlen(fw_version) > 0) {
                strncpy(sensor->fw_version, fw_version, sizeof(sensor->fw_version) - 1);
                sensor->fw_version[sizeof(sensor->fw_version) - 1] = '\0';
            }
            sensor_registry_save();
        }
        return;
    }

    // 2) Genuinely new device — find free slot and register.
    slot = sensor_registry_find_free_slot();
    if (slot < 0) {
        console.println("[tcp] No free slots available");
        return;
    }

    if (!sensor_registry_add(mac, sensor_type, slot, device_name, client_chip, RADIO_TCP)) {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[tcp] sensor_registry_add failed (MAC %s conflict); not creating phantom slot\n", mac_str);
        return;
    }

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (sensor) {
        strncpy(sensor->bridge_device_id, device_id, sizeof(sensor->bridge_device_id) - 1);
        sensor->bridge_device_id[sizeof(sensor->bridge_device_id) - 1] = '\0';
        sensor->paired = true;
        sensor->online = true;
        sensor->last_seen = millis();
        if (fw_version && strlen(fw_version) > 0) {
            strncpy(sensor->fw_version, fw_version, sizeof(sensor->fw_version) - 1);
            sensor->fw_version[sizeof(sensor->fw_version) - 1] = '\0';
        }
        sensor_registry_save();
    }

    console.printf("[tcp] Device %s registered at slot %d (type %d, fw=%s)\n", device_id, slot, sensor_type, fw_version ? fw_version : "unknown");

    // Publish discovery + online availability for the new sensor
    if (mqtt_client_is_connected()) {
        mqtt_client_publish_discovery(sensor_registry_get(slot));
        mqtt_client_publish_availability(sensor_registry_get(slot), true);
    }
}

void TcpRadioHandler::handle_state(const char* device_id, JsonObject& state) {
    int slot = find_slot_by_device_id(device_id);
    if (slot < 0) {
        console.printf("[tcp] Unknown device: %s\n", device_id);
        return;
    }

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor) return;

    // Optional metadata (IP, free heap) — the node reports these on each state
    // POST so the hub can display them. Parse the IP string into bytes.
    if (state["ip"].is<const char*>()) {
        const char* ip_str = state["ip"];
        int a = 0, b = 0, c = 0, d = 0;
        if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 &&
            a >= 0 && a <= 255 && b >= 0 && b <= 255 &&
            c >= 0 && c <= 255 && d >= 0 && d <= 255) {
            uint8_t new_ip[4] = { (uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d };
            // Detect IP change
            if (sensor->ip[0] != new_ip[0] || sensor->ip[1] != new_ip[1] ||
                sensor->ip[2] != new_ip[2] || sensor->ip[3] != new_ip[3]) {
                bool has_old = (sensor->ip[0] | sensor->ip[1] | sensor->ip[2] | sensor->ip[3]) != 0;
                if (has_old) {
                    console.printf("[tcp] %s IP changed: %d.%d.%d.%d -> %d.%d.%d.%d\n",
                                   device_id,
                                   sensor->ip[0], sensor->ip[1], sensor->ip[2], sensor->ip[3],
                                   new_ip[0], new_ip[1], new_ip[2], new_ip[3]);
                } else {
                    console.printf("[tcp] %s IP: %d.%d.%d.%d\n",
                                   device_id, new_ip[0], new_ip[1], new_ip[2], new_ip[3]);
                }
            }
            sensor->ip[0] = new_ip[0];
            sensor->ip[1] = new_ip[1];
            sensor->ip[2] = new_ip[2];
            sensor->ip[3] = new_ip[3];
        }
    }
    if (state["free_heap"].is<uint32_t>()) {
        sensor->free_heap = state["free_heap"];
    }
    if (state["fw_version"].is<const char*>()) {
        const char* fw = state["fw_version"];
        if (strlen(fw) > 0) {
            strncpy(sensor->fw_version, fw, sizeof(sensor->fw_version) - 1);
            sensor->fw_version[sizeof(sensor->fw_version) - 1] = '\0';
        }
    }

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM:
            if (state["temperature"].is<float>()) sensor->state.temp_hum.temperature = state["temperature"];
            if (state["humidity"].is<float>()) sensor->state.temp_hum.humidity = state["humidity"];
            break;
        case SENSOR_TYPE_GAS:
            if (state["gas_level"].is<uint16_t>()) sensor->state.gas.gas_level = state["gas_level"];
            if (state["alarm"].is<uint8_t>()) sensor->state.gas.alarm = state["alarm"];
            break;
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT: {
            // Accept both numeric (uint8_t) and boolean state, plus the legacy
            // relay_state field.
            uint8_t val = 0xFF;
            if (state["state"].is<uint8_t>()) val = (uint8_t)state["state"];
            else if (state["state"].is<bool>()) val = state["state"] ? 1 : 0;
            else if (state["relay_state"].is<uint8_t>()) val = (uint8_t)state["relay_state"];
            else if (state["relay_state"].is<bool>()) val = state["relay_state"] ? 1 : 0;
            if (val != 0xFF) sensor->state.onoff.state = val;
            break;
        }
        default:
            break;
    }

    bool was_offline = !sensor->online;
    sensor->last_seen = millis();
    sensor->online = true;
    m_rx_count++;

    if (was_offline && mqtt_client_is_connected()) {
        mqtt_client_publish_availability(sensor, true);
    }

     /* Feedback to HA: the ESP-NOW path bridges state via queue_bridge_state()
        -> process_bridge_queue() -> mqtt_client_publish_state(); without the
        same publish here, TCP nodes never update the MQTT state_topic and HA
        shows the entity always "off" (non-optimistic light/switch waits for
        state_topic). Queue instead of publishing synchronously so the HTTP
        handler never blocks on the broker TCP write. */
     queue_bridge_state(slot);
}

void TcpRadioHandler::handle_heartbeat(const char* device_id) {
    int slot = find_slot_by_device_id(device_id);
    if (slot < 0) return;

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor) return;

    bool was_offline = !sensor->online;
    sensor->last_seen = millis();
    sensor->online = true;
    m_rx_count++;

    if (was_offline && mqtt_client_is_connected()) {
        mqtt_client_publish_availability(sensor, true);
    }
}

void TcpRadioHandler::queue_bridge_state(int slot) {
    int next = (m_pending_state_head + 1) % TCP_PENDING_STATE_MAX;
    if (next == m_pending_state_tail) return;
    m_pending_state_slots[m_pending_state_head] = slot;
    m_pending_state_head = next;
}

void TcpRadioHandler::process_bridge_queue() {
    while (m_pending_state_tail != m_pending_state_head) {
        int slot = m_pending_state_slots[m_pending_state_tail];
        m_pending_state_tail = (m_pending_state_tail + 1) % TCP_PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected())
            mqtt_client_publish_state(s);
    }
}

bool TcpRadioHandler::handle_command_get(const char* device_id, JsonObject& response) {
    auto it = m_pending_commands.find(device_id);
    if (it == m_pending_commands.end() || it->second.empty()) {
        log_add("info", "[tcp] Command poll: %s -> no pending commands (map_size=%d)", device_id, (int)m_pending_commands.size());
        return false;
    }

    PendingCommand cmd = it->second.front();
    it->second.erase(it->second.begin());

    response["command"] = cmd.command;
    response["slot"] = cmd.slot;

    log_add("info", "[tcp] Command dispatch: %s -> cmd=%s slot=%d (remaining=%d)", device_id, cmd.command.c_str(), cmd.slot, (int)it->second.size());
    return true;
}

void TcpRadioHandler::handle_udp_discover() {
    int packetSize = m_udp.parsePacket();
    if (packetSize <= 0) return;

    uint8_t buf[64];
    int len = m_udp.read(buf, sizeof(buf));
    if (len < 1) return;

    uint8_t msg_type = buf[0];
    if (msg_type != MSG_GW_DISCOVER) return;

    tcp_gw_discover_t* discover = (tcp_gw_discover_t*)buf;

    console.printf("[tcp] UDP Discover from %s (type %d)\n",
                   discover->device_name, discover->sensor_type);

    tcp_gw_announce_t announce;
    announce.msg_type = MSG_GW_ANNOUNCE;
    memset(announce.fw_version, 0, sizeof(announce.fw_version));

    IPAddress ip = WiFi.localIP();
    snprintf(announce.hub_ip, sizeof(announce.hub_ip), "%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
    announce.hub_port = TCP_HTTP_PORT;

    m_udp.beginPacket(m_udp.remoteIP(), m_udp.remotePort());
    m_udp.write((uint8_t*)&announce, sizeof(announce));
    m_udp.endPacket();

    console.printf("[tcp] Sent announce to %s\n", m_udp.remoteIP().toString().c_str());
}

void TcpRadioHandler::cleanup_expired_commands() {
    unsigned long now = millis();
    for (auto it = m_pending_commands.begin(); it != m_pending_commands.end(); ) {
        auto& cmds = it->second;
        cmds.erase(
            std::remove_if(cmds.begin(), cmds.end(), [now](const PendingCommand& cmd) {
                return (now - cmd.created_at) > TCP_COMMAND_TTL_MS;
            }),
            cmds.end()
        );

        if (cmds.empty()) {
            it = m_pending_commands.erase(it);
        } else {
            ++it;
        }
    }
}

int TcpRadioHandler::find_slot_by_device_id(const char* device_id) {
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t* sensor = sensor_registry_get(i);
        if (sensor && sensor->paired && strcmp(sensor->bridge_device_id, device_id) == 0) {
            return i;
        }
    }
    return -1;
}

bool tcp_handler_init() {
    if (TcpRadioHandler::s_self) {
        return TcpRadioHandler::s_self->init() == 0;
    }
    return false;
}

void tcp_handler_loop() {
    if (TcpRadioHandler::s_self) {
        TcpRadioHandler::s_self->loop();
    }
}

bool tcp_send_command(const uint8_t* mac, uint8_t slot, uint8_t state) {
    if (TcpRadioHandler::s_self) {
        return TcpRadioHandler::s_self->send_command(mac, state);
    }
    return false;
}

bool tcp_send_restart(const uint8_t* mac, uint8_t slot) {
    if (TcpRadioHandler::s_self) {
        return TcpRadioHandler::s_self->send_restart(mac);
    }
    return false;
}

#endif
