#include <Arduino.h>
#include "platform.h"
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include "config.h"
#include "pages.h"
#include "tcp_protocol.h"
#include "sensor_type.h"
#include "myWiFiManager.h"
#include "common_console.h"
#include "common_ota.h"
#include "common_web.h"

static const char *TAG = PLATFORM_PREFIX "_tcp";

// Suppress redefinition warnings from shared_config.h defaults
#undef WIFI_CONFIG_PORTAL_SSID
#undef WIFI_CONFIG_PORTAL_PASS
#define WIFI_CONFIG_PORTAL_SSID "AgriSense-TCP-Setup"
#define WIFI_CONFIG_PORTAL_PASS "agrisense"

static ESP8266WebServer s_server(80);
static WiFiUDP s_udp;

static unsigned long s_start_time = 0;
static unsigned long s_last_state_send = 0;
static unsigned long s_last_heartbeat = 0;
static unsigned long s_last_reconnect = 0;
static unsigned long s_last_discovery = 0;
static unsigned long s_last_led_toggle = 0;

static int s_retry_count = 0;
static int s_discovery_retries = 0;
static int s_http_fallback_retries = 0;
static bool s_registered = false;
static bool s_hub_found = false;
static bool s_hub_ip_configured = false;
static int s_assigned_slot = -1;
static char s_hub_ip[16] = HUB_IP_DEFAULT;
static uint16_t s_hub_port = HUB_PORT;

static unsigned long s_tx_count = 0;
static unsigned long s_rx_count = 0;

static float s_temperature = 0;
static float s_humidity = 0;
static bool s_sensor_valid = false;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static void save_hub_ip_to_eeprom(const char *ip) {
    EEPROM.begin(EEPROM_SIZE);
    int len = strlen(ip);
    if (len >= 16) len = 15;
    for (int i = 0; i < 16; i++) {
        EEPROM.write(EEPROM_HUB_IP_OFFSET + i, i < len ? ip[i] : 0);
    }
    EEPROM.write(EEPROM_HUB_IP_VALID, 0x01);
    EEPROM.commit();
    EEPROM.end();
    console.printf("[tcp] Hub IP saved to EEPROM: %s\n", ip);
}

static bool load_hub_ip_from_eeprom(char *buf, size_t len) {
    EEPROM.begin(EEPROM_SIZE);
    if (EEPROM.read(EEPROM_HUB_IP_VALID) != 0x01) {
        EEPROM.end();
        return false;
    }
    int pos = 0;
    for (int i = 0; i < (int)len - 1; i++) {
        uint8_t c = EEPROM.read(EEPROM_HUB_IP_OFFSET + i);
        if (c == 0) break;
        if (c < 32 || c > 126) { pos = 0; break; }
        buf[pos++] = (char)c;
    }
    buf[pos] = '\0';
    EEPROM.end();
    return pos > 0;
}

static void read_sensor() {
    switch (SENSOR_TYPE) {
        case SENSOR_TYPE_TEMP_HUM:
            s_temperature = 25.0 + random(-50, 50) / 10.0;
            s_humidity = 60.0 + random(-100, 100) / 10.0;
            s_sensor_valid = true;
            break;
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS:
            s_sensor_valid = true;
            break;
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT:
            s_sensor_valid = true;
            break;
        default:
            s_sensor_valid = false;
            break;
    }
}

static void send_udp_discover() {
    tcp_gw_discover_t discover;
    memset(&discover, 0, sizeof(discover));
    discover.msg_type = MSG_GW_DISCOVER;
    discover.sensor_type = SENSOR_TYPE;
    strncpy(discover.device_name, s_device_name, sizeof(discover.device_name) - 1);

    IPAddress broadcast(255, 255, 255, 255);
    s_udp.beginPacket(broadcast, TCP_UDP_PORT);
    s_udp.write((uint8_t *)&discover, sizeof(discover));
    s_udp.endPacket();
    s_tx_count++;

    console.printf("[%s] Sent UDP discover (attempt %d)\n", TAG, s_discovery_retries);
}

static void handle_udp_announce() {
    int packetSize = s_udp.parsePacket();
    if (packetSize <= 0) return;

    uint8_t buf[64];
    int len = s_udp.read(buf, sizeof(buf));
    if (len < (int)sizeof(tcp_gw_announce_t)) return;

    if (buf[0] != MSG_GW_ANNOUNCE) return;

    tcp_gw_announce_t *announce = (tcp_gw_announce_t *)buf;

    memset(s_hub_ip, 0, sizeof(s_hub_ip));
    strncpy(s_hub_ip, announce->hub_ip, sizeof(s_hub_ip) - 1);
    s_hub_port = announce->hub_port;
    s_hub_found = true;
    s_rx_count++;

    console.printf("[%s] Hub found: %s:%d\n", TAG, s_hub_ip, s_hub_port);
    console.printf("[%s] Hub FW: %d.%d.%d.%d\n", TAG,
                   announce->fw_version[0], announce->fw_version[1],
                   announce->fw_version[2], announce->fw_version[3]);
}

static bool send_to_hub(const char *endpoint, const String &payload) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient client;
    HTTPClient http;

    String url = String("http://") + s_hub_ip + ":" + String(s_hub_port) + endpoint;

    if (http.begin(client, url)) {
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(HTTP_TIMEOUT_MS);
        int httpCode = http.POST(payload);

        if (httpCode > 0) {
            console.printf("[tcp] %s -> %d\n", endpoint, httpCode);
            s_tx_count++;
            if (httpCode == 200) s_rx_count++;
            http.end();
            return httpCode == 200;
        } else {
            console.printf("[tcp] %s failed: %s\n", endpoint, http.errorToString(httpCode).c_str());
        }
        http.end();
    }

    return false;
}

static bool register_with_hub() {
    JsonDocument doc;
    doc["device_id"] = s_device_id;
    doc["sensor_type"] = (int)SENSOR_TYPE;
    doc["device_name"] = s_device_name;
    doc["fw_version"] = FW_VERSION;

    String payload;
    serializeJson(doc, payload);

    if (send_to_hub("/node/register", payload)) {
        s_registered = true;
        s_retry_count = 0;
        console.println("[tcp] Registered with hub");
        return true;
    }

    s_retry_count++;
    return false;
}

static void send_state() {
    if (!s_registered || WiFi.status() != WL_CONNECTED) return;

    read_sensor();

    JsonDocument doc;
    doc["device_id"] = s_device_id;
    doc["device_name"] = s_device_name;
    doc["fw_version"] = FW_VERSION;
    doc["uptime_s"] = (millis() - s_start_time) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["tx_count"] = s_tx_count;
    doc["rx_count"] = s_rx_count;
    doc["ip"] = WiFi.localIP().toString();
    doc["hub_ip"] = s_hub_ip;
    doc["slot"] = s_assigned_slot;

    switch (SENSOR_TYPE) {
        case SENSOR_TYPE_TEMP_HUM:
            doc["temperature"] = s_temperature;
            doc["humidity"] = s_humidity;
            break;
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS:
            doc["gas_level"] = 0;
            doc["alarm"] = false;
            break;
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT:
            doc["relay_state"] = false;
            break;
        default:
            break;
    }

    String payload;
    serializeJson(doc, payload);

    if (send_to_hub("/node/state", payload)) {
        s_last_state_send = millis();
        s_retry_count = 0;
    } else {
        s_retry_count++;
    }
}

static void send_heartbeat() {
    if (!s_registered || WiFi.status() != WL_CONNECTED) return;

    JsonDocument doc;
    doc["device_id"] = s_device_id;

    String payload;
    serializeJson(doc, payload);

    if (send_to_hub("/node/heartbeat", payload)) {
        s_last_heartbeat = millis();
    }
}

static void check_commands() {
    if (!s_registered || WiFi.status() != WL_CONNECTED) return;

    WiFiClient client;
    HTTPClient http;

    String url = String("http://") + s_hub_ip + ":" + String(s_hub_port) + "/node/command/" + s_device_id;

    if (http.begin(client, url)) {
        http.setTimeout(HTTP_TIMEOUT_MS);
        int httpCode = http.GET();

        if (httpCode == 200) {
            s_rx_count++;
            String response = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);

            if (!error && doc["command"].is<const char *>()) {
                const char *command = doc["command"];
                console.printf("[tcp] Received command: %s\n", command);

                if (strcmp(command, "restart") == 0) {
                    console.println("[tcp] Restarting...");
                    delay(100);
                    ESP.restart();
                } else if (strcmp(command, "on") == 0) {
                    console.println("[tcp] ON command received");
                } else if (strcmp(command, "off") == 0) {
                    console.println("[tcp] OFF command received");
                }
            }
        }
        http.end();
    }
}

static void handle_root() {
    serve_pgm_page(s_server, PAGE_DASHBOARD);
}

static void handle_docs() {
    serve_pgm_page(s_server, PAGE_DOCS);
}

static void handle_wifi_page() {
    serve_pgm_page(s_server, PAGE_WIFI_CONFIG);
}

static void handle_api_state() {
    JsonDocument doc;
    doc["device_id"] = s_device_id;
    doc["device_name"] = s_device_name;
    doc["fw_version"] = FW_VERSION;
    doc["uptime_s"] = (millis() - s_start_time) / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["tx_count"] = s_tx_count;
    doc["rx_count"] = s_rx_count;
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["hub_ip"] = s_hub_ip;
    doc["slot"] = s_assigned_slot;
    doc["online"] = s_registered;

    switch (SENSOR_TYPE) {
        case SENSOR_TYPE_TEMP_HUM:
            doc["sensor_value"] = s_temperature;
            doc["sensor_unit"] = "\xC2\xB0" "C";
            break;
        case SENSOR_TYPE_GAS:
        case SENSOR_TYPE_DHT_GAS:
            doc["sensor_value"] = 0;
            doc["sensor_unit"] = "ppm";
            break;
        case SENSOR_TYPE_ONOFF:
        case SENSOR_TYPE_LIGHT:
            doc["sensor_value"] = 0;
            doc["sensor_unit"] = "";
            break;
        default:
            doc["sensor_value"] = 0;
            doc["sensor_unit"] = "";
            break;
    }

    String response;
    serializeJson(doc, response);
    s_server.send(200, "application/json", response);
}

static void handle_api_settings() {
    if (s_server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["device_name"] = s_device_name;
        String response;
        serializeJson(doc, response);
        s_server.send(200, "application/json", response);
    } else if (s_server.method() == HTTP_POST) {
        if (s_server.hasArg("plain")) {
            JsonDocument doc;
            deserializeJson(doc, s_server.arg("plain"));
            if (doc["device_name"].is<const char *>()) {
                String name = doc["device_name"].as<String>();
                if (name.length() > 0 && name.length() < 32) {
                    strncpy(s_device_name, name.c_str(), sizeof(s_device_name) - 1);
                    s_device_name[sizeof(s_device_name) - 1] = '\0';
                    s_server.send(200, "application/json", "{\"ok\":true}");
                    return;
                }
            }
            s_server.send(400, "application/json", "{\"error\":\"invalid\"}");
        } else {
            s_server.send(400, "application/json", "{\"error\":\"no body\"}");
        }
    }
}

static void handle_api_wifi() {
    if (s_server.method() == HTTP_GET) {
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["status"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
        String response;
        serializeJson(doc, response);
        s_server.send(200, "application/json", response);
    } else if (s_server.method() == HTTP_POST) {
        if (s_server.hasArg("plain")) {
            JsonDocument doc;
            deserializeJson(doc, s_server.arg("plain"));
            if (doc["ssid"].is<const char *>() && doc["password"].is<const char *>()) {
                s_server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"restarting\"}");
                delay(100);
                ESP.restart();
            } else {
                s_server.send(400, "application/json", "{\"error\":\"missing ssid or password\"}");
            }
        } else {
            s_server.send(400, "application/json", "{\"error\":\"no body\"}");
        }
    }
}

static void handle_api_ota() {
    s_server.send(501, "application/json", "{\"error\":\"not implemented\"}");
}

static void handle_api_restart() {
    s_server.send(200, "application/json", "{\"status\":\"restarting\"}");
    delay(100);
    ESP.restart();
}

static void handle_serial(char c) {
    switch (c) {
    case 'r':
    case 'R':
        ESP.restart();
        break;
    case 's':
    case 'S': {
        unsigned long up = (millis() - s_start_time) / 1000;
        console.printf("\n--- Status ---\n");
        console.printf("  Device:    %s\n", s_device_id);
        console.printf("  Nome:      %s\n", s_device_name);
        console.printf("  FW:        %s\n", FW_VERSION);
        console.printf("  Hub:       %s:%d\n", s_hub_ip, s_hub_port);
        console.printf("  Registrado:%s\n", s_registered ? "sim" : "nao");
        console.printf("  Slot:      %d\n", s_assigned_slot);
        console.printf("  Sensor:    %d\n", SENSOR_TYPE);
        console.printf("  IP:        %s\n", WiFi.localIP().toString().c_str());
        console.printf("  RSSI:      %d dBm\n", WiFi.RSSI());
        console.printf("  TX:        %lu\n", s_tx_count);
        console.printf("  RX:        %lu\n", s_rx_count);
        console.printf("  Heap:      %d\n", ESP.getFreeHeap());
        console.printf("  Uptime:    %lu s\n", up);
        console.printf("-------------\n\n");
        break;
    }
    case 'l':
    case 'L':
        console.printf("\n--- Leitura forcada ---\n");
        read_sensor();
        if (SENSOR_TYPE == SENSOR_TYPE_TEMP_HUM) {
            console.printf("  Temp:  %.1f C\n", s_temperature);
            console.printf("  Hum:   %.1f %%\n", s_humidity);
        }
        console.printf("-------------------------\n\n");
        break;
    case 'd':
    case 'D': {
        console.printf("\n--- EEPROM Hub IP ---\n");
        char saved[16];
        if (load_hub_ip_from_eeprom(saved, sizeof(saved))) {
            console.printf("  Salvo: %s\n", saved);
        } else {
            console.printf("  Nenhum IP salvo\n");
        }
        console.printf("---------------------\n\n");
        break;
    }
    case 'u':
    case 'U':
        console.printf("\n--- OTA ---\n");
        console.printf("  Hostname: %s.local\n", s_device_id);
        console.printf("  Port:     8266 (ArduinoOTA)\n");
        console.printf("  PlatformIO CLI:\n");
        console.printf("    pio run -t upload --upload-port %s.local\n", s_device_id);
        console.printf("-------------\n\n");
        break;
    case 'h':
    case 'H':
    case '?':
        console.printf("\n--- Comandos ---\n");
        console.printf("  s    - status do dispositivo\n");
        console.printf("  l    - ler sensor agora\n");
        console.printf("  d    - ver hub IP salvo na EEPROM\n");
        console.printf("  r    - reset\n");
        console.printf("  u    - info OTA\n");
        console.printf("  h/?  - esta ajuda\n");
        console.printf("  Browser: http://%s\n", WiFi.localIP().toString().c_str());
        console.printf("  Hub: %s:%d\n", s_hub_ip, s_hub_port);
        console.printf("  RSSI:     %d dBm\n", WiFi.RSSI());
        console.printf("  Up:       %lu s\n", (millis() - s_start_time) / 1000);
        console.printf("-------------\n\n");
        break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    console.begin();
    console.set_banner(CONSOLE_BANNER);

    s_start_time = millis();

    uint32_t id = chip_id();
    snprintf(s_device_id, sizeof(s_device_id), "agri_%06x", id);

#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);
#endif

    randomSeed(analogRead(A0));

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  %s %s\n", CONSOLE_BANNER, FW_VERSION);
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
    console.printf("============================================\n");

    mywifi_begin(false);

    s_udp.begin(TCP_UDP_PORT);
    console.printf("[tcp] UDP listening on port %d\n", TCP_UDP_PORT);

    char saved_ip[16];
    if (load_hub_ip_from_eeprom(saved_ip, sizeof(saved_ip))) {
        strncpy(s_hub_ip, saved_ip, sizeof(s_hub_ip) - 1);
        s_hub_ip[sizeof(s_hub_ip) - 1] = '\0';
        s_hub_ip_configured = true;
        console.printf("[tcp] Hub IP from EEPROM: %s\n", s_hub_ip);
    } else {
        console.println("[tcp] No hub IP in EEPROM, will use UDP discovery");
    }

    read_sensor();

    s_server.on("/", handle_root);
    s_server.on("/docs", handle_docs);
    s_server.on("/wifi", HTTP_GET, handle_wifi_page);
    s_server.on("/api/state", HTTP_GET, handle_api_state);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/api/ota", HTTP_POST, handle_api_ota);
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.begin();
    console.println("[tcp] Web server started");

    ota_setup(s_device_id);

    console.printf("\n  => Browser: http://%s\n", WiFi.localIP().toString().c_str());
    console.printf("  => Terminal: 'h' comando de ajuda\n");
    console.printf("============================================\n\n");
}

void loop() {
    console.loop();
    if (Serial.available() > 0)
        handle_serial(Serial.read());
    int tc = console.telnet_read();
    if (tc >= 0)
        handle_serial((char)tc);

    mywifi_loop();
    ota_handle();
    s_server.handleClient();

    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - s_last_led_toggle >= LED_BLINK_WIFI_MS) {
            s_last_led_toggle = now;
#ifdef LED_PIN
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
#endif
        }
        handle_udp_announce();
        if (s_hub_found && !s_hub_ip_configured) {
            save_hub_ip_to_eeprom(s_hub_ip);
            s_hub_ip_configured = true;
        }
        return;
    }

#ifdef LED_PIN
    digitalWrite(LED_PIN, LED_OFF);
#endif

    unsigned long now = millis();

    if (!s_hub_found) {
        if (s_hub_ip_configured) {
            if (now - s_last_reconnect > RECONNECT_INTERVAL) {
                if (register_with_hub()) {
                    s_hub_found = true;
                    console.printf("[tcp] Connected to configured hub: %s\n", s_hub_ip);
                } else {
                    s_http_fallback_retries++;
                    if (s_http_fallback_retries > HUB_FALLBACK_RETRIES) {
                        console.println("[tcp] Configured hub not responding, starting UDP discovery...");
                        s_hub_ip_configured = false;
                        s_http_fallback_retries = 0;
                        s_discovery_retries = 0;
                    }
                }
                s_last_reconnect = now;
            }
            return;
        }

        if (now - s_last_discovery > DISCOVERY_INTERVAL) {
            send_udp_discover();
            s_last_discovery = now;
            s_discovery_retries++;

            if (s_discovery_retries > MAX_DISCOVERY_RETRIES) {
                strncpy(s_hub_ip, HUB_IP_DEFAULT, sizeof(s_hub_ip) - 1);
                s_hub_found = true;
                console.printf("[tcp] Using default hub IP: %s\n", s_hub_ip);
            }
        }

        handle_udp_announce();
        if (s_hub_found && !s_hub_ip_configured) {
            save_hub_ip_to_eeprom(s_hub_ip);
            s_hub_ip_configured = true;
        }
        return;
    }

    if (!s_registered) {
        if (now - s_last_reconnect > RECONNECT_INTERVAL) {
            if (!register_with_hub()) {
                s_http_fallback_retries++;
                if (s_http_fallback_retries > HUB_FALLBACK_RETRIES) {
                    console.println("[tcp] Hub offline, restarting UDP discovery...");
                    s_hub_found = false;
                    s_registered = false;
                    s_http_fallback_retries = 0;
                    s_discovery_retries = 0;
                }
            }
            s_last_reconnect = now;
        }
        return;
    }

    if (now - s_last_state_send > STATE_UPDATE_INTERVAL) {
        send_state();
    }

    if (now - s_last_heartbeat > HEARTBEAT_INTERVAL) {
        send_heartbeat();
    }

    check_commands();

#ifdef LED_PIN
    static bool led_state = false;
    unsigned long blink_ms = s_registered ? 1000 : LED_BLINK_GATEWAY_MS;
    if (now - s_last_led_toggle >= blink_ms) {
        s_last_led_toggle = now;
        led_state = !led_state;
        digitalWrite(LED_PIN, led_state ? LED_ON : LED_OFF);
    }
#endif
}
