#include <Arduino.h>
#include "platform.h"
#include <EEPROM.h>
#include <Update.h>
#include <DNSServer.h>
#include <Wire.h>
#include "lora_spi_radio.h"
#include "lora_node_protocol.h"
#include <ArduinoJson.h>

#include "config.h"
#include "pages.h"
#include "lora_protocol.h"
#include "common_console.h"
#include "display.h"

#include "sensor_type.h"
#include "myWiFiManager.h"

static LoraSpiConfig s_lora_cfg = []() {
    LoraSpiConfig cfg;
    cfg.ss = 18; cfg.rst = 14; cfg.dio0 = -1;
    cfg.sck = 5; cfg.miso = 19; cfg.mosi = 27;
    cfg.freq = 868.0; cfg.sf = 10; cfg.bw = 125E3;
    cfg.cr = 7; cfg.preamble = 8; cfg.tx_power = 17;
    return cfg;
}();
static LoraSpiRadio s_radio(s_lora_cfg);
LoraNodeProtocol s_proto(&s_radio);

static WebServer s_server(DASHBOARD_PORT);

bool s_relay = false;
static bool s_gateway_connected = false;
static uint8_t s_my_mac[6];
static uint32_t s_ota_bytes = 0;
static bool s_ota_in_progress = false;
static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;
static bool s_config_portal_active = false;
static unsigned long s_wifi_connect_start = 0;
static unsigned long s_wifi_config_start = 0;
static DNSServer s_dns;

#define EEPROM_NAME_MARKER 0xFF
#define EEPROM_NAME_MARKER_OFF 10
#define EEPROM_NAME_DATA_OFF 11
#define EEPROM_NAME_MAX 48

static bool name_is_valid(const char *s) {
    if (!s || s[0] == '\0') return false;
    for (int i = 0; s[i]; i++) {
        char c = s[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

static void name_save(const char *name) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_NAME_MARKER_OFF, EEPROM_NAME_MARKER);
    for (int i = 0; i < EEPROM_NAME_MAX - 1; i++) {
        EEPROM.write(EEPROM_NAME_DATA_OFF + i, name[i]);
        if (name[i] == '\0') break;
    }
    EEPROM.write(EEPROM_NAME_DATA_OFF + EEPROM_NAME_MAX - 1, '\0');
    EEPROM.commit();
    EEPROM.end();
}

static bool name_load(char *out, size_t max_len) {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t marker = EEPROM.read(EEPROM_NAME_MARKER_OFF);
    bool found = false;
    if (marker == EEPROM_NAME_MARKER) {
        char buf[EEPROM_NAME_MAX];
        for (int i = 0; i < EEPROM_NAME_MAX - 1; i++) {
            buf[i] = EEPROM.read(EEPROM_NAME_DATA_OFF + i);
            if (buf[i] == '\0') break;
        }
        buf[EEPROM_NAME_MAX - 1] = '\0';
        if (name_is_valid(buf)) {
            size_t slen = strlen(buf);
            size_t copy_len = (slen < max_len - 1) ? slen : max_len - 1;
            memcpy(out, buf, copy_len);
            out[copy_len] = '\0';
            found = true;
        }
    }
    EEPROM.end();
    return found;
}

static void set_relay(bool state) {
    s_relay = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : !RELAY_ON);
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_RELAY_STATE, state);
    EEPROM.commit();
    EEPROM.end();
    s_proto.publish_state();
    console.printf("Relay set to %s\n", state ? "ON" : "OFF");
}

static void toggle_relay() {
    set_relay(!s_relay);
}

static uint8_t get_sensor_type() {
    return SENSOR_TYPE_ONOFF;
}

static uint8_t get_sensor_payload(uint8_t *buf, uint8_t max_len) {
    if (max_len < 1) return 0;
    buf[0] = s_relay ? 1 : 0;
    return 1;
}

static void on_command(uint8_t command) {
    set_relay(command ? true : false);
}

static void on_paired(uint8_t slot) {
    (void)slot;
    s_gateway_connected = true;
}

static void handle_root() {
    s_server.send_P(200, "text/html", PAGE_DASHBOARD);
}

static void handle_docs() {
    s_server.send_P(200, "text/html", PAGE_DOCS);
}

static void handle_api_state() {
    JsonDocument doc;
    doc["relay"] = s_relay;
    doc["paired"] = s_proto.is_paired();
    doc["gateway_connected"] = s_gateway_connected;
    doc["device_id"] = s_device_id;
    doc["device_name"] = s_device_name;
    doc["fw_version"] = FW_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["wifi_channel"] = WiFi.channel();
    doc["rssi"] = s_proto.last_rssi();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["uptime_s"] = millis() / 1000;
    doc["rx_count"] = s_proto.rx_count();
    doc["tx_count"] = s_proto.tx_count();
    String json;
    serializeJson(doc, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_relay() {
    toggle_relay();
    JsonDocument doc;
    doc["relay"] = s_relay;
    String json;
    serializeJson(doc, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_settings() {
    if (s_server.method() == HTTP_POST) {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
            return;
        }
        if (doc["device_name"].is<const char *>()) {
            const char *new_name = doc["device_name"];
            if (name_is_valid(new_name) && strcmp(s_device_name, new_name) != 0) {
                strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                s_device_name[sizeof(s_device_name) - 1] = '\0';
                name_save(s_device_name);
                s_proto.set_device_name(s_device_name);
            }
        }
    }
    JsonDocument resp;
    resp["device_name"] = s_device_name;
    String json;
    serializeJson(resp, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_restart() {
    s_server.send(200, "application/json", "{\"ok\":true}");
    delay(100);
    ESP.restart();
}

static void handle_ota() {
    if (!Update.hasError()) {
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    } else {
        s_ota_in_progress = false;
        s_server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

static void handle_ota_upload() {
    HTTPUpload &upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        s_ota_in_progress = true;
        s_ota_bytes = 0;
        if (!Update.begin(ESP.getFreeSketchSpace()))
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        s_ota_bytes += upload.currentSize;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true))
            console.printf("OTA success: %u bytes\n", s_ota_bytes);
        else
            Update.printError(Serial);
    }
}

static void handle_serial(char c) {
    switch (c) {
        case 'h': case 'H': case '?':
            console.printf("\n--- Comandos ---\n");
            console.printf("  h/?   - ajuda\n");
            console.printf("  s     - status\n");
            console.printf("  l     - alterna relé\n");
            console.printf("  p     - re-parear\n");
            console.printf("  r     - reiniciar\n");
            break;
        case 's': case 'S':
            console.printf("\n--- Status ---\n");
            console.printf("  Device:  %s\n", s_device_id);
            console.printf("  Nome:    %s\n", s_device_name);
            console.printf("  Relé:    %s\n", s_relay ? "ON" : "OFF");
            console.printf("  Pareado: %s\n", s_proto.is_paired() ? "Sim" : "Nao");
            console.printf("  RSSI:    %d dBm (LoRa)\n", s_proto.last_rssi());
            console.printf("  IP:      %s\n", WiFi.localIP().toString().c_str());
            console.printf("  WiFi:    %s ch%d\n", WiFi.SSID().c_str(), WiFi.channel());
            console.printf("  Uptime:  %lus\n", millis() / 1000);
            console.printf("  RX/TX:   %lu / %lu\n", s_proto.rx_count(), s_proto.tx_count());
            console.printf("  FW:      %s\n", FW_VERSION);
            break;
        case 'l': case 'L':
            toggle_relay();
            console.printf("  Relé: %s\n", s_relay ? "ON" : "OFF");
            break;
        case 'p': case 'P':
            s_proto.force_repair();
            s_gateway_connected = false;
            console.printf("  Re-pareando...\n");
            break;
        case 'r': case 'R':
            console.printf("  Reiniciando...\n");
            delay(100);
            ESP.restart();
            break;
    }
}

static void handle_api_wifi();

static void start_ap() {
    char ssid[33];
    snprintf(ssid, sizeof(ssid), "LoRa_%.6s", s_device_id + 5);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, WIFI_CONFIG_PORTAL_PASS);
    s_dns.start(53, "*", WiFi.softAPIP());
    console.printf("AP '%s' iniciado, conecte-se para configurar WiFi\n", ssid);
}

static void handle_wifi() {
    mywifi_loop();
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        if (s_config_portal_active) {
            console.printf("WiFi conectado, parando AP\n");
            WiFi.mode(WIFI_STA);
            WiFi.softAPdisconnect(true);
            s_dns.stop();
            s_config_portal_active = false;
        }
        return;
    }

    if (s_config_portal_active) {
        s_dns.processNextRequest();
        if (now - s_wifi_config_start > 600000) {
            console.printf("AP config timeout, reiniciando\n");
            ESP.restart();
        }
        return;
    }

    if (mywifi_state() == WIFI_STATE_PORTAL) {
        if (!s_config_portal_active) {
            console.printf("No WiFi creds, starting AP\n");
            s_config_portal_active = true;
            s_wifi_config_start = now;
            start_ap();
        }
        return;
    }

    if (mywifi_state() != WIFI_STATE_CONNECTING && WiFi.status() != WL_CONNECTED) {
        if (s_wifi_connect_start > 0 && now - s_wifi_connect_start > 120000) {
            console.printf("WiFi timeout, iniciando AP\n");
            s_config_portal_active = true;
            s_wifi_config_start = now;
            start_ap();
            return;
        }
        if (now - s_wifi_connect_start < 30000 || s_wifi_connect_start == 0) {
            if (s_wifi_connect_start == 0) {
                s_wifi_connect_start = now;
                mywifi_begin(false);
            }
        } else if (now - s_wifi_connect_start >= 30000) {
            WiFi.reconnect();
            s_wifi_connect_start = now;
        }
    }
}

static void handle_root_wifi() {
    s_server.send_P(200, "text/html", PAGE_WIFI_CONFIG);
}

static void handle_api_wifi() {
    if (s_server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID();
        doc["configured"] = (WiFi.SSID().length() > 0);
        doc["ap_active"] = s_config_portal_active;
        doc["status"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
        doc["device_name"] = s_device_name;
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
        return;
    }
    if (s_server.method() == HTTP_POST) {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        const char *ssid = doc["ssid"];
        if (!ssid || strlen(ssid) == 0) {
            s_server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
            return;
        }
        const char *pass = doc["password"] | "";
        if (doc["device_name"].is<const char *>()) {
            const char *new_name = doc["device_name"];
            if (name_is_valid(new_name) && strcmp(s_device_name, new_name) != 0) {
                strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                s_device_name[sizeof(s_device_name) - 1] = '\0';
                name_save(s_device_name);
                s_proto.set_device_name(s_device_name);
            }
        }
        console.printf("WiFi credentials received, connecting to %s...\n", ssid);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        if (s_config_portal_active) {
            WiFi.mode(WIFI_STA);
            WiFi.softAPdisconnect(true);
            s_dns.stop();
            s_config_portal_active = false;
        }
        EEPROM.begin(EEPROM_SIZE);
        for (int i = 0; i < EEPROM_WIFI_SSID_SIZE - 1; i++) {
            EEPROM.write(EEPROM_WIFI_SSID_OFFSET + i, ssid[i]);
            if (ssid[i] == '\0') break;
        }
        EEPROM.write(EEPROM_WIFI_SSID_OFFSET + EEPROM_WIFI_SSID_SIZE - 1, '\0');
        for (int i = 0; i < EEPROM_WIFI_PASS_SIZE - 1; i++) {
            EEPROM.write(EEPROM_WIFI_PASS_OFFSET + i, pass[i]);
            if (pass[i] == '\0') break;
        }
        EEPROM.write(EEPROM_WIFI_PASS_OFFSET + EEPROM_WIFI_PASS_SIZE - 1, '\0');
        EEPROM.commit();
        EEPROM.end();
        delay(100);
        WiFi.begin(ssid, pass);
        s_wifi_connect_start = millis();
    }
}

static void init_radio_and_protocol() {
    s_proto.callbacks.get_sensor_type = get_sensor_type;
    s_proto.callbacks.get_sensor_payload = get_sensor_payload;
    s_proto.callbacks.on_command = on_command;
    s_proto.callbacks.on_paired = on_paired;
    s_proto.set_mac(s_my_mac);
    s_proto.set_device_name(s_device_name);

    if (s_radio.init() != 0) {
        console.printf("LoRa init failed\n");
        return;
    }
    console.printf("LoRa initialized\n");
    s_proto.begin();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    console.printf("\n\nLoRa Switch " FW_VERSION " starting...\n");

    mywifi_begin(false);
    s_wifi_connect_start = millis();

    console.begin();
    console.set_banner("LoRa Switch " FW_VERSION);
    console.printf("Console started, telnet to %s:23\n", WiFi.localIP().toString().c_str());

    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    console.printf("Relay pin: %d, LED pin: %d, Button pin: %d\n", RELAY_PIN, LED_PIN, BUTTON_PIN);
    EEPROM.begin(EEPROM_SIZE);
    s_relay = EEPROM.read(EEPROM_RELAY_STATE);
    EEPROM.end();
    digitalWrite(RELAY_PIN, s_relay ? RELAY_ON : !RELAY_ON);

    uint64_t chipid = ESP.getEfuseMac();
    s_my_mac[0] = (chipid >> 40) & 0xFF;
    s_my_mac[1] = (chipid >> 32) & 0xFF;
    s_my_mac[2] = (chipid >> 24) & 0xFF;
    s_my_mac[3] = (chipid >> 16) & 0xFF;
    s_my_mac[4] = (chipid >> 8) & 0xFF;
    s_my_mac[5] = chipid & 0xFF;
    snprintf(s_device_id, sizeof(s_device_id), "agri_%02X%02X%02X%02X%02X%02X",
             s_my_mac[0], s_my_mac[1], s_my_mac[2],
             s_my_mac[3], s_my_mac[4], s_my_mac[5]);

    if (!name_load(s_device_name, sizeof(s_device_name)))
        strncpy(s_device_name, DEVICE_NAME, sizeof(s_device_name) - 1);
    s_proto.set_device_name(s_device_name);

    display_init();

    init_radio_and_protocol();
console.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  s_my_mac[0], s_my_mac[1], s_my_mac[2],
                  s_my_mac[3], s_my_mac[4], s_my_mac[5]);
    s_server.on("/", handle_root);
    s_server.on("/docs", handle_docs);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/relay", HTTP_POST, handle_api_relay);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/wifi", handle_root_wifi);
    s_server.onNotFound([]() {
        if (s_config_portal_active)
            s_server.send_P(200, "text/html", PAGE_WIFI_CONFIG);
        else
            s_server.send(404, "text/plain", "Not Found");
    });
    s_server.begin();
console.printf("HTTP server started on port %d\n", DASHBOARD_PORT);
    digitalWrite(LED_PIN, HIGH);
}

void loop() {
    console.loop();
    handle_wifi();
    s_server.handleClient();
    s_radio.loop();
    s_proto.loop();

    if (Serial.available() > 0) handle_serial(Serial.read());
    int tc = console.telnet_read();
    if (tc >= 0) handle_serial((char)tc);

    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(50);
        if (digitalRead(BUTTON_PIN) == LOW) {
            while (digitalRead(BUTTON_PIN) == LOW) delay(10);
            toggle_relay();
        }
    }

    if (s_proto.is_paired()) {
        digitalWrite(LED_PIN, s_relay ? LOW : HIGH);
    } else {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    display_loop();

    delay(1);
}
