#include "web_server.h"
#include "sensor_registry.h"
#include "radio_manager.h"

extern RadioManager s_radio_mgr;
#include "mqtt_client.h"
#include "config.h"
#include "pages.h"
#include "log_buffer.h"
#include "common_console.h"
#include "platform.h"
#include "captive_portal.h"
#include "device_router.h"
#include <ArduinoJson.h>
#include "AsyncJson.h"  // AsyncCallbackJsonWebHandler (JSON POST bodies)

extern bool gateway_ntp_synced();
extern time_t gateway_ntp_epoch();
extern void gateway_set_browser_epoch(time_t epoch);

#include <EEPROM.h>
#ifdef ESP32
#include <Update.h>
#endif

MyWebServer s_server(80);
static bool s_wifi_config_mode = false;
static unsigned long s_wifi_config_start = 0;
static bool s_wifi_reconnect_active = false;
static unsigned long s_wifi_reconnect_deadline = 0;

// WiFi credentials are not persisted by the ESP32 WiFi driver NVS in this
// environment (esp_wifi_set_config returns OK but is lost on reboot), so we
// store them in our own EEPROM NVS namespace instead.
static bool wifi_creds_load(char *ssid, char *pass) {
    EEPROM.begin(EEPROM_SIZE);
    bool valid = false;
    int pos = 0;
    for (int i = 0; i < EEPROM_WIFI_SSID_SIZE; i++) {
        uint8_t c = EEPROM.read(EEPROM_WIFI_SSID_OFFSET + i);
        if (c == 0) { valid = pos > 0; break; }
        if (c < 32 || c > 126) break;
        ssid[pos++] = (char)c;
        if (i == EEPROM_WIFI_SSID_SIZE - 1) valid = pos > 0;
    }
    ssid[pos] = '\0';
    for (int i = 0; i < EEPROM_WIFI_PASS_SIZE; i++) {
        pass[i] = EEPROM.read(EEPROM_WIFI_PASS_OFFSET + i);
    }
    pass[EEPROM_WIFI_PASS_SIZE - 1] = '\0';
    EEPROM.end();
    return valid && strlen(ssid) > 0;
}

static void wifi_creds_save(const char *ssid, const char *pass) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_WIFI_SSID_SIZE; i++) {
        EEPROM.write(EEPROM_WIFI_SSID_OFFSET + i, i < (int)strlen(ssid) ? ssid[i] : 0);
    }
    for (int i = 0; i < EEPROM_WIFI_PASS_SIZE; i++) {
        EEPROM.write(EEPROM_WIFI_PASS_OFFSET + i, i < (int)strlen(pass) ? pass[i] : 0);
    }
    EEPROM.commit();
    EEPROM.end();
    console.printf("[WIFI] Credenciais salvas no EEPROM: %s\n", ssid);
}

// WiFi network configuration (DHCP vs static IP). Stored in EEPROM.
static void wifi_net_load(int *mode, char *ip, char *gw, char *mask, char *dns) {
    EEPROM.begin(EEPROM_SIZE);
    *mode = EEPROM.read(EEPROM_WIFI_MODE_OFFSET) == WIFI_MODE_STATIC ? WIFI_MODE_STATIC : WIFI_MODE_DHCP;
    auto read_str = [](int off, int size, char *buf) {
        int pos = 0;
        for (int i = 0; i < size - 1; i++) {
            uint8_t c = EEPROM.read(off + i);
            if (c == 0) break;
            if (c < 32 || c > 126) break;
            buf[pos++] = (char)c;
        }
        buf[pos] = '\0';
    };
    read_str(EEPROM_WIFI_IP_OFFSET, EEPROM_WIFI_IP_SIZE, ip);
    read_str(EEPROM_WIFI_GW_OFFSET, EEPROM_WIFI_GW_SIZE, gw);
    read_str(EEPROM_WIFI_MASK_OFFSET, EEPROM_WIFI_MASK_SIZE, mask);
    read_str(EEPROM_WIFI_DNS_OFFSET, EEPROM_WIFI_DNS_SIZE, dns);
    EEPROM.end();
}

static void wifi_net_save(int mode, const char *ip, const char *gw, const char *mask, const char *dns) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_WIFI_MODE_OFFSET, mode == WIFI_MODE_STATIC ? WIFI_MODE_STATIC : WIFI_MODE_DHCP);
    auto write_str = [](int off, int size, const char *s) {
        for (int i = 0; i < size; i++) {
            EEPROM.write(off + i, i < (int)strlen(s) ? s[i] : 0);
        }
    };
    write_str(EEPROM_WIFI_IP_OFFSET, EEPROM_WIFI_IP_SIZE, ip ? ip : "");
    write_str(EEPROM_WIFI_GW_OFFSET, EEPROM_WIFI_GW_SIZE, gw ? gw : "");
    write_str(EEPROM_WIFI_MASK_OFFSET, EEPROM_WIFI_MASK_SIZE, mask ? mask : "");
    write_str(EEPROM_WIFI_DNS_OFFSET, EEPROM_WIFI_DNS_SIZE, dns ? dns : "");
    EEPROM.commit();
    EEPROM.end();
    console.printf("[WIFI] Network config salva: mode=%d\n", mode);
}

// Apply a static IP configuration to the STA interface before connecting.
// No-op when in DHCP mode or when the stored IP is invalid.
static void apply_wifi_static_ip() {
    int mode = WIFI_MODE_DHCP;
    char ip[EEPROM_WIFI_IP_SIZE], gw[EEPROM_WIFI_GW_SIZE];
    char mask[EEPROM_WIFI_MASK_SIZE], dns[EEPROM_WIFI_DNS_SIZE];
    wifi_net_load(&mode, ip, gw, mask, dns);
    if (mode != WIFI_MODE_STATIC || strlen(ip) == 0) return;
    IPAddress ipa, gwa, maska, dnsa;
    if (!ipa.fromString(ip)) {
        console.println("[WIFI] Static IP invalido, usando DHCP");
        return;
    }
    maska = IPAddress(255, 255, 255, 0);
    if (mask[0]) maska.fromString(mask);
    gwa = INADDR_NONE;
    if (gw[0]) gwa.fromString(gw);
    dnsa = gwa;
    if (dns[0]) dnsa.fromString(dns);
    WiFi.config(ipa, gwa, maska, dnsa);
    console.printf("[WIFI] Static IP aplicado: %s gw %s mask %s dns %s\n",
                   ip, gw, mask[0] ? mask : "255.255.255.0", dns[0] ? dns : (gw[0] ? gw : "none"));
}

#define PAIRING_ENABLED_DEFAULT false

// Cached pairing config — read once from EEPROM at boot
static int s_pairing_enabled = -1;  // -1 = not loaded yet

static bool pairing_config_load() {
    if (s_pairing_enabled >= 0) return s_pairing_enabled == 1;
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_PAIRING_EN_OFFSET);
    if (val == 1) { EEPROM.end(); s_pairing_enabled = 1; return true; }
    if (val == 0) { EEPROM.end(); s_pairing_enabled = 0; return false; }
    EEPROM.write(EEPROM_PAIRING_EN_OFFSET, 0);
    EEPROM.commit();
    EEPROM.end();
    s_pairing_enabled = 0;
    return false;
}

static void pairing_config_save(bool enabled) {
    s_pairing_enabled = enabled ? 1 : 0;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_PAIRING_EN_OFFSET, enabled ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

int getModeOperStrategy() {
#if defined(ESPNOW_ENABLED) && defined(TCP_ENABLED)
    return OP_MODE_HYBRID;
#elif defined(ESPNOW_ENABLED)
    return OP_MODE_AP;
#else
    return OP_MODE_TERMINAL;
#endif
}

// Cached operation mode — read once from EEPROM at boot, never re-read in loop.
// Avoids excessive EEPROM.begin()/end()/commit() calls that wear out NVS flash.
static int s_op_mode = -1;  // -1 = not loaded yet

int op_mode_load() {
    if (s_op_mode >= 0) return s_op_mode;
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_OP_MODE_OFFSET);
    EEPROM.end();
    if (val <= OP_MODE_HYBRID) {
        s_op_mode = val;
    } else {
        // Invalid: write strategy default
        s_op_mode = getModeOperStrategy();
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.write(EEPROM_OP_MODE_OFFSET, (uint8_t)s_op_mode);
        EEPROM.commit();
        EEPROM.end();
    }
    console.printf("[MODE] Modo de operacao: %d (%s)\n", s_op_mode,
        s_op_mode == OP_MODE_TERMINAL ? "Terminal" :
        s_op_mode == OP_MODE_AP ? "AP" : "Hibrido");
    return s_op_mode;
}

void op_mode_save(int mode) {
    if (mode < OP_MODE_TERMINAL || mode > OP_MODE_HYBRID) mode = getModeOperStrategy();
    s_op_mode = mode;  // update cache
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_OP_MODE_OFFSET, mode);
    EEPROM.commit();
    EEPROM.end();
    console.printf("[MODE] Modo de operacao salvo: %d (%s)\n", mode,
        mode == OP_MODE_TERMINAL ? "Terminal" :
        mode == OP_MODE_AP ? "Ponto de Acesso" : "Hibrido");
}

void web_server_init() {
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (s_wifi_config_mode) request->send_P(200, "text/html", PAGE_PORTAL);
        else request->send_P(200, "text/html", PAGE_SHELL);
    });

    s_server.on("/overview", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", PAGE_OVERVIEW);
    });

    s_server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", PAGE_SETTINGS);
    });

    s_server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", PAGE_LOGS);
    });

    s_server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", PAGE_UPDATE);
    });

    s_server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", log_get_json());
    });

    s_server.on("/api/logs/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_buffer_clear();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/docs", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", PAGE_DOCS);
    });

    s_server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["paired_count"] = sensor_registry_count_paired();
        doc["online_count"] = sensor_registry_count_online();
        doc["rx_total"] = s_radio_mgr.total_rx_count();
        doc["ack_total"] = s_radio_mgr.total_ack_count();
        doc["crc_errors"] = s_radio_mgr.total_crc_errors();
        doc["uptime_ms"] = millis();
        doc["pairing_mode"] = s_radio_mgr.any_pairing_active();
        doc["pairing_required"] = pairing_config_load();
        doc["pairing_window_sec"] = PAIRING_WINDOW_MS / 1000;
        {
            RadioInterface* r = s_radio_mgr.get_radio(RADIO_ESPNOW);
            doc["pairing_remaining_sec"] = r ? r->pairing_remaining_ms() / 1000 : 0;
        }
        doc["mqtt_host"] = mqtt_client_get_host();
        doc["mqtt_port"] = mqtt_client_get_port();
        doc["mqtt_user"] = mqtt_client_get_user();
        doc["mqtt_connected"] = mqtt_client_is_connected();
        doc["mqtt_connected_since"] = mqtt_client_connected_since();
        uint8_t mac_buf[6];
        {
            RadioInterface* r = s_radio_mgr.get_radio(RADIO_ESPNOW);
            if (r && r->get_radio_mac()) {
                memcpy(mac_buf, r->get_radio_mac(), 6);
            } else {
                WiFi.macAddress(mac_buf);
            }
        }
        char mac_str[18];
        mac_to_str(mac_buf, mac_str, sizeof(mac_str));
        doc["gateway_mac"] = mac_str;
        doc["gateway_id"] = get_gateway_device_id();
        doc["fw_version"] = FW_VERSION;
        doc["op_mode"] = op_mode_load();
#if defined(ARDUINO_ARCH_ESP32)
        doc["platform"] = "esp32";
#else
        doc["platform"] = "esp8266";
#endif
        doc["free_heap"] = ESP.getFreeHeap();
        doc["max_sensors"] = MAX_VIRTUAL_SENSORS;
        doc["ntp_synced"] = gateway_ntp_synced();
        doc["epoch"] = (uint32_t)gateway_ntp_epoch();
        doc["ip"] = WiFi.localIP().toString();
        doc["wifi_channel"] = WiFi.channel();
        doc["wifi_ssid"] = WiFi.SSID();
        doc["wifi_rssi"] = WiFi.RSSI();
        {
            int wmode = WIFI_MODE_DHCP;
            char wip[EEPROM_WIFI_IP_SIZE], wgw[EEPROM_WIFI_GW_SIZE];
            char wmask[EEPROM_WIFI_MASK_SIZE], wdns[EEPROM_WIFI_DNS_SIZE];
            wifi_net_load(&wmode, wip, wgw, wmask, wdns);
            doc["wifi_mode"] = wmode;
        }
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/time", [](AsyncWebServerRequest *request, JsonVariant json) {
        if (!json.isNull() && json["epoch"].is<uint32_t>()) {
            gateway_set_browser_epoch((time_t)json["epoch"].as<uint32_t>());
            console.printf("[TIME] hora ajustada via browser (epoch=%u)\n", json["epoch"].as<uint32_t>());
        }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));


    s_server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();

        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired) {
                JsonObject obj = arr.add<JsonObject>();
                obj["slot"] = s->slot;
                char mac_str[18];
                mac_to_str(s->mac, mac_str, sizeof(mac_str));
                obj["mac"] = mac_str;
                for (int j = 0; j < 6; j++) obj["mac_bytes"].add(s->mac[j]);
                obj["type"] = s->type;
                obj["type_name"] = sensor_type_to_string(s->type);
                obj["radio_type"] = s->radio_type;
                obj["client_chip"] = s->client_chip;
                obj["name"] = s->name;
                obj["bridge_device_id"] = s->bridge_device_id;
                if (s->fw_version[0]) {
                    obj["fw_version"] = s->fw_version;
                }
                obj["sequence"] = s->sequence;
                obj["battery_pct"] = s->battery_pct;
                obj["last_rssi"] = s->last_rssi;
                obj["free_heap"] = s->free_heap;
                obj["last_seen"] = (s->last_seen > 0) ? (long)(millis() - s->last_seen) : -1;
                obj["online"] = s->online;
                obj["paired"] = s->paired;
                if (s->ip[0] || s->ip[1] || s->ip[2] || s->ip[3]) {
                    char ip_str[16];
                    sprintf(ip_str, "%d.%d.%d.%d", s->ip[0], s->ip[1], s->ip[2], s->ip[3]);
                    obj["ip"] = ip_str;
                }

                JsonObject state = obj["state"].to<JsonObject>();
                switch (s->type) {
                    case SENSOR_TYPE_TEMP_HUM:
                        state["temperature"] = s->state.temp_hum.temperature;
                        state["humidity"] = s->state.temp_hum.humidity;
                        break;
                    case SENSOR_TYPE_CONTACT:
                        state["contact"] = s->state.contact.contact_state;
                        state["tamper"] = s->state.contact.tamper;
                        break;
                    case SENSOR_TYPE_MOTION:
                        state["occupancy"] = s->state.motion.motion_state;
                        state["duration"] = s->state.motion.occupancy_duration;
                        break;
                    case SENSOR_TYPE_GAS:
                        state["gas_level"] = s->state.gas.gas_level;
                        state["alarm"] = s->state.gas.alarm;
                        break;
                    case SENSOR_TYPE_RAIN:
                        state["rain_level"] = s->state.rain.rain_level;
                        state["rain_digital"] = s->state.rain.rain_digital;
                        break;
                    case SENSOR_TYPE_LEVEL:
                        state["level_pct"] = s->state.tank.level_pct;
                        state["distance_cm"] = s->state.tank.distance_cm;
                        break;
                    case SENSOR_TYPE_DHT_GAS:
                        state["temperature"] = s->state.dht_gas.temperature;
                        state["humidity"] = s->state.dht_gas.humidity;
                        state["gas_level"] = s->state.dht_gas.gas_level;
                        state["alarm"] = s->state.dht_gas.alarm;
                        break;
                    case SENSOR_TYPE_ONOFF:
                    case SENSOR_TYPE_LIGHT:
                        state["state"] = s->state.onoff.state;
                        break;
                    case SENSOR_TYPE_SOIL_MOISTURE:
                        state["moisture_pct"] = s->state.soil_moisture.moisture_pct;
                        state["raw_adc"] = s->state.soil_moisture.raw_adc;
                        break;
                    case SENSOR_TYPE_REPEATER:
                        state["received"] = s->state.repeater.received;
                        state["forwarded"] = s->state.repeater.forwarded;
                        state["client_count"] = s->state.repeater.client_count;
                        state["channel"] = s->state.repeater.channel;
                        state["uptime_s"] = s->state.repeater.uptime_s;
                        state["free_heap"] = s->state.repeater.free_heap;
                        state["ack_failures"] = s->state.repeater.ack_failures;
                        break;
                }
            }
        }

        String json;
        serializeJson(doc, json);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });

    s_server.on("/api/pair/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (s_radio_mgr.any_pairing_active()) {
            request->send(409, "application/json", "{\"error\":\"already pairing\"}");
        } else if (s_radio_mgr.any_start_pairing()) {
            log_add("info", "Pareamento iniciado");
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"max sensors reached\"}");
        }
    });

    s_server.on("/api/pair/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        s_radio_mgr.all_stop_pairing();
        log_add("info", "Pareamento finalizado");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/api/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
        sensor_registry_clear_all();
        log_add("warn", "Todos os sensores removidos");
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/api/broadcast", HTTP_POST, [](AsyncWebServerRequest *request) {
        mqtt_client_publish_all();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/sensor/*", [](AsyncWebServerRequest *request, JsonVariant json) {
        // Parse /api/sensor/{slot}/{action} from URL
        String url = request->url();
        int p1 = url.indexOf('/', 11); // slash before slot (after "/api/sensor/")
        int p2 = url.indexOf('/', p1 + 1);
        if (p1 < 0 || p2 < 0) {
            request->send(400, "application/json", "{\"error\":\"invalid path\"}");
            return;
        }
        int slot = url.substring(p1 + 1, p2).toInt();
        String action = url.substring(p2 + 1);

        if (action == "name") {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, json.as<String>());
            if (err || !doc.containsKey("name")) {
                request->send(400, "application/json", "{\"error\":\"invalid json or missing name\"}");
                return;
            }
            const char *name = doc["name"];
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                request->send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            strncpy(s->name, name, sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            sensor_registry_save();
            mqtt_client_publish_discovery(s);
            log_add("info", "Sensor slot %d renomeado para \"%s\"", slot, name);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else if (action == "command") {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, json.as<String>());
            if (err || !doc.containsKey("state")) {
                request->send(400, "application/json", "{\"error\":\"state required\"}");
                return;
            }
            uint8_t state = doc["state"] ? 1 : 0;
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                request->send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) {
                request->send(400, "application/json", "{\"error\":\"sensor type not supported\"}");
                return;
            }
            if (device_send_command(s->mac, slot, state)) {
                log_add("info", "Comando %s enviado para slot %d", state ? "ON" : "OFF", slot);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else
                request->send(500, "application/json", "{\"error\":\"send failed\"}");
        } else if (action == "restart") {
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                request->send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            if (device_send_restart(s->mac, s->slot)) {
                log_add("info", "Restart enviado para slot %d", slot);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else
                request->send(500, "application/json", "{\"error\":\"send failed\"}");
        } else if (action == "remove") {
            if (sensor_registry_remove(slot)) {
                log_add("warn", "Sensor slot %d removido", slot);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                request->send(404, "application/json", "{\"error\":\"sensor not found\"}");
            }
        } else {
            request->send(404, "application/json", "{\"error\":\"unknown action\"}");
        }
    }));

    s_server.on("/api/config/mqtt", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["host"] = mqtt_client_get_host();
        doc["port"] = mqtt_client_get_port();
        doc["user"] = mqtt_client_get_user();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/mqtt", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err || !doc.containsKey("host") || !doc.containsKey("port")) {
            request->send(400, "application/json", "{\"error\":\"host and port required\"}");
            return;
        }
        const char *host = doc["host"];
        uint16_t port = doc["port"];
        const char *user = doc["user"] | "";
        const char *pass = doc["pass"] | "";
        mqtt_client_save_config(host, port, user, pass);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    s_server.on("/api/config/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
        char ssid[EEPROM_WIFI_SSID_SIZE];
        char pass[EEPROM_WIFI_PASS_SIZE];
        bool have_creds = wifi_creds_load(ssid, pass);
        int mode = WIFI_MODE_DHCP;
        char ip[EEPROM_WIFI_IP_SIZE], gw[EEPROM_WIFI_GW_SIZE];
        char mask[EEPROM_WIFI_MASK_SIZE], dns[EEPROM_WIFI_DNS_SIZE];
        wifi_net_load(&mode, ip, gw, mask, dns);
        JsonDocument doc;
        doc["ssid"] = have_creds ? ssid : "";
        doc["mode"] = mode;
        doc["ip"] = ip;
        doc["gateway"] = gw;
        doc["subnet"] = mask;
        doc["dns"] = dns;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/wifi", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err || !doc.containsKey("ssid")) {
            request->send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        const char *ssid = doc["ssid"];
        const char *newpass = doc["pass"] | "";
        int mode = doc["mode"] | WIFI_MODE_DHCP;
        const char *ip = doc["ip"] | "";
        const char *gw = doc["gateway"] | "";
        const char *mask = doc["subnet"] | "";
        const char *dns = doc["dns"] | "";
        if (strlen(ssid) == 0) {
            request->send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        char cur_ssid[EEPROM_WIFI_SSID_SIZE];
        char cur_pass[EEPROM_WIFI_PASS_SIZE];
        wifi_creds_load(cur_ssid, cur_pass);
        const char *pass = (strlen(newpass) > 0) ? newpass : cur_pass;
        wifi_creds_save(ssid, pass);
        wifi_net_save(mode, ip, gw, mask, dns);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        delay(300);
        ESP.restart();
    }));

    s_server.on("/api/config/pairing", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["enabled"] = pairing_config_load();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/pairing", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err || !doc.containsKey("enabled")) {
            request->send(400, "application/json", "{\"error\":\"enabled required\"}");
            return;
        }
        bool enabled = doc["enabled"];
        pairing_config_save(enabled);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    // Telegram config endpoints
    s_server.on("/api/config/telegram", HTTP_GET, [](AsyncWebServerRequest *request) {
        EEPROM.begin(EEPROM_SIZE);
        bool enabled = EEPROM.read(EEPROM_TELEGRAM_EN_OFFSET) == 1;
        char token[EEPROM_TELEGRAM_TOKEN_SIZE];
        char chatid[EEPROM_TELEGRAM_CHATID_SIZE];
        uint32_t poll_ms = 2000;
        uint8_t alerts_lvl_mask = 0x0F;  // All levels enabled by default
        uint16_t alerts_type_mask = 0x03FF;  // All types enabled by default (10 bits)
        
        for (int i = 0; i < EEPROM_TELEGRAM_TOKEN_SIZE - 1; i++) {
            uint8_t c = EEPROM.read(EEPROM_TELEGRAM_TOKEN_OFFSET + i);
            token[i] = (c >= 32 && c <= 126) ? c : 0;
            if (c == 0) break;
        }
        token[EEPROM_TELEGRAM_TOKEN_SIZE - 1] = '\0';
        
        for (int i = 0; i < EEPROM_TELEGRAM_CHATID_SIZE - 1; i++) {
            uint8_t c = EEPROM.read(EEPROM_TELEGRAM_CHATID_OFFSET + i);
            chatid[i] = (c >= 32 && c <= 126) ? c : 0;
            if (c == 0) break;
        }
        chatid[EEPROM_TELEGRAM_CHATID_SIZE - 1] = '\0';
        
        EEPROM.get(EEPROM_TELEGRAM_POLL_OFFSET, poll_ms);
        if (poll_ms < 1000 || poll_ms > 60000) poll_ms = 2000;
        alerts_lvl_mask = EEPROM.read(EEPROM_TELEGRAM_ALERTS_LVL_OFFSET);
        if (alerts_lvl_mask > 0x0F) alerts_lvl_mask = 0x0F;
        EEPROM.get(EEPROM_TELEGRAM_ALERTS_TYPE_OFFSET, alerts_type_mask);
        if (alerts_type_mask > 0x03FF) alerts_type_mask = 0x03FF;
        EEPROM.end();
        
        JsonDocument doc;
        doc["enabled"] = enabled;
        doc["token"] = token;
        doc["chat_id"] = chatid;
        doc["poll_interval_ms"] = poll_ms;
        // Alert levels
        doc["alert_critical"] = (alerts_lvl_mask & 0x01) != 0;
        doc["alert_alert"] = (alerts_lvl_mask & 0x02) != 0;
        doc["alert_warning"] = (alerts_lvl_mask & 0x04) != 0;
        doc["alert_info"] = (alerts_lvl_mask & 0x08) != 0;
        // Alert types
        doc["alert_gas"] = (alerts_type_mask & 0x0001) != 0;
        doc["alert_smoke"] = (alerts_type_mask & 0x0002) != 0;
        doc["alert_offline"] = (alerts_type_mask & 0x0004) != 0;
        doc["alert_reconnect"] = (alerts_type_mask & 0x0008) != 0;
        doc["alert_battery"] = (alerts_type_mask & 0x0010) != 0;
        doc["alert_temperature"] = (alerts_type_mask & 0x0020) != 0;
        doc["alert_humidity"] = (alerts_type_mask & 0x0040) != 0;
        doc["alert_rssi"] = (alerts_type_mask & 0x0080) != 0;
        doc["alert_heap"] = (alerts_type_mask & 0x0100) != 0;
        doc["alert_daily_report"] = (alerts_type_mask & 0x0200) != 0;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/telegram", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err) {
            request->send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }
        
        bool enabled = doc["enabled"] | false;
        const char *token = doc["token"] | "";
        const char *chatid = doc["chat_id"] | "";
        uint32_t poll_ms = doc["poll_interval_ms"] | 2000;
        // Alert levels
        bool alert_critical = doc["alert_critical"] | true;
        bool alert_alert = doc["alert_alert"] | true;
        bool alert_warning = doc["alert_warning"] | true;
        bool alert_info = doc["alert_info"] | true;
        // Alert types
        bool alert_gas = doc["alert_gas"] | true;
        bool alert_smoke = doc["alert_smoke"] | true;
        bool alert_offline = doc["alert_offline"] | true;
        bool alert_reconnect = doc["alert_reconnect"] | true;
        bool alert_battery = doc["alert_battery"] | true;
        bool alert_temperature = doc["alert_temperature"] | true;
        bool alert_humidity = doc["alert_humidity"] | true;
        bool alert_rssi = doc["alert_rssi"] | false;
        bool alert_heap = doc["alert_heap"] | false;
        bool alert_daily_report = doc["alert_daily_report"] | true;
        
        if (poll_ms < 1000 || poll_ms > 60000) poll_ms = 2000;
        
        uint8_t alerts_lvl_mask = 0;
        if (alert_critical) alerts_lvl_mask |= 0x01;
        if (alert_alert) alerts_lvl_mask |= 0x02;
        if (alert_warning) alerts_lvl_mask |= 0x04;
        if (alert_info) alerts_lvl_mask |= 0x08;
        
        uint16_t alerts_type_mask = 0;
        if (alert_gas) alerts_type_mask |= 0x0001;
        if (alert_smoke) alerts_type_mask |= 0x0002;
        if (alert_offline) alerts_type_mask |= 0x0004;
        if (alert_reconnect) alerts_type_mask |= 0x0008;
        if (alert_battery) alerts_type_mask |= 0x0010;
        if (alert_temperature) alerts_type_mask |= 0x0020;
        if (alert_humidity) alerts_type_mask |= 0x0040;
        if (alert_rssi) alerts_type_mask |= 0x0080;
        if (alert_heap) alerts_type_mask |= 0x0100;
        if (alert_daily_report) alerts_type_mask |= 0x0200;
        
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.write(EEPROM_TELEGRAM_EN_OFFSET, enabled ? 1 : 0);
        
        for (int i = 0; i < EEPROM_TELEGRAM_TOKEN_SIZE; i++) {
            EEPROM.write(EEPROM_TELEGRAM_TOKEN_OFFSET + i, i < (int)strlen(token) ? token[i] : 0);
        }
        for (int i = 0; i < EEPROM_TELEGRAM_CHATID_SIZE; i++) {
            EEPROM.write(EEPROM_TELEGRAM_CHATID_OFFSET + i, i < (int)strlen(chatid) ? chatid[i] : 0);
        }
        EEPROM.put(EEPROM_TELEGRAM_POLL_OFFSET, poll_ms);
        EEPROM.write(EEPROM_TELEGRAM_ALERTS_LVL_OFFSET, alerts_lvl_mask);
        EEPROM.put(EEPROM_TELEGRAM_ALERTS_TYPE_OFFSET, alerts_type_mask);
        
        EEPROM.commit();
        EEPROM.end();
        
        console.printf("[TELEGRAM] Config salva: enabled=%d, chat_id=%s, lvl=0x%02X, type=0x%04X\n", enabled, chatid, alerts_lvl_mask, alerts_type_mask);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    }));

    s_server.on("/api/config/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["mode"] = op_mode_load();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/config/mode", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err || !doc.containsKey("mode")) {
            request->send(400, "application/json", "{\"error\":\"mode required\"}");
            return;
        }
        int mode = doc["mode"];
        if (mode < OP_MODE_TERMINAL || mode > OP_MODE_HYBRID) {
            request->send(400, "application/json", "{\"error\":\"invalid mode (0-2)\"}");
            return;
        }
        int cur = op_mode_load();
        if (mode == cur) {
            request->send(200, "application/json", "{\"status\":\"no change\"}");
            return;
        }
        op_mode_save(mode);
        request->send(200, "application/json", "{\"status\":\"ok\",\"restarting\":true}");
        delay(300);
        ESP.restart();
    }));

    s_server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
        log_add("warn", "Reiniciando via web");
        request->send(200, "application/json", "{\"status\":\"restarting\"}");
        delay(500);
        ESP.restart();
    });

    s_server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            bool error = Update.hasError();
            if (error) {
                Update.printError(console);
                request->send(500, "application/json", "{\"status\":\"error\"}");
            } else {
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                delay(500);
                ESP.restart();
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                console.printf("[OTA] Update started: %s (%d bytes)\n", filename.c_str(), request->contentLength());
                if (!Update.begin(request->contentLength())) {
                    Update.printError(console);
                    console.printf("[OTA] Begin failed (no OTA partition space?)\n");
                }
            }
            if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                    console.printf("[OTA] Write failed at %d\n", index + len);
                    Update.printError(console);
                }
            }
            if (final) {
                if (!Update.end()) {
                    Update.printError(console);
                }
            }
        }
    );

    s_server.on("/api/ota", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"deprecated\"}");
    });

    s_server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(204);
    });

    s_server.addHandler(new AsyncCallbackJsonWebHandler("/api/portal/setup", [](AsyncWebServerRequest *request, JsonVariant json) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json.as<String>());
        if (err || !doc.containsKey("ssid")) {
            request->send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        const char *ssid = doc["ssid"];
        const char *pass = doc["pass"] | "";
        int mode = doc["mode"] | WIFI_MODE_DHCP;
        const char *ip = doc["ip"] | "";
        const char *gw = doc["gateway"] | "";
        const char *mask = doc["subnet"] | "";
        const char *dns = doc["dns"] | "";
        if (strlen(ssid) == 0) {
            request->send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        if (mode == WIFI_MODE_STATIC && (strlen(ip) == 0 || strlen(gw) == 0)) {
            request->send(400, "application/json", "{\"error\":\"static ip and gateway required\"}");
            return;
        }
        const char *mqtt_host = doc["mqtt_host"] | "";
        uint16_t mqtt_port = doc["mqtt_port"] | MQTT_PORT_DEFAULT;
        const char *mqtt_user = doc["mqtt_user"] | "";
        const char *mqtt_pass = doc["mqtt_pass"] | "";
        if (strlen(mqtt_host) > 0) {
            mqtt_client_save_config(mqtt_host, mqtt_port, mqtt_user, mqtt_pass);
        }
        wifi_creds_save(ssid, pass);
        wifi_net_save(mode, ip, gw, mask, dns);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        captive_portal_set_submitted();
    }));
    s_server.on("/api/portal/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        int n = WiFi.scanNetworks();
        const int MAX_NET = 64;
        String ssids[MAX_NET];
        int32_t rssis[MAX_NET];
        int encs[MAX_NET];
        int count = 0;
        for (int i = 0; i < n && count < MAX_NET; i++) {
            String s = WiFi.SSID(i);
            if (s.length() == 0) continue; // omit hidden networks
            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (ssids[j] == s) { dup = true; break; }
            }
            if (dup) continue;
            ssids[count] = s;
            rssis[count] = WiFi.RSSI(i);
            encs[count] = (int)WiFi.encryptionType(i);
            count++;
        }
        // sort by RSSI descending (selection sort)
        for (int a = 0; a < count - 1; a++) {
            for (int b = a + 1; b < count; b++) {
                if (rssis[b] > rssis[a]) {
                    int32_t tr = rssis[a]; rssis[a] = rssis[b]; rssis[b] = tr;
                    String ts = ssids[a]; ssids[a] = ssids[b]; ssids[b] = ts;
                    int te = encs[a]; encs[a] = encs[b]; encs[b] = te;
                }
            }
        }
        JsonDocument doc;
        JsonArray arr = doc["networks"].to<JsonArray>();
        for (int i = 0; i < count; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = ssids[i];
            o["rssi"] = rssis[i];
            o["enc"] = encs[i];
        }
        WiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    auto portal_redirect = [](AsyncWebServerRequest *request) {
        request->redirect("/");
    };
    s_server.on("/generate_204", HTTP_GET, portal_redirect);
    s_server.on("/hotspot-detect.html", HTTP_GET, portal_redirect);
    s_server.on("/ncsi.txt", HTTP_GET, portal_redirect);
    s_server.on("/success.html", HTTP_GET, portal_redirect);
    s_server.onNotFound([](AsyncWebServerRequest *request) {
        if (s_wifi_config_mode) {
            request->redirect("/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });

    s_server.begin();
    console.println("[WEB] Server started on port 80");
}

void web_server_loop() {
    // No handleClient() needed — async server handles requests automatically
    if (s_wifi_config_mode) captive_dns_poll();

    if (s_wifi_config_mode && millis() - s_wifi_config_start > 300000) {
        console.println("[WIFI] Config portal timeout, restarting...");
        ESP.restart();
    }
}

void web_server_handle_client() {
    // No-op: async server does not require polling
}

bool web_server_wifi_setup(bool force_portal) {
    int op_mode = op_mode_load();
    console.printf("[WIFI] Operation mode: %d (%s)\n", op_mode,
        op_mode == OP_MODE_TERMINAL ? "Terminal" :
        op_mode == OP_MODE_AP ? "AP" : "Hibrido");

    /* --- Force portal always opens captive portal --- */
    if (force_portal) {
        console.println("[WIFI] Forcing config portal...");
        s_wifi_config_mode = true;
        s_wifi_config_start = millis();
        captive_portal_start();
        web_server_init();
        captive_portal_run();
        s_wifi_config_mode = false;
        return false;
    }

    /* --- Mode 1: Pure AP --- */
    if (op_mode == OP_MODE_AP) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "%s", get_gateway_device_id());
        console.printf("[WIFI] Starting operational AP: %s ch=%d\n", dev_name, AP_CHANNEL);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(dev_name, AP_PASS, AP_CHANNEL);
        console.printf("[WIFI] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        s_wifi_config_mode = false;
        web_server_init();
        return true;
    }

    /* --- Mode 2: Hybrid (AP + STA) --- */
    if (op_mode == OP_MODE_HYBRID) {
        char dev_name[32];
        snprintf(dev_name, sizeof(dev_name), "%s", get_gateway_device_id());
        console.printf("[WIFI] Starting hybrid AP: %s ch=%d\n", dev_name, AP_CHANNEL);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(dev_name, AP_PASS, AP_CHANNEL);
        console.printf("[WIFI] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        // Fall through to STA connect below
    }

    /* --- Mode 0 (Terminal) and Mode 2 (Hybrid STA part): try STA --- */
    char saved_ssid[EEPROM_WIFI_SSID_SIZE];
    char saved_pass[EEPROM_WIFI_PASS_SIZE];
    bool have_creds = wifi_creds_load(saved_ssid, saved_pass);

    /* --- Step 1: Try saved credentials from EEPROM --- */
    if (have_creds) {
        console.printf("[WIFI] Step 1: Connecting to saved (EEPROM): %s\n", saved_ssid);
        if (op_mode == OP_MODE_TERMINAL) WiFi.mode(WIFI_STA);
        // In hybrid, WIFI_AP_STA is already set
        apply_wifi_static_ip();
        WiFi.begin(saved_ssid, saved_pass);
        unsigned long t0 = millis();
        while (millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED) {
            delay(200);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            web_server_init();
            return true;
        }
        console.println("[WIFI] Step 1 failed: saved WiFi");
    }

#ifdef STATIC_WIFI
    /* --- Step 2: Try STATIC_WIFI hardcoded credentials --- */
    if (strlen(WIFI_SSID) > 0) {
        console.printf("[WIFI] Step 2: Trying STATIC_WIFI: %s\n", WIFI_SSID);
        if (op_mode == OP_MODE_TERMINAL) WiFi.mode(WIFI_STA);
        apply_wifi_static_ip();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        unsigned long t0 = millis();
        while (millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED) {
            delay(200);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[WIFI] Connected via STATIC_WIFI! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            web_server_init();
            return true;
        }
        console.println("[WIFI] Step 2 failed: STATIC_WIFI");
    }
#endif

    /* --- Hybrid: STA failed, but AP is already running --- */
    if (op_mode == OP_MODE_HYBRID) {
        console.println("[WIFI] Hybrid: STA connect failed, AP remains active");
        s_wifi_config_mode = false;
        web_server_init();
        return true;  // AP is running, don't restart
    }

    /* --- Terminal: no STA → start captive portal --- */
    console.println("[WIFI] Starting config portal...");
    s_wifi_config_mode = true;
    s_wifi_config_start = millis();
    captive_portal_start();
    web_server_init();
    captive_portal_run();
    s_wifi_config_mode = false;
    return false;
}

void web_server_maintain_wifi() {
    int op_mode = op_mode_load();
    if (op_mode == OP_MODE_AP) return;  // Pure AP: no STA reconnect

    if (WiFi.status() == WL_CONNECTED) {
        if (s_wifi_reconnect_active) {
            s_wifi_reconnect_active = false;
            console.printf("[WIFI] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    static unsigned long last_attempt = 0;

    if (!s_wifi_reconnect_active) {
        if (millis() - last_attempt < 30000) return;
        last_attempt = millis();

        /* Step 1: Try saved EEPROM creds */
        char saved_ssid[EEPROM_WIFI_SSID_SIZE];
        char saved_pass[EEPROM_WIFI_PASS_SIZE];
        bool have_creds = wifi_creds_load(saved_ssid, saved_pass);
        if (have_creds) {
            console.printf("[WIFI] Reconnecting (step 1 - EEPROM): %s\n", saved_ssid);
            WiFi.mode(WIFI_STA);
            apply_wifi_static_ip();
            WiFi.begin(saved_ssid, saved_pass);
        }
#ifdef STATIC_WIFI
        else if (strlen(WIFI_SSID) > 0) {
            /* Step 2: Try STATIC_WIFI */
            console.printf("[WIFI] Reconnecting (step 2 - STATIC_WIFI): %s\n", WIFI_SSID);
            WiFi.mode(WIFI_STA);
            apply_wifi_static_ip();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
#endif
        else {
            console.println("[WIFI] Reconnecting (no credentials)");
            WiFi.mode(WIFI_STA);
            apply_wifi_static_ip();
            WiFi.begin();
        }
        s_wifi_reconnect_active = true;
        s_wifi_reconnect_deadline = millis() + 15000;
    } else if (millis() >= s_wifi_reconnect_deadline) {
        console.println("[WIFI] Reconnect timeout");
        s_wifi_reconnect_active = false;
    }
}
