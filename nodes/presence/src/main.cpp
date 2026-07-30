#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"
#include "espnow_node_protocol.h"
#include "common_console.h"
#include "common_espnow.h"
#include "common_web.h"

static const char *TAG = "agri-presence";

static unsigned long s_last_reconnect_attempt = 0;

static bool s_motion_state = false;
static unsigned long s_last_motion_time = 0;
static int s_battery = 100;
static unsigned long s_start_time = 0;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static bool s_wifi_configuration_mode = false;
static unsigned long s_wifi_config_start_time = 0;

static EspnowNodeProtocol s_espnow;

static ESP8266WebServer s_server(80);

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_MAGIC 0xAA

static uint8_t get_sensor_type() {
    return SENSOR_TYPE_MOTION;
}

static uint8_t get_sensor_payload(uint8_t* buf, uint8_t max_len) {
    payload_motion_t pl;
    memset(&pl, 0, sizeof(pl));
    pl.motion_state = s_motion_state;
    pl.occupancy_duration = 0;
    uint8_t len = sizeof(pl);
    if (len > max_len) len = max_len;
    memcpy(buf, &pl, len);
    return len;
}

static void on_command(uint8_t command) {
    console.printf("[%s] Command received: %d\n", TAG, command);
}

static void on_paired(uint8_t slot) {
    console.printf("[%s] Paired, slot %d\n", TAG, slot);
}

static void on_restart() {
    console.printf("[%s] Restart command received\n", TAG);
    delay(100);
    ESP.restart();
}

static void read_pir(void)
{
    int raw = digitalRead(PIR_PIN);
    if (raw == HIGH)
    {
        s_last_motion_time = millis();
        if (!s_motion_state)
        {
            s_motion_state = true;
            s_espnow.publish_state();
        }
    }
    else
    {
        if (s_motion_state && (millis() - s_last_motion_time >= PIR_HOLD_TIME_MS))
        {
            s_motion_state = false;
            s_espnow.publish_state();
        }
    }
}

static void init_hardware(void)
{
    pinMode(PIR_PIN, INPUT);
#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif
}

static bool wifi_setup(bool force_config_portal = false)
{
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20);

    if (!force_config_portal && WiFi.SSID() != "")
    {
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(3);
        console.printf("[%s] Connecting to saved WiFi: %s\n", TAG, WiFi.SSID().c_str());
        if (wifiManager.autoConnect())
        {
            console.printf("[%s] WiFi connected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            s_wifi_configuration_mode = false;
            return true;
        }
        console.printf("[%s] Failed to connect to saved WiFi\n", TAG);
    }

    console.printf("[%s] Starting configuration portal...\n", TAG);
    s_wifi_configuration_mode = true;
    s_wifi_config_start_time = millis();
    wifiManager.setConfigPortalTimeout(300);

    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 32);
    wifiManager.addParameter(&custom_dev_name);

    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0)
        {
            strncpy(s_device_name, custom_dev_name.getValue(), sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
            espnow_save_device_name(s_device_name);
        }
        s_wifi_configuration_mode = false;
        return true;
    }

    console.printf("[%s] Configuration portal timed out\n", TAG);
    s_wifi_configuration_mode = false;
    return false;
}

static void maintain_wifi_connection(void)
{
    if (WiFi.status() == WL_CONNECTED)
        return;
    unsigned long now = millis();
    if (now - s_last_reconnect_attempt < 30000)
        return;
    s_last_reconnect_attempt = now;

    console.printf("[%s] WiFi disconnected. Reconnecting...\n", TAG);
    WiFi.begin();

    static unsigned long last_config_attempt = 0;
    if (now - last_config_attempt > 300000)
    {
        last_config_attempt = now;
        wifi_setup(true);
    }
}

static void check_config_portal_timeout(void)
{
    if (s_wifi_configuration_mode && (millis() - s_wifi_config_start_time > 600000))
    {
        console.printf("[%s] Config portal timeout. Restarting...\n", TAG);
        ESP.restart();
    }
}

static void handle_api_settings(void)
{
    if (s_server.method() == HTTP_GET)
    {
        DynamicJsonDocument doc(256);
        doc["device_name"] = s_device_name;
        JsonArray pins = doc["available_pins"].to<JsonArray>();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    }
    else if (s_server.method() == HTTP_POST)
    {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, s_server.arg("plain"));
        if (doc.containsKey("device_name"))
        {
            String name = doc["device_name"].as<String>();
            if (name.length() > 0)
            {
                espnow_save_device_name(name.c_str());
                strncpy(s_device_name, name.c_str(), sizeof(s_device_name) - 1);
                s_device_name[sizeof(s_device_name) - 1] = '\0';
                s_server.send(200, "application/json", "{\"ok\":true}");
                return;
            }
        }
        s_server.send(400, "application/json", "{\"error\":\"invalid\"}");
    }
}

static void handle_root(void)
{
    serve_pgm_page(s_server, PAGE_DASHBOARD);
}

static void handle_api_state(void)
{
    String json;
    {
        JsonDocument doc;
        doc["motion_state"] = s_motion_state;
        doc["battery"] = s_battery;
        doc["device_id"] = s_device_id;
        doc["device_name"] = s_device_name;
        doc["fw_version"] = FW_VERSION;
        doc["platform"] = "esp8266";
        doc["type"] = "pir";
        doc["gateway_connected"] = s_espnow.is_paired();
        doc["paired"] = s_espnow.is_paired();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        doc["slot"] = s_espnow.assigned_slot();
        doc["tx_count"] = s_espnow.tx_count();
        doc["rx_count"] = s_espnow.rx_count();
        doc["free_heap"] = ESP.getFreeHeap();
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_serial(char c)
{
    switch (c)
    {
    case 'R':
    case 'r':
        ESP.restart();
        break;
    case 'l':
    case 'L':
    {
        console.printf("\n--- Leitura forcada ---\n");
        read_pir();
        console.printf("  Motion:   %s\n", s_motion_state ? "SIM" : "NAO");
        console.printf("  Bateria:  %d %%\n", s_battery);
        if (s_espnow.is_paired())
        {
            s_espnow.publish_state();
        }
        else
        {
            console.printf("  (gateway nao pareado)\n");
        }
        console.printf("-------------------------\n\n");
        break;
    }
    case 'u':
    case 'U':
        console.printf("\n--- OTA ---\n");
        console.printf("  Hostname: %s.local\n", s_device_id);
        console.printf("  Port:     8266 (ArduinoOTA)\n");
        console.printf("  PlatformIO CLI:\n");
        console.printf("    pio run -t upload --upload-port %s.local\n", s_device_id);
        console.printf("  espota.py:\n");
        console.printf("    espota.py -i %s.local -p 8266 -f firmware.bin\n", s_device_id);
        console.printf("-------------\n\n");
        break;
    case 'p':
    case 'P':
    {
        console.printf("\n--- Par ---\n");
        s_espnow.force_repair();
        console.printf("  Enviando requisicao de par...\n");
        console.printf("----------------\n\n");
        break;
    }
    case 'h':
    case 'H':
    case '?':
        console.printf("\n--- Comandos ---\n");
        console.printf("  l    - ler PIR agora\n");
        console.printf("  r    - reset\n");
        console.printf("  s    - status do dispositivo\n");
        console.printf("  p    - resetar par e tentar parear\n");
        console.printf("  u    - info OTA\n");
        console.printf("  h/?  - esta ajuda\n");
        console.printf("  Browser: http://%s\n", WiFi.localIP().toString().c_str());
        if (s_espnow.is_paired())
        {
            char mac_str[18];
            mac_to_str(s_espnow.gateway_mac(), mac_str, sizeof(mac_str));
            console.printf("  Gateway: %s (slot %d)\n", mac_str, s_espnow.assigned_slot());
        }
        console.printf("  IP local: %s\n", WiFi.localIP().toString().c_str());
        console.printf("  RSSI:     %d dBm\n", WiFi.RSSI());
        console.printf("  Up:       %lu s\n", (millis() - s_start_time) / 1000);
        console.printf("----------------\n\n");
        break;
    case 's':
    case 'S':
    {
        unsigned long up = (millis() - s_start_time) / 1000;
        console.printf("\n--- Status ---\n");
        console.printf("  Dispositivo: %s\n", s_device_id);
        console.printf("  Nome:        %s\n", s_device_name);
        console.printf("  Motion:      %s\n", s_motion_state ? "SIM" : "NAO");
        console.printf("  Bateria:     %d %%\n", s_battery);
        if (s_espnow.is_paired())
        {
            char mac_str[18];
            mac_to_str(s_espnow.gateway_mac(), mac_str, sizeof(mac_str));
            console.printf("  Gateway:     %s (slot %d)\n", mac_str, s_espnow.assigned_slot());
        }
        else
        {
            console.printf("  Gateway:     nao pareado\n");
        }
        console.printf("  Browser:     http://%s\n", WiFi.localIP().toString().c_str());
        console.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
        console.printf("  Uptime:      %lu s\n", up);
        console.printf("---------------\n\n");
        break;
    }
    }
}

static void handle_ota(void)
{
    if (!Update.hasError())
    {
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    }
    else
    {
        s_server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

static void handle_ota_upload(void)
{
    HTTPUpload &upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        console.printf("[%s] OTA update started: %s (%d bytes)\n", TAG, upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(upload.totalSize))
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
            console.printf("[%s] OTA update success: %d bytes\n", TAG, upload.totalSize);
        else
            Update.printError(Serial);
    }
}

void setup(void)
{
    Serial.begin(115200);
    delay(1000);
    console.begin();
    s_start_time = millis();

    uint32_t chip_id = ESP.getChipId();
    snprintf(s_device_id, sizeof(s_device_id), "agri_%06x", chip_id);

    espnow_load_device_name(s_device_name, sizeof(s_device_name));

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  AgriSense Presence " FW_VERSION "\n");
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();
    console.printf("============================================\n");

    if (!wifi_setup(false))
    {
        console.printf("[%s] WiFi setup failed, restarting...\n", TAG);
        delay(5000);
        ESP.restart();
    }

    uint8_t my_mac[6];
    WiFi.macAddress(my_mac);
    s_espnow.set_mac(my_mac);
    s_espnow.set_device_name(s_device_name);
    s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr };
    s_espnow.load_gateway_mac();
    s_espnow.begin();

    s_server.on("/", handle_root);
    s_server.on("/docs", []() { serve_pgm_page(s_server, PAGE_DOCS); });
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/pin", HTTP_ANY, []() { handle_api_pin(s_server); });
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.on("/api/restart", HTTP_POST, []() {
        s_server.send(200, "application/json", "{\"status\":\"restarting\"}");
        delay(200);
        ESP.restart();
    });
    s_server.begin();

    ArduinoOTA.setHostname(s_device_id);
    ArduinoOTA.onStart([]() { console.printf("[%s] OTA update start\n", TAG); });
    ArduinoOTA.onEnd([]() { console.printf("[%s] OTA update end\n", TAG); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        console.printf("[%s] OTA progress: %u%%\r", TAG, (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        console.printf("[%s] OTA error: %d\n", TAG, error);
    });
    ArduinoOTA.begin();
    console.printf("[%s] OTA ready: %s.local\n", TAG, s_device_id);

    console.printf("\n  => Browser: http://%s\n", WiFi.localIP().toString().c_str());
    console.printf("  => Terminal: 'h' comando de ajuda\n");

    console.printf("============================================\n");
    console.printf("  Pronto! Pressione 'h' para ajuda\n");
    console.printf("============================================\n\n");
}

void loop(void)
{
    console.loop();
    if (Serial.available() > 0)
        handle_serial(Serial.read());
    int tc = console.telnet_read();
    if (tc >= 0)
        handle_serial((char)tc);
    check_config_portal_timeout();
    ArduinoOTA.handle();
    s_server.handleClient();

    if (WiFi.status() != WL_CONNECTED)
    {
        maintain_wifi_connection();
        return;
    }

    unsigned long now = millis();

    read_pir();

    s_espnow.loop();

#ifdef LED_PIN
    static unsigned long last_led = 0;
    if (s_wifi_configuration_mode)
    {
        digitalWrite(LED_PIN, HIGH);
    }
    else if (WiFi.status() != WL_CONNECTED)
    {
        if (now - last_led >= LED_BLINK_WIFI_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else if (!s_espnow.is_paired())
    {
        if (now - last_led >= LED_BLINK_GATEWAY_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
    }
#endif
}
