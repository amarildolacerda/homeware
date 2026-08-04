#include <Arduino.h>
#include "platform.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"
#include "radio_node_strategy.h"
#include "common_console.h"
#include "common_espnow.h"
#include "common_wifi.h"
#include "common_web.h"

static const char *TAG = "agri-rain";

static unsigned long s_last_reconnect_attempt = 0;

static int s_rain_level = 0;
static int s_rain_digital = HIGH;
static int s_battery = 100;
static unsigned long s_start_time = 0;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static bool s_wifi_configuration_mode = false;
static unsigned long s_wifi_config_start_time = 0;

static NodeRadioType s_radio;

static ESP8266WebServer s_server(80);

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_MAGIC 0xAA

static uint8_t get_sensor_type() {
    return SENSOR_TYPE_RAIN;
}

static uint8_t get_sensor_payload(uint8_t* buf, uint8_t max_len) {
    payload_rain_t pl;
    memset(&pl, 0, sizeof(pl));
    pl.rain_level = s_rain_level;
    pl.rain_digital = s_rain_digital;
    uint8_t len = sizeof(pl);
    if (len > max_len) len = max_len;
    memcpy(buf, &pl, len);
    if (len + 4 <= max_len) {
        IPAddress ip = WiFi.localIP();
        buf[len]     = ip[0];
        buf[len + 1] = ip[1];
        buf[len + 2] = ip[2];
        buf[len + 3] = ip[3];
        len += 4;
    }
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

static void read_sensor(void)
{
    int raw = analogRead(RAIN_ANALOG_PIN);
    s_rain_level = map(raw, 0, 1024, 100, 0);
    s_rain_level = constrain(s_rain_level, 0, 100);
    s_rain_digital = digitalRead(RAIN_DIGITAL_PIN);

    static int counter = 0;
    counter++;
    if (counter > 100)
    {
        counter = 0;
        s_battery = max(0, s_battery - 1);
    }
}

static void init_hardware(void)
{
    pinMode(RAIN_ANALOG_PIN, INPUT);
    pinMode(RAIN_DIGITAL_PIN, INPUT);
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

enum WifiState { WIFI_IDLE, WIFI_RECONNECTING };
static WifiState s_wifi_state = WIFI_IDLE;
static unsigned long s_wifi_reconnect_deadline = 0;
static unsigned long s_last_config_attempt = 0;

static void maintain_wifi_connection(void)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        s_wifi_state = WIFI_IDLE;
        return;
    }

    unsigned long now = millis();

    if (s_wifi_state == WIFI_RECONNECTING)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            console.printf("[%s] Reconnected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            s_wifi_state = WIFI_IDLE;
            return;
        }
        if (now >= s_wifi_reconnect_deadline)
        {
            console.printf("[%s] WiFi reconnect timeout\n", TAG);
            s_wifi_state = WIFI_IDLE;
            if (now - s_last_config_attempt > 300000)
            {
                s_last_config_attempt = now;
                wifi_setup(true);
            }
        }
        return;
    }

    if (now - s_last_reconnect_attempt < 30000)
        return;
    s_last_reconnect_attempt = now;

    console.printf("[%s] WiFi disconnected. Reconnecting...\n", TAG);
    WiFi.begin();
    s_wifi_reconnect_deadline = millis() + 15000;
    s_wifi_state = WIFI_RECONNECTING;
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
        doc["rain_level"] = s_rain_level;
        doc["rain_digital"] = s_rain_digital == LOW;
        doc["battery"] = s_battery;
        doc["device_id"] = s_device_id;
        doc["device_name"] = s_device_name;
        doc["fw_version"] = FW_VERSION;
        doc["platform"] = "esp8266";
        doc["gateway_connected"] = s_radio.is_paired();
        doc["paired"] = s_radio.is_paired();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        doc["slot"] = s_radio.assigned_slot();
        doc["tx_count"] = s_radio.tx_count();
        doc["rx_count"] = s_radio.rx_count();
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
        read_sensor();
        console.printf("  Chuva:    %d %%\n", s_rain_level);
        console.printf("  Digital:  %s\n", s_rain_digital == LOW ? "chuva" : "seco");
        console.printf("  Bateria:  %d %%\n", s_battery);
        if (s_radio.is_paired())
        {
            s_radio.publish_state();
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
        s_radio.force_repair();
        console.printf("  Enviando requisicao de par...\n");
        console.printf("----------------\n\n");
        break;
    }
    case 'h':
    case 'H':
    case '?':
        console.printf("\n--- Comandos ---\n");
        console.printf("  l    - ler sensor agora\n");
        console.printf("  r    - reset\n");
        console.printf("  s    - status do dispositivo\n");
        console.printf("  p    - resetar par e tentar parear\n");
        console.printf("  u    - info OTA\n");
        console.printf("  h/?  - esta ajuda\n");
        console.printf("  Browser: http://%s\n", WiFi.localIP().toString().c_str());
        if (s_radio.is_paired())
        {
            char mac_str[18];
            mac_to_str(s_radio.gateway_mac(), mac_str, sizeof(mac_str));
            console.printf("  Gateway: %s (slot %d)\n", mac_str, s_radio.assigned_slot());
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
        console.printf("  Chuva:       %d %%\n", s_rain_level);
        console.printf("  Digital:     %s\n", s_rain_digital == LOW ? "chuva" : "seco");
        console.printf("  Bateria:     %d %%\n", s_battery);
        if (s_radio.is_paired())
        {
            char mac_str[18];
            mac_to_str(s_radio.gateway_mac(), mac_str, sizeof(mac_str));
            console.printf("  Gateway:     %s (slot %d)\n", mac_str, s_radio.assigned_slot());
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

static void on_pairing_failed() {
    console.printf("[%s] Pairing failed on ch %d — trying next AP...\n", TAG, WiFi.channel());
    if (mywifi_try_next_bssid()) {
        console.printf("[%s] Reconnecting, will retry pairing\n", TAG);
    } else {
        console.printf("[%s] No other APs found, will retry on current\n", TAG);
    }
}

void setup(void)
{
    Serial.begin(115200);
    delay(1000);
    console.begin();
    s_start_time = millis();

    uint32_t id = chip_id();
    snprintf(s_device_id, sizeof(s_device_id), "agri_%06x", id);

    espnow_load_device_name(s_device_name, sizeof(s_device_name));

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  AgriSense Rain " FW_VERSION "\n");
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
    s_radio.set_mac(my_mac);
    s_radio.set_device_name(s_device_name);
    s_radio.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr, on_pairing_failed };
    s_radio.load_gateway_mac();
    s_radio.begin();

    s_server.on("/", handle_root);
    s_server.on("/docs", []() { serve_pgm_page(s_server, PAGE_DOCS); });
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/pin", HTTP_ANY, []() { handle_api_pin(s_server); });
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
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
        yield();
        return;
    }

    unsigned long now = millis();

    static unsigned long s_last_sensor_read = 0;
    if (now - s_last_sensor_read > STATE_UPDATE_INTERVAL)
    {
        s_last_sensor_read = now;
        read_sensor();
    }

    s_radio.loop();

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
    else if (!s_radio.is_paired())
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

    yield();
}
