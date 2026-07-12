#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <espnow.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"
#include "console.h"

static const char *TAG = "esp8266-chuva";

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_reconnect_attempt = 0;
static unsigned long s_last_espnow_send = 0;
static unsigned long s_last_espnow_pair = 0;
static unsigned long s_last_heartbeat = 0;

static bool s_gateway_connected = false;
static bool s_paired = false;
static uint8_t s_gateway_mac[6];
static uint16_t s_sequence = 0;
static uint16_t s_assigned_slot = 0;
static int s_pair_attempts = 0;
static bool s_ack_received = false;
static bool s_send_pending = false;
static bool s_espnow_ready = false;

static int s_rain_level = 0;
static int s_rain_digital = HIGH;
static int s_battery = 100;
static unsigned long s_start_time = 0;
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;
static unsigned long s_last_send_ms = 0;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static bool s_wifi_configuration_mode = false;
static unsigned long s_wifi_config_start_time = 0;

static ESP8266WebServer s_server(80);

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_MAGIC 0xAA

static void save_gateway_mac(const uint8_t *mac)
{
    EEPROM.begin(128);
    EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, EEPROM_MAGIC);
    for (int i = 0; i < 6; i++)
        EEPROM.write(EEPROM_GATEWAY_MAC_ADDR + 1 + i, mac[i]);
    EEPROM.commit();
    EEPROM.end();
}

static bool load_gateway_mac(void)
{
    EEPROM.begin(128);
    uint8_t marker = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR);
    if (marker == EEPROM_MAGIC)
    {
        for (int i = 0; i < 6; i++)
            s_gateway_mac[i] = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR + 1 + i);
        EEPROM.end();
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Loaded gateway MAC: %s\n", TAG, mac_str);
        return true;
    }
    EEPROM.end();
    return false;
}

static void save_device_name(const char *name)
{
    EEPROM.begin(128);
    EEPROM.write(EEPROM_NAME_ADDR, 0xFF);
    for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
    {
        EEPROM.write(EEPROM_NAME_ADDR + 1 + i, name[i]);
        if (name[i] == '\0') break;
    }
    EEPROM.write(EEPROM_NAME_ADDR + EEPROM_NAME_MAX, '\0');
    EEPROM.commit();
    EEPROM.end();
}

static bool is_valid_name(const char *s)
{
    if (!s || s[0] == '\0') return false;
    for (int i = 0; s[i]; i++)
    {
        char c = s[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}

static void load_device_name(void)
{
    EEPROM.begin(128);
    uint8_t marker = EEPROM.read(EEPROM_NAME_ADDR);
    if (marker == 0xFF)
    {
        char buf[EEPROM_NAME_MAX];
        for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
        {
            buf[i] = EEPROM.read(EEPROM_NAME_ADDR + 1 + i);
            if (buf[i] == '\0') break;
        }
        buf[EEPROM_NAME_MAX - 1] = '\0';
        if (is_valid_name(buf))
        {
            strncpy(s_device_name, buf, sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
        }
    }
    EEPROM.end();
}

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    (void)status;
    s_espnow_tx_count++;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    s_espnow_rx_count++;
    if (!data || len < 1) return;

    switch (data[0])
    {
        case ESPNOW_MSG_PAIR_RESPONSE:
        {
            if (len < sizeof(espnow_pair_response_t)) return;
            espnow_pair_response_t *resp = (espnow_pair_response_t *)data;
            if (resp->status == PAIR_STATUS_OK)
            {
                mac_copy(s_gateway_mac, mac);
                s_assigned_slot = resp->assigned_slot;
                save_gateway_mac(mac);
                s_paired = true;
                s_gateway_connected = true;
                char mac_str[18];
                mac_to_str(mac, mac_str, sizeof(mac_str));
                console.printf("[%s] Paired with gateway %s slot %d\n", TAG, mac_str, s_assigned_slot);
            }
            else
            {
                console.printf("[%s] Pair response: status=%d\n", TAG, resp->status);
            }
            break;
        }
        case ESPNOW_MSG_ACK:
        {
            if (len < sizeof(espnow_ack_t)) return;
            espnow_ack_t *ack = (espnow_ack_t *)data;
            if (ack->status == PAIR_STATUS_DENIED)
            {
                s_paired = false;
                s_gateway_connected = false;
                console.printf("[%s] Gateway rejected data (denied), need re-pair\n", TAG);
            }
            else
            {
                s_gateway_connected = true;
            }
            s_ack_received = true;
            break;
        }
    }
}

static bool espnow_init_client(void)
{
    if (esp_now_init() != 0)
    {
        console.printf("[%s] ESP-NOW init failed\n", TAG);
        return false;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);
    s_espnow_ready = true;
    console.printf("[%s] ESP-NOW initialized\n", TAG);
    return true;
}

static bool espnow_add_peer(const uint8_t *mac)
{
    if (!s_espnow_ready) return false;
    esp_now_del_peer((uint8_t *)mac);
    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    int ret = esp_now_add_peer((uint8_t *)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    if (ret != 0)
    {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Failed to add peer %s: %d\n", TAG, mac_str, ret);
    }
    return (ret == 0);
}

#define ESPNOW_HEADER_FIXED_SIZE (sizeof(espnow_header_t) - sizeof(((espnow_header_t*)0)->payload))

static bool espnow_send_data(void)
{
    if (!s_paired || !s_espnow_ready) return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_rain_t) + 4];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_RAIN;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();

    payload_rain_t *pl = (payload_rain_t *)hdr->payload;
    pl->rain_level = (uint8_t)s_rain_level;
    pl->rain_digital = (uint8_t)s_rain_digital;

    IPAddress ip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_rain_t);
    ip_ptr[0] = ip[0];
    ip_ptr[1] = ip[1];
    ip_ptr[2] = ip[2];
    ip_ptr[3] = ip[3];
    hdr->payload_len = sizeof(payload_rain_t) + 4;

    if (!espnow_add_peer(s_gateway_mac))
    {
        console.printf("[%s] Failed to add gateway peer\n", TAG);
        return false;
    }

    s_ack_received = false;
    s_send_pending = true;
    int ret = esp_now_send(s_gateway_mac, buf, sizeof(buf));
    if (ret != 0)
    {
        console.printf("[%s] ESP-NOW send failed: %d\n", TAG, ret);
        s_send_pending = false;
        return false;
    }
    return true;
}

static bool espnow_send_heartbeat(void)
{
    if (!s_paired || !s_espnow_ready) return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_HEARTBEAT;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_RAIN;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();
    hdr->payload_len = 0;

    if (!espnow_add_peer(s_gateway_mac)) return false;

    s_ack_received = false;
    return esp_now_send(s_gateway_mac, buf, sizeof(buf)) == 0;
}

static bool espnow_send_pair_request(void)
{
    if (!s_espnow_ready) return false;

    uint8_t buf[sizeof(espnow_pair_request_t)];
    memset(buf, 0, sizeof(buf));

    espnow_pair_request_t *req = (espnow_pair_request_t *)buf;
    req->msg_type = ESPNOW_MSG_PAIR_REQUEST;
    req->sequence = s_sequence++;
    WiFi.macAddress(req->sensor_mac);
    req->sensor_type = SENSOR_TYPE_RAIN;
    uint32_t ver = 0x000A000B;
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';

    if (!espnow_add_peer(s_broadcast_mac)) return false;

    s_ack_received = false;
    int ret = esp_now_send(s_broadcast_mac, buf, sizeof(buf));
    if (ret != 0)
    {
        console.printf("[%s] Pair request send failed: %d\n", TAG, ret);
        return false;
    }
    return true;
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
            save_device_name(s_device_name);
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
    unsigned long connect_start = millis();
    while (millis() - connect_start < 15000)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            console.printf("[%s] Reconnected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            return;
        }
        delay(500);
    }

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
                save_device_name(name.c_str());
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
    s_server.send(200, "text/html", FPSTR(PAGE_DASHBOARD));
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
        doc["gateway_connected"] = s_gateway_connected;
        doc["paired"] = s_paired;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        if (s_last_send_ms) doc["last_send_s"] = (millis() - s_last_send_ms) / 1000;
        doc["slot"] = s_assigned_slot;
        doc["tx_count"] = s_espnow_tx_count;
        doc["rx_count"] = s_espnow_rx_count;
        doc["free_heap"] = ESP.getFreeHeap();
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_api_pin(void)
{
    if (s_server.method() == HTTP_GET)
    {
        int pin = s_server.arg("gpio").toInt();
        pinMode(pin, INPUT_PULLUP);
        int state = digitalRead(pin);
        String json;
        JsonDocument doc;
        doc["gpio"] = pin;
        doc["state"] = state;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    }
    else if (s_server.method() == HTTP_POST)
    {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err)
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        int pin = doc["gpio"] | -1;
        if (pin < 0 || pin > 16)
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid gpio\"}");
            return;
        }
        int state = doc["state"] | -1;
        if (state != 0 && state != 1)
        {
            s_server.send(400, "application/json", "{\"error\":\"state must be 0 or 1\"}");
            return;
        }
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state);
        String json;
        JsonDocument resp;
        resp["gpio"] = pin;
        resp["state"] = state;
        resp["status"] = "ok";
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
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
        if (s_paired)
        {
            s_last_espnow_send = 0;
            if (espnow_send_data())
                s_last_send_ms = millis();
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
        s_paired = false;
        s_gateway_connected = false;
        s_pair_attempts = 0;
        console.printf("  Estado de pareamento resetado\n");
        console.printf("  Enviando requisicao de par...\n");
        if (espnow_send_pair_request())
            console.printf("  Requisicao enviada!\n");
        else
            console.printf("  Falha ao enviar requisicao\n");
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
        if (s_paired)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            console.printf("  Gateway: %s (slot %d)\n", mac_str, s_assigned_slot);
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
        if (s_paired)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            console.printf("  Gateway:     %s (slot %d) %s\n", mac_str, s_assigned_slot,
                          s_gateway_connected ? "conectado" : "desconectado");
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
    snprintf(s_device_id, sizeof(s_device_id), "esp8266_%06x", chip_id);

    load_device_name();

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  ESP8266 Rain Sensor " FW_VERSION "\n");
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

    espnow_init_client();

    s_server.on("/", handle_root);
    s_server.on("/docs", []() { s_server.send(200, "text/html", FPSTR(PAGE_DOCS)); });
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/pin", handle_api_pin);
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

    if (load_gateway_mac())
    {
        console.printf("[%s] Gateway MAC loaded from EEPROM\n", TAG);
        s_paired = true;
    }
    else
    {
        console.printf("[%s] No saved gateway MAC, will pair\n", TAG);
    }

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
        delay(1000);
        return;
    }

    unsigned long now = millis();

    static unsigned long s_last_sensor_read = 0;
    if (now - s_last_sensor_read > STATE_UPDATE_INTERVAL)
    {
        s_last_sensor_read = now;
        read_sensor();
    }

    if (!s_paired)
    {
        if (now - s_last_espnow_pair > ESPNOW_PAIR_INTERVAL_MS)
        {
            s_last_espnow_pair = now;
            s_pair_attempts++;
            console.printf("[%s] Pair attempt %d/%d\n", TAG, s_pair_attempts, ESPNOW_MAX_PAIR_ATTEMPTS);
            espnow_send_pair_request();
            if (s_pair_attempts >= ESPNOW_MAX_PAIR_ATTEMPTS)
            {
                s_pair_attempts = 0;
                console.printf("[%s] Max pair attempts, waiting before retry\n", TAG);
                delay(60000);
            }
        }
        delay(1);
        return;
    }

    if (now - s_last_espnow_send > STATE_UPDATE_INTERVAL)
    {
        s_last_espnow_send = now;
        s_ack_received = false;
        if (espnow_send_data())
        {
            unsigned long deadline = millis() + ESPNOW_ACK_TIMEOUT_MS;
            while (millis() < deadline)
            {
                if (s_ack_received) break;
                delay(1);
            }
            if (s_ack_received)
            {
                s_gateway_connected = true;
                s_last_send_ms = millis();
            }
            else
            {
                s_gateway_connected = false;
                bool ok = false;
                for (int retry = 0; retry < ESPNOW_SEND_RETRIES; retry++)
                {
                    delay(50);
                    s_ack_received = false;
                    if (espnow_send_data())
                    {
                        deadline = millis() + ESPNOW_ACK_TIMEOUT_MS;
                        while (millis() < deadline)
                        {
                            if (s_ack_received) break;
                            delay(1);
                        }
                        if (s_ack_received) { ok = true; break; }
                    }
                }
                if (ok)
                {
                    s_gateway_connected = true;
                    s_last_send_ms = millis();
                }
                else
                {
                    console.printf("[%s] Send failed after %d retries, re-pairing\n", TAG, ESPNOW_SEND_RETRIES);
                    s_paired = false;
                    s_pair_attempts = 0;
                    s_last_espnow_pair = 0;
                }
            }
        }
    }

    if (now - s_last_heartbeat > HEARTBEAT_INTERVAL)
    {
        s_last_heartbeat = now;
        console.printf("[%s] RSSI=%d dBm  up=%lus\n", TAG, WiFi.RSSI(), (millis() - s_start_time) / 1000);
        if (s_paired)
            espnow_send_heartbeat();
    }

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
    else if (!s_paired)
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

    delay(1);
}
