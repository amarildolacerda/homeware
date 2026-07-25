#include "web_server.h"
#include "sensor_registry.h"
#include "espnow_handler.h"
#include "mqtt_client.h"
#include "config.h"
#include "pages.h"
#include "log_buffer.h"
#include "common_console.h"
#include "platform.h"
#include "captive_portal.h"
#include <uri/UriBraces.h>
#include <ArduinoJson.h>

extern bool gateway_ntp_synced();
extern time_t gateway_ntp_epoch();
extern void gateway_set_browser_epoch(time_t epoch);

#include <EEPROM.h>
#ifdef ESP32
#include <Update.h>
#endif

static MyWebServer s_server(80);
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

static bool pairing_config_load() {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_PAIRING_EN_OFFSET);
    if (val == 1) { EEPROM.end(); return true; }
    if (val == 0) { EEPROM.end(); return false; }
    EEPROM.write(EEPROM_PAIRING_EN_OFFSET, 0);
    EEPROM.commit();
    EEPROM.end();
    return false;
}

static void pairing_config_save(bool enabled) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_PAIRING_EN_OFFSET, enabled ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

static void serve_pgm_page(const char* page) {
    size_t total = strlen_P(page);
    WiFiClient cl = s_server.client();
    cl.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "));
    cl.print(total);
    cl.print(F("\r\nConnection: close\r\n\r\n"));
    PGM_P src = page;
    char buf[256];
    while (total > 0) {
        size_t chunk = total > sizeof(buf) ? sizeof(buf) : total;
        memcpy_P(buf, src, chunk);
        cl.write((const uint8_t*)buf, chunk);
        src += chunk;
        total -= chunk;
        yield();
    }
}

void web_server_init() {
    s_server.on("/", HTTP_GET, []() {
        if (s_wifi_config_mode) serve_pgm_page(PAGE_PORTAL);
        else serve_pgm_page(PAGE_SHELL);
    });

    s_server.on("/overview", HTTP_GET, []() {
        serve_pgm_page(PAGE_OVERVIEW);
    });

    s_server.on("/settings", HTTP_GET, []() {
        serve_pgm_page(PAGE_SETTINGS);
    });

    s_server.on("/logs", HTTP_GET, []() {
        serve_pgm_page(PAGE_LOGS);
    });

    s_server.on("/api/logs", HTTP_GET, []() {
        s_server.send(200, "application/json", log_get_json());
    });

    s_server.on("/api/logs/clear", HTTP_POST, []() {
        log_buffer_clear();
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/docs", HTTP_GET, []() {
        serve_pgm_page(PAGE_DOCS);
    });
    
    s_server.on("/api/info", HTTP_GET, []() {
        JsonDocument doc;
        doc["paired_count"] = sensor_registry_count_paired();
        doc["online_count"] = sensor_registry_count_online();
        doc["rx_total"] = espnow_get_rx_count();
        doc["ack_total"] = espnow_get_ack_count();
        doc["crc_errors"] = espnow_get_crc_errors();
        doc["uptime_ms"] = millis();
        doc["pairing_mode"] = espnow_is_pairing();
        doc["pairing_required"] = pairing_config_load();
        doc["pairing_window_sec"] = PAIRING_WINDOW_MS / 1000;
        doc["pairing_remaining_sec"] = espnow_pairing_remaining_ms() / 1000;
        doc["mqtt_host"] = mqtt_client_get_host();
        doc["mqtt_port"] = mqtt_client_get_port();
        doc["mqtt_user"] = mqtt_client_get_user();
        doc["mqtt_connected"] = mqtt_client_is_connected();
        doc["mqtt_connected_since"] = mqtt_client_connected_since();
        char mac_buf[18];
        mac_to_str(espnow_get_gateway_mac(), mac_buf, sizeof(mac_buf));
        doc["gateway_mac"] = mac_buf;
        doc["gateway_id"] = get_gateway_device_id();
        doc["fw_version"] = FW_VERSION;
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
        doc["wifi_ssid"] = WiFi.SSID();
        {
            int wmode = WIFI_MODE_DHCP;
            char wip[EEPROM_WIFI_IP_SIZE], wgw[EEPROM_WIFI_GW_SIZE];
            char wmask[EEPROM_WIFI_MASK_SIZE], wdns[EEPROM_WIFI_DNS_SIZE];
            wifi_net_load(&wmode, wip, wgw, wmask, wdns);
            doc["wifi_mode"] = wmode;
        }
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/time", HTTP_POST, []() {
        if (s_server.hasArg("plain")) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
            if (!err && doc["epoch"].is<uint32_t>()) {
                gateway_set_browser_epoch((time_t)doc["epoch"].as<uint32_t>());
                console.printf("[TIME] hora ajustada via browser (epoch=%u)\n", doc["epoch"].as<uint32_t>());
            }
        }
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    
    s_server.on("/api/sensors", HTTP_GET, []() {
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
                obj["name"] = s->name;
                obj["bridge_device_id"] = s->bridge_device_id;
                obj["sequence"] = s->sequence;
                obj["battery_pct"] = s->battery_pct;
                obj["last_rssi"] = s->last_rssi;
                obj["free_heap"] = s->free_heap;
                obj["last_seen"] = (s->online && s->last_seen > 0) ? (long)(millis() - s->last_seen) : -1;
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
                    case SENSOR_TYPE_TANK:
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
        s_server.sendHeader("Access-Control-Allow-Origin", "*");
        s_server.send(200, "application/json", json);
    });
    
    s_server.on("/api/pair/start", HTTP_POST, []() {
        if (espnow_is_pairing()) {
            s_server.send(409, "application/json", "{\"error\":\"already pairing\"}");
        } else if (espnow_start_pairing()) {
            log_add("info", "Pareamento iniciado");
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            s_server.send(400, "application/json", "{\"error\":\"max sensors reached\"}");
        }
    });
    
    s_server.on("/api/pair/stop", HTTP_POST, []() {
        espnow_stop_pairing();
        log_add("info", "Pareamento finalizado");
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/api/clear", HTTP_POST, []() {
        sensor_registry_clear_all();
        log_add("warn", "Todos os sensores removidos");
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    s_server.on("/api/broadcast", HTTP_POST, []() {
        mqtt_client_publish_all();
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    s_server.on(UriBraces("/api/sensor/{}/{}"), HTTP_POST, []() {
        int slot = s_server.pathArg(0).toInt();
        String action = s_server.pathArg(1);
        
        if (action == "name") {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
            if (err || !doc.containsKey("name")) {
                s_server.send(400, "application/json", "{\"error\":\"invalid json or missing name\"}");
                return;
            }
            const char *name = doc["name"];
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            strncpy(s->name, name, sizeof(s->name) - 1);
            s->name[sizeof(s->name) - 1] = '\0';
            sensor_registry_save();
            mqtt_client_publish_discovery(s);
            log_add("info", "Sensor slot %d renomeado para \"%s\"", slot, name);
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else if (action == "command") {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
            if (err || !doc.containsKey("state")) {
                s_server.send(400, "application/json", "{\"error\":\"state required\"}");
                return;
            }
            uint8_t state = doc["state"] ? 1 : 0;
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            if (s->type != SENSOR_TYPE_ONOFF && s->type != SENSOR_TYPE_LIGHT) {
                s_server.send(400, "application/json", "{\"error\":\"sensor type not supported\"}");
                return;
            }
            if (espnow_send_command(s->mac, slot, state)) {
                log_add("info", "Comando %s enviado para slot %d", state ? "ON" : "OFF", slot);
                s_server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else
                s_server.send(500, "application/json", "{\"error\":\"send failed\"}");
        } else if (action == "restart") {
            virtual_sensor_t *s = sensor_registry_get(slot);
            if (!s || !s->paired) {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
                return;
            }
            if (espnow_send_restart(s->mac, s->slot)) {
                log_add("info", "Restart enviado para slot %d", slot);
                s_server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else
                s_server.send(500, "application/json", "{\"error\":\"send failed\"}");
        } else if (action == "remove") {
            if (sensor_registry_remove(slot)) {
                log_add("warn", "Sensor slot %d removido", slot);
                s_server.send(200, "application/json", "{\"status\":\"ok\"}");
            } else {
                s_server.send(404, "application/json", "{\"error\":\"sensor not found\"}");
            }
        } else {
            s_server.send(404, "application/json", "{\"error\":\"unknown action\"}");
        }
    });
    
    s_server.on("/api/config/mqtt", HTTP_GET, []() {
        JsonDocument doc;
        doc["host"] = mqtt_client_get_host();
        doc["port"] = mqtt_client_get_port();
        doc["user"] = mqtt_client_get_user();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/config/mqtt", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("host") || !doc.containsKey("port")) {
            s_server.send(400, "application/json", "{\"error\":\"host and port required\"}");
            return;
        }
        const char *host = doc["host"];
        uint16_t port = doc["port"];
        const char *user = doc["user"] | "";
        const char *pass = doc["pass"] | "";
        mqtt_client_save_config(host, port, user, pass);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });

    s_server.on("/api/config/wifi", HTTP_GET, []() {
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
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/config/wifi", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("ssid")) {
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
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
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        char cur_ssid[EEPROM_WIFI_SSID_SIZE];
        char cur_pass[EEPROM_WIFI_PASS_SIZE];
        wifi_creds_load(cur_ssid, cur_pass);
        const char *pass = (strlen(newpass) > 0) ? newpass : cur_pass;
        wifi_creds_save(ssid, pass);
        wifi_net_save(mode, ip, gw, mask, dns);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(300);
        ESP.restart();
    });

    s_server.on("/api/config/pairing", HTTP_GET, []() {
        JsonDocument doc;
        doc["enabled"] = pairing_config_load();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/config/pairing", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("enabled")) {
            s_server.send(400, "application/json", "{\"error\":\"enabled required\"}");
            return;
        }
        bool enabled = doc["enabled"];
        pairing_config_save(enabled);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
    
    s_server.on("/api/restart", HTTP_POST, []() {
        log_add("warn", "Reiniciando via web");
        s_server.send(200, "application/json", "{\"status\":\"restarting\"}");
        delay(500);
        ESP.restart();
    });
    
    s_server.on("/update", HTTP_POST, []() {
        if (Update.hasError()) {
#ifdef ESP32
            console.printf("[OTA] Error: %s\n", Update.errorString());
#else
            console.printf("[OTA] Error: %s\n", Update.getErrorString().c_str());
#endif
            s_server.send(500, "application/json", "{\"status\":\"error\"}");
            return;
        }
        if (!Update.end()) {
#ifdef ESP32
            console.printf("[OTA] End failed: %s\n", Update.errorString());
#else
            console.printf("[OTA] End failed: %s\n", Update.getErrorString().c_str());
#endif
            s_server.send(500, "application/json", "{\"status\":\"error\"}");
            return;
        }
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    }, []() {
        HTTPUpload &upload = s_server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            console.printf("[OTA] Update started: %s (%d bytes)\n", upload.filename.c_str(), upload.totalSize);
            if (!Update.begin(upload.totalSize)) {
                Update.printError(console);
                console.printf("[OTA] Begin failed (no OTA partition space?)\n");
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                console.printf("[OTA] Write failed at %d\n", upload.currentSize);
                Update.printError(console);
            }
        }
    });

    s_server.on("/api/ota", HTTP_POST, []() {
        s_server.send(200, "application/json", "{\"status\":\"deprecated\"}");
    });
    
    s_server.on("/favicon.ico", HTTP_GET, []() {
        s_server.send(204);
    });

    s_server.on("/api/portal/setup", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("ssid")) {
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
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
            s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
            return;
        }
        if (mode == WIFI_MODE_STATIC && (strlen(ip) == 0 || strlen(gw) == 0)) {
            s_server.send(400, "application/json", "{\"error\":\"static ip and gateway required\"}");
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
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        captive_portal_set_submitted();
    });
    s_server.on("/api/portal/scan", HTTP_GET, []() {
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
        s_server.send(200, "application/json", json);
    });
    auto portal_redirect = []() {
        s_server.sendHeader("Location", "/");
        s_server.send(302, "text/plain", "");
    };
    s_server.on("/generate_204", HTTP_GET, portal_redirect);
    s_server.on("/hotspot-detect.html", HTTP_GET, portal_redirect);
    s_server.on("/ncsi.txt", HTTP_GET, portal_redirect);
    s_server.on("/success.html", HTTP_GET, portal_redirect);
    s_server.onNotFound([]() {
        if (s_wifi_config_mode) {
            s_server.sendHeader("Location", "/");
            s_server.send(302, "text/plain", "");
        } else {
            s_server.send(404, "text/plain", "Not found");
        }
    });

    s_server.begin();
    console.println("[WEB] Server started on port 80");
}

void web_server_loop() {
    s_server.handleClient();

    if (s_wifi_config_mode) captive_dns_poll();

    if (s_wifi_config_mode && millis() - s_wifi_config_start > 300000) {
        console.println("[WIFI] Config portal timeout, restarting...");
        ESP.restart();
    }
}

void web_server_handle_client() {
    s_server.handleClient();
}

bool web_server_wifi_setup(bool force_portal) {
    char saved_ssid[EEPROM_WIFI_SSID_SIZE];
    char saved_pass[EEPROM_WIFI_PASS_SIZE];
    bool have_creds = wifi_creds_load(saved_ssid, saved_pass);

#ifdef STATIC_WIFI
    if (strlen(WIFI_SSID) > 0) {
        strncpy(saved_ssid, WIFI_SSID, sizeof(saved_ssid) - 1);
        saved_ssid[sizeof(saved_ssid) - 1] = '\0';
        strncpy(saved_pass, WIFI_PASS, sizeof(saved_pass) - 1);
        saved_pass[sizeof(saved_pass) - 1] = '\0';
        have_creds = true;
        console.println("[WIFI] Usando credenciais STATIC_WIFI");
    }
#endif

    if (!force_portal && have_creds) {
        console.printf("[WIFI] Connecting to saved (EEPROM): %s\n", saved_ssid);
        WiFi.mode(WIFI_STA);
        apply_wifi_static_ip();
        WiFi.begin(saved_ssid, saved_pass);
        unsigned long t0 = millis();
        while (millis() - t0 < 45000 && WiFi.status() != WL_CONNECTED) {
            delay(200);
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            s_wifi_config_mode = false;
            web_server_init();
            return true;
        }
        console.println("[WIFI] Failed to connect to saved WiFi");
    }

    console.println("[WIFI] Starting config portal...");
    s_wifi_config_mode = true;
    s_wifi_config_start = millis();
    captive_portal_start();
    web_server_init();
    captive_portal_run();
    // captive_portal_run() only returns after ESP.restart(); this is a fallback:
    s_wifi_config_mode = false;
    return false;
}

void web_server_maintain_wifi() {
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
        console.println("[WIFI] Reconnecting...");
        WiFi.mode(WIFI_STA);
        apply_wifi_static_ip();
        WiFi.begin();
        s_wifi_reconnect_active = true;
        s_wifi_reconnect_deadline = millis() + 15000;
    } else if (millis() >= s_wifi_reconnect_deadline) {
        console.println("[WIFI] Reconnect timeout");
        s_wifi_reconnect_active = false;
    }
}