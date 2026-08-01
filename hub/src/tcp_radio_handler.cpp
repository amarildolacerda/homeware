#ifdef TCP_ENABLED

#include "tcp_radio_handler.h"
#include "sensor_registry.h"
#include "config.h"
#include "common_console.h"
#include "web_server.h"
#include <ESP8266WebServer.h>

extern MyWebServer s_server;

TcpRadioHandler* TcpRadioHandler::s_self = nullptr;

TcpRadioHandler::TcpRadioHandler() {
    s_self = this;
}

int TcpRadioHandler::init() {
    m_udp.begin(TCP_UDP_PORT);
    console.printf("[tcp] UDP server started on port %d\n", TCP_UDP_PORT);

    s_server.on("/node/register", HTTP_POST, [this]() {
        if (!s_server.hasArg("plain")) {
            s_server.send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, s_server.arg("plain"));
        if (error) {
            s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        const char* device_name = doc["device_name"];
        const char* fw_version = doc["fw_version"];
        uint8_t sensor_type = doc["sensor_type"];

        if (!device_id || !device_name) {
            s_server.send(400, "application/json", "{\"error\":\"missing fields\"}");
            return;
        }

        for (int i = 0; device_id[i]; i++) {
            if (device_id[i] < 0x20 || device_id[i] > 0x7E) {
                s_server.send(400, "application/json", "{\"error\":\"invalid device_id\"}");
                return;
            }
        }

        if (sensor_type < 1 || sensor_type > 10) {
            s_server.send(400, "application/json", "{\"error\":\"invalid sensor_type\"}");
            return;
        }

        uint8_t mac[6];
        WiFi.macAddress(mac);

        handle_register(mac, device_id, sensor_type, device_name, fw_version);

        int slot = find_slot_by_device_id(device_id);

        DynamicJsonDocument response(256);
        response["status"] = "ok";
        response["assigned_slot"] = slot;
        response["device_id"] = device_id;

        String responseStr;
        serializeJson(response, responseStr);
        s_server.send(200, "application/json", responseStr);
    });

    s_server.on("/node/state", HTTP_POST, [this]() {
        if (!s_server.hasArg("plain")) {
            s_server.send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, s_server.arg("plain"));
        if (error) {
            s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        if (!device_id) {
            s_server.send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        JsonObject state = doc.as<JsonObject>();
        handle_state(device_id, state);

        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/node/heartbeat", HTTP_POST, [this]() {
        if (!s_server.hasArg("plain")) {
            s_server.send(400, "application/json", "{\"error\":\"no body\"}");
            return;
        }

        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, s_server.arg("plain"));
        if (error) {
            s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }

        const char* device_id = doc["device_id"];
        if (!device_id) {
            s_server.send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        handle_heartbeat(device_id);

        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/node/command/", HTTP_GET, [this]() {
        String uri = s_server.uri();
        String device_id = uri.substring(uri.lastIndexOf("/") + 1);

        if (device_id.length() == 0) {
            s_server.send(400, "application/json", "{\"error\":\"missing device_id\"}");
            return;
        }

        DynamicJsonDocument response(256);
        JsonObject resObj = response.as<JsonObject>();
        handle_command_get(device_id.c_str(), resObj);

        String responseStr;
        serializeJson(response, responseStr);
        s_server.send(200, "application/json", responseStr);
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

void TcpRadioHandler::handle_register(const uint8_t* mac, const char* device_id, uint8_t sensor_type, const char* device_name, const char* fw_version) {
    int slot = find_slot_by_device_id(device_id);
    if (slot >= 0) {
        console.printf("[tcp] Device %s already registered at slot %d\n", device_id, slot);
        return;
    }

    slot = sensor_registry_find_free_slot();
    if (slot < 0) {
        console.println("[tcp] No free slots available");
        return;
    }

    sensor_registry_add(mac, sensor_type, slot, device_name, HW_CHIP_UNKNOWN, RADIO_TCP);

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (sensor) {
        strncpy(sensor->bridge_device_id, device_id, sizeof(sensor->bridge_device_id) - 1);
        sensor->bridge_device_id[sizeof(sensor->bridge_device_id) - 1] = '\0';
        sensor->paired = true;
        sensor->online = true;
        sensor->last_seen = millis();
    }

    console.printf("[tcp] Device %s registered at slot %d (type %d)\n", device_id, slot, sensor_type);
}

void TcpRadioHandler::handle_state(const char* device_id, JsonObject& state) {
    int slot = find_slot_by_device_id(device_id);
    if (slot < 0) {
        console.printf("[tcp] Unknown device: %s\n", device_id);
        return;
    }

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor) return;

    switch (sensor->type) {
        case SENSOR_TYPE_TEMP_HUM:
            if (state.containsKey("temperature")) sensor->state.temp_hum.temperature = state["temperature"];
            if (state.containsKey("humidity")) sensor->state.temp_hum.humidity = state["humidity"];
            break;
        case SENSOR_TYPE_GAS:
            if (state.containsKey("gas_level")) sensor->state.gas.gas_level = state["gas_level"];
            if (state.containsKey("alarm")) sensor->state.gas.alarm = state["alarm"];
            break;
        case SENSOR_TYPE_ONOFF:
            if (state.containsKey("state")) sensor->state.onoff.state = state["state"];
            break;
        default:
            break;
    }

    sensor->last_seen = millis();
    sensor->online = true;
    m_rx_count++;
}

void TcpRadioHandler::handle_heartbeat(const char* device_id) {
    int slot = find_slot_by_device_id(device_id);
    if (slot < 0) return;

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor) return;

    sensor->last_seen = millis();
    sensor->online = true;
    m_rx_count++;
}

bool TcpRadioHandler::handle_command_get(const char* device_id, JsonObject& response) {
    auto it = m_pending_commands.find(device_id);
    if (it == m_pending_commands.end() || it->second.empty()) {
        return false;
    }

    PendingCommand cmd = it->second.front();
    it->second.erase(it->second.begin());

    response["command"] = cmd.command;
    response["slot"] = cmd.slot;

    return true;
}

void TcpRadioHandler::handle_udp_discover() {
    int packetSize = m_udp.parsePacket();
    if (packetSize <= 0) return;

    uint8_t buf[64];
    int len = m_udp.read(buf, sizeof(buf));
    if (len < 1) return;

    uint8_t msg_type = buf[0];
    if (msg_type != TCP_MSG_GW_DISCOVER) return;

    tcp_gw_discover_t* discover = (tcp_gw_discover_t*)buf;

    console.printf("[tcp] UDP Discover from %s (type %d)\n",
                   discover->device_name, discover->sensor_type);

    tcp_gw_announce_t announce;
    announce.msg_type = TCP_MSG_GW_ANNOUNCE;
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
