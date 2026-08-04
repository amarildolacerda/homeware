#include <Arduino.h>
#include "platform.h"
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <Updater.h>

#ifdef ALEXA_ENABLED
#include <Espalexa.h>
#endif
#include "config.h"
#include "pages.h"
#include "common_types.h"
#include "sensor_type.h"

#ifdef ESPNOW_ENABLED
#include "espnow_protocol.h"
#endif

#include "common_console.h"
#include "common_ota.h"
#include "common_util.h"
#include "common_wifi.h"
#include "common_espnow.h"
#include "common_web.h"
#include "common_repeater.h"
#include "node_protocol.h"
#include "radio_node_strategy.h"
#include "timer.h"

static const char *TAG = "agri-lamp";

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_reconnect_attempt = 0;
static unsigned long s_last_alexa_activity = 0;

static uint8_t s_gateway_mac[6];

static bool mac_is_nonzero(const uint8_t *mac)
{
    return mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5];
}

// Radio strategy — selected at compile time via radio_strategy.h
static NodeRadioType s_radio;

static bool s_relay_state = false;
static int s_relay_pin = RELAY_PIN;
static int s_button_pin = BUTTON_PIN;
static int s_battery = 100;
static bool s_button_last = HIGH;
static unsigned long s_button_last_ms = 0;
static int s_btn_press_count = 0;
static unsigned long s_btn_press_start = 0;
static unsigned long s_start_time = 0;

static uint32_t s_on_count = 0;

static int s_timezone_offset = -3;
static unsigned long s_synced_epoch = 0;
static unsigned long s_sync_millis = 0;
static bool s_tz_changed = false;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static unsigned long s_wifi_config_start_time = 0;
static bool s_config_portal_active = false;
static bool s_ota_in_progress = false;
static uint32_t s_ota_bytes = 0;
static bool s_led_enabled = true;
static bool s_multihub = false;
static int s_startup_mode = 0; // 0=OFF, 1=ON, 2=LAST
static unsigned long s_wifi_connect_start = 0;
static bool s_wifi_connected = false;

static ESP8266WebServer s_server(DASHBOARD_PORT);
#ifdef ALEXA_ENABLED
static Espalexa s_alexa;
static EspalexaDevice *s_alexa_dev = nullptr;
static bool s_alexa_initialized = false;
#endif

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// D1-MINI é invtido
#define LED_ON LOW   // GPIO2 acende com LOW
#define LED_OFF HIGH // GPIO2 apaga com HIGH

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_RELAY_STATE_ADDR (EEPROM_NAME_ADDR + EEPROM_NAME_MAX + 1)
#define EEPROM_RELAY_PIN_ADDR (EEPROM_RELAY_STATE_ADDR + 1)
#define EEPROM_BUTTON_PIN_ADDR (EEPROM_RELAY_PIN_ADDR + 1)
#define EEPROM_LED_ENABLED_ADDR (EEPROM_BUTTON_PIN_ADDR + 1)
#define EEPROM_STARTUP_MODE_ADDR (EEPROM_LED_ENABLED_ADDR + 1)
#define EEPROM_REPEATER_EN_ADDR (EEPROM_STARTUP_MODE_ADDR + 1)
#define EEPROM_SSID_ADDR 64
#define EEPROM_MULTIHUB_ADDR 200
#define EEPROM_SSID_MAX 32
#define EEPROM_PASS_ADDR (EEPROM_SSID_ADDR + EEPROM_SSID_MAX)
#define EEPROM_PASS_MAX 64
#define EEPROM_WIFI_MARKER 0x5A
#define EEPROM_MAGIC 0xAA

#define SYNC_LITTLEFS_FILE "/sync.json"
#define KNOWN_DEVICES_FILE "/known_devices.json"
#define MAX_KNOWN_DEVICES 16

typedef struct
{
    bool enabled;
    char target_device_id[32];
    char target_device_name[32];
} sync_config_t;

static sync_config_t s_sync_cfg = {false, "", ""};

static void sync_load(void)
{
    if (!LittleFS.exists(SYNC_LITTLEFS_FILE))
        return;
    File f = LittleFS.open(SYNC_LITTLEFS_FILE, "r");
    if (!f)
        return;
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
        return;
    s_sync_cfg.enabled = doc["enabled"] | false;
    const char *tid = doc["target_device_id"] | "";
    strncpy(s_sync_cfg.target_device_id, tid, sizeof(s_sync_cfg.target_device_id) - 1);
    s_sync_cfg.target_device_id[sizeof(s_sync_cfg.target_device_id) - 1] = '\0';
    const char *tn = doc["target_device_name"] | "";
    strncpy(s_sync_cfg.target_device_name, tn, sizeof(s_sync_cfg.target_device_name) - 1);
    s_sync_cfg.target_device_name[sizeof(s_sync_cfg.target_device_name) - 1] = '\0';
}

static void devices_load(JsonDocument &doc)
{
    if (!LittleFS.exists(KNOWN_DEVICES_FILE))
    {
        doc.to<JsonArray>();
        return;
    }
    File f = LittleFS.open(KNOWN_DEVICES_FILE, "r");
    if (!f)
    {
        doc.to<JsonArray>();
        return;
    }
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err)
    {
        doc.to<JsonArray>();
        return;
    }
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() > 0 && arr[0].is<const char *>())
    {
        JsonDocument ndoc;
        JsonArray narr = ndoc.to<JsonArray>();
        for (JsonVariant v : arr)
        {
            JsonObject obj = narr.add<JsonObject>();
            obj["id"] = v.as<const char *>();
            obj["name"] = v.as<const char *>();
        }
        doc.clear();
        for (JsonVariant v : narr)
        {
            JsonObject obj = doc.add<JsonObject>();
            obj["id"] = v["id"];
            obj["name"] = v["name"];
        }
    }
}

static bool device_exists(JsonArray &arr, const char *device_id)
{
    for (JsonVariant v : arr)
    {
        if (v.is<JsonObject>())
        {
            if (strcmp(v["id"] | "", device_id) == 0)
                return true;
        }
        else if (v.is<const char *>())
        {
            if (strcmp(v, device_id) == 0)
                return true;
        }
    }
    return false;
}

static void devices_add(const char *device_id, const char *device_name)
{
    if (!device_id || strlen(device_id) == 0)
        return;
    JsonDocument doc;
    devices_load(doc);
    JsonArray arr = doc.as<JsonArray>();
    if (device_exists(arr, device_id))
        return;
    if (arr.size() >= MAX_KNOWN_DEVICES)
        arr.remove(0);
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = device_id;
    obj["name"] = (device_name && strlen(device_name) > 0) ? device_name : device_id;
    File f = LittleFS.open(KNOWN_DEVICES_FILE, "w");
    if (!f)
        return;
    serializeJson(doc, f);
    f.close();
}

static void sync_save(void)
{
    StaticJsonDocument<128> doc;
    doc["enabled"] = s_sync_cfg.enabled;
    doc["target_device_id"] = s_sync_cfg.target_device_id;
    doc["target_device_name"] = s_sync_cfg.target_device_name;
    File f = LittleFS.open(SYNC_LITTLEFS_FILE, "w");
    if (!f)
        return;
    serializeJson(doc, f);
    f.close();
}

#ifdef ESPNOW_ENABLED
static void repeater_send_adapter(const uint8_t *mac, const uint8_t *data, int len, const char *tag)
{
    espnow_send_wrapper(mac, data, (size_t)len, tag);
}
#endif

static void save_relay_state(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_RELAY_STATE_ADDR, s_relay_state ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
    console.printf("Saved relay state: %s\n", s_relay_state ? "ON" : "OFF");
}

static void load_relay_state(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_RELAY_STATE_ADDR);
    EEPROM.end();
    s_relay_state = (val == 1);
}

static void save_relay_pin(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_RELAY_PIN_ADDR, (uint8_t)s_relay_pin);
    EEPROM.commit();
    EEPROM.end();
}

static void load_relay_pin(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_RELAY_PIN_ADDR);
    EEPROM.end();
    if (val != 0xFF)
    {
        for (int i = 0; i < AVAILABLE_GPIOS_COUNT; i++)
        {
            if (AVAILABLE_GPIOS[i] == (int)val)
            {
                s_relay_pin = (int)val;
                return;
            }
        }
    }
}

static void save_button_pin(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_BUTTON_PIN_ADDR, (uint8_t)s_button_pin);
    EEPROM.commit();
    EEPROM.end();
}

static void load_button_pin(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_BUTTON_PIN_ADDR);
    EEPROM.end();
    if (val != 0xFF)
    {
        for (int i = 0; i < AVAILABLE_GPIOS_COUNT; i++)
        {
            if (AVAILABLE_GPIOS[i] == (int)val)
            {
                s_button_pin = (int)val;
                return;
            }
        }
    }
}

static void save_led_enabled(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_LED_ENABLED_ADDR, s_led_enabled ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}

static void load_led_enabled(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_LED_ENABLED_ADDR);
    EEPROM.end();
    if (val == 0)
        s_led_enabled = false;
    else
        s_led_enabled = true;
}

static void save_startup_mode(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_STARTUP_MODE_ADDR, (uint8_t)s_startup_mode);
    EEPROM.commit();
    EEPROM.end();
}

static void load_startup_mode(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_STARTUP_MODE_ADDR);
    EEPROM.end();
    if (val <= 2)
        s_startup_mode = (int)val;
    else
        s_startup_mode = 0;
}

static void save_wifi_credentials(const char *ssid, const char *pass)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_SSID_ADDR, EEPROM_WIFI_MARKER);
    EEPROM.write(EEPROM_PASS_ADDR, EEPROM_WIFI_MARKER);
    for (int i = 0; i < EEPROM_SSID_MAX - 1; i++)
    {
        EEPROM.write(EEPROM_SSID_ADDR + 1 + i, ssid[i]);
        if (ssid[i] == '\0')
            break;
    }
    EEPROM.write(EEPROM_SSID_ADDR + EEPROM_SSID_MAX - 1, '\0');
    for (int i = 0; i < EEPROM_PASS_MAX - 1; i++)
    {
        EEPROM.write(EEPROM_PASS_ADDR + 1 + i, pass[i]);
        if (pass[i] == '\0')
            break;
    }
    EEPROM.write(EEPROM_PASS_ADDR + EEPROM_PASS_MAX - 1, '\0');
    EEPROM.commit();
    EEPROM.end();
}

static bool eeprom_str_valid_at(uint16_t addr, uint8_t max_len)
{
    for (int i = 0; i < max_len; i++)
    {
        uint8_t c = EEPROM.read(addr + i);
        if (c == '\0')
            return true;
        if (c < 0x20 || c > 0x7E)
            return false;
    }
    return EEPROM.read(addr + max_len) == '\0';
}

static bool load_wifi_credentials(char *ssid, size_t ssid_size, char *pass, size_t pass_size)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t marker = EEPROM.read(EEPROM_SSID_ADDR);
    bool found = false;
    if (marker == EEPROM_WIFI_MARKER && eeprom_str_valid_at(EEPROM_SSID_ADDR + 1, EEPROM_SSID_MAX - 1))
    {
        char buf[64];
        for (int i = 0; i < EEPROM_SSID_MAX - 1; i++)
        {
            buf[i] = EEPROM.read(EEPROM_SSID_ADDR + 1 + i);
            if (buf[i] == '\0')
                break;
        }
        buf[EEPROM_SSID_MAX - 1] = '\0';
        strncpy(ssid, buf, ssid_size - 1);
        ssid[ssid_size - 1] = '\0';
        found = true;

        marker = EEPROM.read(EEPROM_PASS_ADDR);
        if (marker == EEPROM_WIFI_MARKER && eeprom_str_valid_at(EEPROM_PASS_ADDR + 1, EEPROM_PASS_MAX - 1))
        {
            for (int i = 0; i < EEPROM_PASS_MAX - 1; i++)
            {
                buf[i] = EEPROM.read(EEPROM_PASS_ADDR + 1 + i);
                if (buf[i] == '\0')
                    break;
            }
            buf[EEPROM_PASS_MAX - 1] = '\0';
            strncpy(pass, buf, pass_size - 1);
            pass[pass_size - 1] = '\0';
        }
    }
    EEPROM.end();
    return found;
}

static void set_relay(bool state);

static void name_to_ssid(const char *name, char *out, size_t max)
{
    size_t j = 0;
    for (size_t i = 0; name[i] && j < max - 1; i++)
    {
        char c = name[i];
        if (c >= 32 && c <= 126 && c != '"' && c != '\\')
            out[j++] = c;
    }
    while (j > 0 && out[j - 1] == ' ')
        j--;
    if (j == 0)
    {
        strncpy(out, "Lampada", max - 1);
        out[max - 1] = '\0';
        return;
    }
    out[j] = '\0';
}

static void set_relay(bool state)
{
    s_relay_state = state;
    digitalWrite(s_relay_pin, state ? RELAY_ON : !RELAY_ON);
    console.printf("Relay: %s -> %s\n", s_device_name, state ? "ON" : "OFF");

#ifdef ALEXA_ENABLED
    if (s_alexa_dev)
    {
        s_alexa_dev->setValue(state ? 255 : 0);
        s_alexa_dev->setState(state);
    }
#endif

    save_relay_state();
    if (state)
        s_on_count++;
    else
        cyclic_reset();
}

static void toggle_relay(void)
{
    set_relay(!s_relay_state);
}

#ifdef ALEXA_ENABLED
static void alexa_callback(EspalexaDevice *d)
{
    bool state = (d->getValue() > 0);
    s_last_alexa_activity = millis();
    console.printf("[%s] Alexa: %s -> %s\n", TAG, s_device_name, state ? "ON" : "OFF");
    set_relay(state);
    if (s_radio.is_paired())
    {
        s_radio.publish_state();
    }
}
#endif

static void init_hardware(void)
{
    load_relay_pin();
    load_button_pin();
    load_led_enabled();
    load_startup_mode();
    {
        EEPROM.begin(EEPROM_SIZE);
        uint8_t val = EEPROM.read(EEPROM_MULTIHUB_ADDR);
        EEPROM.end();
        s_multihub = (val == 1);
    }
    pinMode(s_relay_pin, OUTPUT);
    if (s_startup_mode == 0)
    {
        s_relay_state = false;
    }
    else if (s_startup_mode == 1)
    {
        s_relay_state = true;
    }
    else
    {
        load_relay_state();
    }
#ifdef REPEATER_ENABLED
    repeater_init(EEPROM_REPEATER_EN_ADDR);
#endif
    digitalWrite(s_relay_pin, s_relay_state ? RELAY_ON : !RELAY_ON);
    pinMode(s_button_pin, INPUT_PULLUP);
    s_button_last = digitalRead(s_button_pin);
#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);
#endif
}

static void start_ap(void)
{
    char ssid[33];
    name_to_ssid(s_device_name, ssid, sizeof(ssid));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, WIFI_CONFIG_PORTAL_PASS);
    console.printf("[%s] AP '%s' started, connect to configure WiFi\n", TAG, ssid);
}

static void hwifi_begin(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(20.5);
    s_last_reconnect_attempt = millis();

#ifdef STATIC_WIFI
    if (strlen(WIFI_SSID) > 0)
    {
        console.printf("[%s] WiFi: connecting to %s (static)...\n", TAG, WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        s_wifi_connect_start = millis();
        s_config_portal_active = false;
        return;
    }
    console.printf("[%s] No WiFi SSID configured\n", TAG);
#endif

    if (WiFi.SSID().length() > 0)
    {
        console.printf("[%s] Saved SSID: %s, connecting...\n", TAG, WiFi.SSID().c_str());
        WiFi.begin();
        s_wifi_connect_start = millis();
        s_config_portal_active = false;
        return;
    }

    char ssid[EEPROM_SSID_MAX], pass[EEPROM_PASS_MAX];
    ssid[0] = '\0';
    pass[0] = '\0';
    if (load_wifi_credentials(ssid, sizeof(ssid), pass, sizeof(pass)))
    {
        console.printf("[%s] EEPROM SSID: %s, connecting...\n", TAG, ssid);
        WiFi.begin(ssid, pass);
        s_wifi_connect_start = millis();
        s_config_portal_active = false;
        return;
    }

    console.printf("[%s] No saved WiFi, starting AP config mode\n", TAG);
    s_config_portal_active = true;
    s_wifi_config_start_time = millis();
    start_ap();
}

static void wifi_reconnect(void)
{
    unsigned long now = millis();
    if (now - s_last_reconnect_attempt < 30000)
        return;
    s_last_reconnect_attempt = now;
    console.printf("[%s] WiFi disconnected. Reconnecting...\n", TAG);
    WiFi.reconnect();
}

static void handle_wifi(void)
{
    unsigned long now = millis();

    if (WiFi.status() == WL_CONNECTED)
    {
        if (s_config_portal_active)
        {
            console.printf("[%s] WiFi connected, stopping AP\n", TAG);
            WiFi.mode(WIFI_STA);
            WiFi.softAPdisconnect(true);
            s_config_portal_active = false;
        }
        if (!s_wifi_connected)
        {
            s_wifi_connected = true;
            console.printf("[%s] WiFi connected: %s\n", TAG, WiFi.localIP().toString().c_str());
            console.printf("  => Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
#ifdef ALEXA_ENABLED
            if (!s_alexa_initialized)
            {
                bool ok = s_alexa.begin(&s_server);
                s_alexa_initialized = ok;
                console.printf("[%s] Alexa Hue Bridge: %s (init=%s)\n", TAG, s_device_name, ok ? "OK" : "FAIL");
                if (!ok)
                    console.printf("[%s] Alexa UDP multicast falhou, Alexa indisponivel\n", TAG);
                // Re-registra onNotFound após begin() do Espalexa
                s_server.onNotFound([]() {
                    if (s_alexa_initialized &&
                        s_alexa.handleAlexaApiCall(s_server.uri(), s_server.arg("plain")))
                        return;
                    s_server.send(404, "text/plain", "Not found");
                });
            }
#endif
            console.printf("  => Terminal:  'h' comando de ajuda\n");
        }
        return;
    }

    if (s_config_portal_active)
    {
        if (now - s_wifi_config_start_time > 600000)
        {
            console.printf("[%s] AP config timeout, restarting\n", TAG);
            ESP.restart();
        }
        return;
    }

    if (s_wifi_connect_start > 0)
    {
        if (now - s_wifi_connect_start > 120000)
        {
            console.printf("[%s] WiFi connect timeout, starting AP\n", TAG);
            s_config_portal_active = true;
            s_wifi_config_start_time = now;
            start_ap();
            return;
        }
        if (now - s_last_reconnect_attempt >= 30000)
        {
            s_last_reconnect_attempt = now;
            console.printf("[%s] WiFi not connected, retrying...\n", TAG);
            WiFi.reconnect();
        }
    }
    else
    {
        s_wifi_connect_start = now;
        WiFi.begin();
    }
}

static void handle_api_wifi(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID();
        doc["configured"] = (WiFi.SSID().length() > 0);
        doc["ap_active"] = s_config_portal_active;
        doc["status"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
        doc["device_name"] = s_device_name;
        doc["channel"] = mywifi_configured_channel();
        doc["wifi_channel"] = WiFi.channel();
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
        return;
    }

    if (s_server.method() == HTTP_POST)
    {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err)
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        if (doc.containsKey("ssid"))
        {
            const char *ssid = doc["ssid"];
            const char *pass = doc["password"] | "";

            if (doc.containsKey("device_name"))
            {
                const char *new_name = doc["device_name"];
                if (espnow_is_valid_name(new_name) && strcmp(s_device_name, new_name) != 0)
                {
                    strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                    s_device_name[sizeof(s_device_name) - 1] = '\0';
                    espnow_save_device_name(s_device_name);
                }
            }

#ifdef REPEATER_ENABLED
            if (doc.containsKey("repeater_mac"))
            {
                const char *mac_str = doc["repeater_mac"];
                if (strlen(mac_str) > 0 && mac_parse(mac_str, s_gateway_mac))
                {
                    repeater_set_enabled(true);
                    repeater_save_enable();
                    s_radio.set_gateway_mac(s_gateway_mac);
                    s_radio.save_gateway_mac();
                }
            }
#endif

            if (doc.containsKey("channel"))
            {
                uint8_t ch = doc["channel"];
                if (ch > 0 && ch <= 13)
                    mywifi_save_channel(ch);
            }

            console.printf("[%s] WiFi credentials received, connecting to %s...\n", TAG, ssid);
            s_server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
            save_wifi_credentials(ssid, pass);
            delay(100);
            WiFi.begin(ssid, pass);
            s_config_portal_active = false;
            s_wifi_connect_start = millis();
        }
        else
        {
            s_server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
        }
    }
}

/* Envia múltiplas seções PROGMEM diretamente via WiFiClient,
   sem montar String intermediária (evita fragmentação de heap). */
static void serve_pgm_sections(ESP8266WebServer &server, const char *const *sections, int count)
{
    /* Calcular tamanho total */
    size_t total = 0;
    for (int i = 0; i < count; i++)
    {
        if (sections[i])
            total += strlen_P(sections[i]);
    }

    WiFiClient cl = server.client();
    cl.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "));
    cl.print(total);
    cl.print(F("\r\nConnection: close\r\n\r\n"));

    /* Enviar cada seção em chunks de 256 bytes */
    char buf[256];
    for (int i = 0; i < count; i++)
    {
        if (!sections[i])
            continue;
        PGM_P src = sections[i];
        size_t remaining = strlen_P(src);
        while (remaining > 0)
        {
            size_t chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            memcpy_P(buf, src, chunk);
            cl.write((const uint8_t *)buf, chunk);
            src += chunk;
            remaining -= chunk;
            yield();
        }
    }
}

static void handle_root(void)
{
    if (s_config_portal_active)
    {
        s_server.send(200, "text/html", FPSTR(PAGE_WIFI_CONFIG));
        return;
    }
    const char *sections[] = {
        (const char *)FPSTR(PAGE_DASHBOARD),
        (const char *)FPSTR(PAGE_PINS_NAV),
        (const char *)FPSTR(PAGE_DASHBOARD_CONT1),
#ifdef REPEATER_ENABLED
        (const char *)FPSTR(PAGE_DASHBOARD_REPEATER_CFG),
#endif
        (const char *)FPSTR(PAGE_DASHBOARD_CONT3),
        (const char *)FPSTR(PAGE_PINS_SEC),
        (const char *)FPSTR(PAGE_DASHBOARD_CONT2),
        (const char *)FPSTR(PAGE_SCRIPT_PINS),
        (const char *)FPSTR(PAGE_DASHBOARD_END)};
    serve_pgm_sections(s_server, sections, sizeof(sections) / sizeof(sections[0]));
}

static void handle_api_state(void)
{
    String json;
    {
        JsonDocument doc;
        doc["state"] = s_relay_state;
        doc["button"] = (digitalRead(s_button_pin) == LOW);
        doc["battery"] = s_battery;
        doc["device_id"] = s_device_id;
        doc["device_name"] = s_device_name;
        doc["gateway_connected"] = s_radio.is_paired();
        doc["paired"] = s_radio.is_paired();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        char upbuf[32];
        uptime_to_str(millis() - s_start_time, upbuf, sizeof(upbuf));
        doc["uptime"] = upbuf;
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        doc["slot"] = s_radio.assigned_slot();
#ifdef ALEXA_ENABLED
        doc["alexa_connected"] = (s_last_alexa_activity > 0 && (millis() - s_last_alexa_activity < 600000));
#endif
#ifdef LED_PIN
        doc["led_enabled"] = (s_led_enabled ? "true" : "false");
        doc["led_state"] = (digitalRead(LED_PIN) == LED_ON ? "LIGADO" : "DESLIGADO");
#endif
        doc["fw_version"] = FW_VERSION;
        doc["platform"] = "esp8266";
        doc["type"] = "lampada";
        doc["tx_count"] = s_radio.tx_count();
        doc["rx_count"] = s_radio.rx_count();
        doc["on_count"] = s_on_count;
        doc["free_heap"] = ESP.getFreeHeap();
#ifdef REPEATER_ENABLED
        doc["repeater_supported"] = true;
        doc["repeater_active"] = (repeater_get_fwd_count() > 0) || repeater_is_enabled();
        doc["repeater_enabled"] = repeater_is_enabled();
        if (repeater_is_enabled())
        {
            doc["repeater_fwd"] = repeater_get_fwd_count();
            JsonArray rep_clients = doc["repeater_clients"].to<JsonArray>();
            const repeater_client_t *clients = repeater_get_clients();
            int client_count = repeater_get_client_count();
            for (int i = 0; i < client_count; i++)
            {
                JsonObject obj = rep_clients.add<JsonObject>();
                char mac_str[18];
                mac_to_str(clients[i].mac, mac_str, sizeof(mac_str));
                obj["mac"] = mac_str;
                obj["packets"] = clients[i].pkt_count;
            }
        }
#endif
        if (s_tz_changed)
            doc["timezone"] = s_timezone_offset;
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_api_relay(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["state"] = s_relay_state;
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
        if (doc.containsKey("state"))
        {
            bool new_state = doc["state"];
            set_relay(new_state);
            console.printf("[%s] Relay set to %s via API\n", TAG, new_state ? "ON" : "OFF");
            String json;
            JsonDocument resp;
            resp["state"] = s_relay_state;
            resp["status"] = "ok";
            serializeJson(resp, json);
            s_server.send(200, "application/json", json);
            s_last_state_update = 0;
        }
        else
        {
            s_server.send(400, "application/json", "{\"error\":\"missing state\"}");
        }
    }
}

#ifdef REPEATER_ENABLED
static void handle_api_repeater(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["enabled"] = repeater_is_enabled();
        doc["forwarded"] = repeater_get_fwd_count();
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
        if (doc.containsKey("enabled"))
        {
            bool en = doc["enabled"];
            repeater_set_enabled(en);
            repeater_save_enable();
            console.printf("[%s] Repeater %s via API\n", TAG, en ? "ATIVADO" : "desativado");
            String json;
            JsonDocument resp;
            resp["enabled"] = repeater_is_enabled();
            resp["status"] = "ok";
            serializeJson(resp, json);
            s_server.send(200, "application/json", json);
        }
        else
        {
            s_server.send(400, "application/json", "{\"error\":\"missing enabled\"}");
        }
    }
}
#endif

#ifdef PINS_ENABLED
static uint32_t GPIOMUX[17] = {
    0x000, // GPIO0
    0x000, // GPIO1
    0x000, // GPIO2
    0x000, // GPIO3
    0x000, // GPIO4
    0x000, // GPIO5
    0x000, // GPIO6
    0x000, // GPIO7
    0x000, // GPIO8
    0x000, // GPIO9
    0x000, // GPIO10
    0x000, // GPIO11
    0x000, // GPIO12
    0x000, // GPIO13
    0x000, // GPIO14
    0x000, // GPIO15
    0x000  // GPIO16
};
static uint8_t getMode(int pin)
{
    if (pin < 0 || pin > 16)
        return 0xFF;
    uint32_t reg = GPIOMUX[pin];
    if (reg & 0x100)
        return OUTPUT;
    else if (reg & 0x200)
        return INPUT_PULLUP;
    else
        return INPUT;
}
static void handle_api_pins(void)
{
    String json;
    {
        JsonDocument doc;
        JsonArray arr = doc["pins"].to<JsonArray>();
        for (int i = 0; i < AVAILABLE_GPIOS_COUNT; i++)
        {
            int gpio = AVAILABLE_GPIOS[i];
            JsonObject obj = arr.add<JsonObject>();
            obj["gpio"] = gpio;
            obj["state"] = digitalRead(gpio);
            uint8_t mode = getMode(gpio);
            obj["mode"] = (mode == OUTPUT) ? "OUT" : (mode == INPUT_PULLUP) ? "IN_PU"
                                                                            : "IN";
        }
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}
#endif

static void handle_console(char c)
{
    switch (c)
    {
    case 'R':
    case 'r':
        console.println("r - reiniciando....");
        delay(100);
        ESP.restart();
        break;
    case 'l':
    case 'L':
    {
        console.printf("\n--- Controle da Lampada ---\n");
        toggle_relay();
        console.printf("  Lampada: %s\n", s_relay_state ? "LIGADA" : "DESLIGADA");
        if (s_radio.is_paired())
        {
            s_radio.publish_state();
        }
        else
        {
            console.printf("  (gateway nao pareado)\n");
        }
        console.printf("--------------------------\n\n");
        break;
    }
    case '0':
        set_relay(false);
        console.printf("[%s] Relay OFF\n", TAG);
        if (s_radio.is_paired())
        {
            s_radio.publish_state();
        }
        break;
    case '1':
        set_relay(true);
        console.printf("[%s] Relay ON\n", TAG);
        if (s_radio.is_paired())
        {
            s_radio.publish_state();
        }
        break;
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
        console.printf("  Estado de pareamento resetado\n");
        console.printf("  Enviando requisicao de par...\n");
        console.printf("----------------\n\n");
        break;
    }
    case 'h':
    case 'H':
    case '?':
        console.printf("\n--- Comandos ---\n");
        console.printf("  l    - liga/desliga lampada\n");
        console.printf("  0    - desligar\n");
        console.printf("  1    - ligar\n");
        console.printf("  r    - reset\n");
        console.printf("  s    - status do dispositivo\n");
        console.printf("  p    - resetar par e tentar parear\n");
#ifdef REPEATER_ENABLED
        console.printf("  e    - ligar/desligar repeater\n");
        console.printf("  x    - repeater stats\n");
#endif
        console.printf("  c    - zerar contadores\n");
        console.printf("  u    - info OTA\n");
#ifdef ALEXA_ENABLED
        console.printf("  a    - info Alexa\n");
#endif
        console.printf("  h/?  - esta ajuda\n");
        console.printf("  Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
        if (s_radio.is_paired())
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            console.printf("  Gateway: %s (slot %d)\n", mac_str, s_radio.assigned_slot());
        }
        console.printf("  IP local: %s\n", WiFi.localIP().toString().c_str());
        console.printf("  RSSI:     %d dBm\n", WiFi.RSSI());
        char upbuf[32];
        uptime_to_str(millis() - s_start_time, upbuf, sizeof(upbuf));
        console.printf("  Up:       %s\n", upbuf);
        console.printf("----------------\n\n");
        break;
#ifdef ALEXA_ENABLED
    case 'a':
    case 'A':
        console.printf("\n--- Alexa ---\n");
        console.printf("  Dispositivo: %s\n", s_device_name);
        console.printf("  Protocolo:   Hue Bridge (SSDP + UPnP)\n");
        console.printf("  Dica:        \"Alexa, ligue %s\"\n", s_device_name);
        console.printf("               \"Alexa, desligue %s\"\n", s_device_name);
        console.printf("             Acesse http://%s/espalexa para status\n", WiFi.localIP().toString().c_str());
        console.printf("-------------\n\n");
        break;
#endif
#ifdef REPEATER_ENABLED
    case 'e':
    case 'E':
    {
        bool en = !repeater_is_enabled();
        repeater_set_enabled(en);
        repeater_save_enable();
        console.printf("[%s] Repeater %s\n", TAG, en ? "ATIVADO" : "DESATIVADO");
        break;
    }
    case 'x':
    case 'X':
        if (repeater_is_enabled())
        {
            console.printf("\n--- Repeater Stats ---\n");
            console.printf("  Forwarded: %lu\n", repeater_get_fwd_count());
            int cnt = repeater_get_client_count();
            console.printf("  Clients:   %d\n", cnt);
            const repeater_client_t *clients = repeater_get_clients();
            for (int i = 0; i < cnt; i++)
            {
                char mac_str[18];
                mac_to_str(clients[i].mac, mac_str, sizeof(mac_str));
                console.printf("  %d: %s (%lu pkts)\n", i, mac_str, clients[i].pkt_count);
            }
            console.printf("----------------------\n\n");
        }
        else
        {
            console.printf("[%s] Repeater mode disabled\n", TAG);
        }
        break;
#endif
    case 'c':
    case 'C':
        s_on_count = 0;
#ifdef REPEATER_ENABLED
        repeater_reset_stats();
#endif
        console.printf("[%s] Contadores zerados\n", TAG);
        break;
    case 's':
    case 'S':
    {
        unsigned long up = (millis() - s_start_time) / 1000;
        console.printf("\n--- Status ---\n");
        console.printf("  Dispositivo: %s\n", s_device_id);
        console.printf("  Nome:        %s\n", s_device_name);
        console.printf("  Lampada:     %s\n", s_relay_state ? "LIGADA" : "DESLIGADA");
        console.printf("  Bateria:     %d %%\n", s_battery);

#ifdef LED_PIN
        console.printf("  Led:         %s\n", digitalRead(LED_PIN) == LED_ON ? "LIGADO" : "DESLIGADO");
#endif

        if (s_radio.has_gateway())
            if (s_radio.is_paired())
            {
                char mac_str[18];
                mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
                console.printf("  Gateway:     %s (slot %d)\n", mac_str, s_radio.assigned_slot());
            }
            else
            {

                console.printf("  Gateway:     nao pareado\n");
            }
        console.printf("  Dashboard:   http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
#ifdef ALEXA_ENABLED
        console.printf("  Alexa:       %s (ativo)\n", s_device_name);
#endif
#ifdef REPEATER_ENABLED
        if (repeater_is_enabled())
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            console.printf("  Repeater:    %s\n", mac_str);
        }
#endif
        console.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
        console.printf("  Canal:       %d\n", WiFi.channel());
        console.printf("  Uptime:      %lu s\n", up);
        console.printf("---------------\n\n");
        break;
    }
    }
}

static bool is_valid_gpio(int pin)
{
    for (int i = 0; i < AVAILABLE_GPIOS_COUNT; i++)
    {
        if (AVAILABLE_GPIOS[i] == pin)
            return true;
    }
    return false;
}

static void handle_api_settings(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["device_name"] = s_device_name;
        doc["relay_pin"] = s_relay_pin;
        doc["button_pin"] = s_button_pin;
        doc["led_enabled"] = s_led_enabled;
        doc["startup_mode"] = s_startup_mode;
        doc["multihub"] = s_multihub;
        JsonArray pins = doc["available_pins"].to<JsonArray>();
        for (int i = 0; i < AVAILABLE_GPIOS_COUNT; i++)
            pins.add(AVAILABLE_GPIOS[i]);
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
        bool changed = false;
        if (doc.containsKey("device_name"))
        {
            const char *new_name = doc["device_name"];
            if (espnow_is_valid_name(new_name) && strcmp(s_device_name, new_name) != 0)
            {
                strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                s_device_name[sizeof(s_device_name) - 1] = '\0';
                espnow_save_device_name(s_device_name);
#ifdef ALEXA_ENABLED
                if (s_alexa_dev)
                    s_alexa_dev->setName(s_device_name);
#endif
                console.printf("[%s] Device name changed to: %s\n", TAG, s_device_name);
                changed = true;
            }
        }
        if (doc.containsKey("relay_pin"))
        {
            int new_pin = doc["relay_pin"];
            if (!is_valid_gpio(new_pin))
            {
                s_server.send(400, "application/json", "{\"error\":\"invalid relay_pin\"}");
                return;
            }
            if (new_pin != s_relay_pin)
            {
                pinMode(s_relay_pin, INPUT);
                s_relay_pin = new_pin;
                pinMode(s_relay_pin, OUTPUT);
                digitalWrite(s_relay_pin, s_relay_state ? RELAY_ON : !RELAY_ON);
                save_relay_pin();
                console.printf("[%s] Relay pin changed to GPIO%d\n", TAG, s_relay_pin);
                changed = true;
            }
        }
        if (doc.containsKey("button_pin"))
        {
            int new_pin = doc["button_pin"];
            if (!is_valid_gpio(new_pin))
            {
                s_server.send(400, "application/json", "{\"error\":\"invalid button_pin\"}");
                return;
            }
            if (new_pin != s_button_pin)
            {
                pinMode(s_button_pin, INPUT);
                s_button_pin = new_pin;
                pinMode(s_button_pin, INPUT_PULLUP);
                s_button_last = digitalRead(s_button_pin);
                save_button_pin();
                console.printf("[%s] Button pin changed to GPIO%d\n", TAG, s_button_pin);
                changed = true;
            }
        }
        if (doc.containsKey("led_enabled"))
        {
            bool new_led = doc["led_enabled"];
            if (new_led != s_led_enabled)
            {
                s_led_enabled = new_led;
                save_led_enabled();
                console.printf("[%s] LED %s\n", TAG, s_led_enabled ? "enabled" : "disabled");
                changed = true;
            }
        }
        if (doc.containsKey("startup_mode"))
        {
            int new_mode = doc["startup_mode"];
            if (new_mode >= 0 && new_mode <= 2 && new_mode != s_startup_mode)
            {
                s_startup_mode = new_mode;
                save_startup_mode();
                console.printf("[%s] Startup mode set to %d\n", TAG, s_startup_mode);
                changed = true;
            }
        }
        if (doc.containsKey("multihub"))
        {
            bool new_multihub = doc["multihub"];
            if (new_multihub != s_multihub)
            {
                s_multihub = new_multihub;
                EEPROM.begin(EEPROM_SIZE);
                EEPROM.write(EEPROM_MULTIHUB_ADDR, s_multihub ? 1 : 0);
                EEPROM.commit();
                EEPROM.end();
                console.printf("[%s] Multihub mode %s\n", TAG, s_multihub ? "ATIVADO" : "desativado");
                changed = true;
            }
        }
        if (!changed)
        {
            s_server.send(200, "application/json", "{\"status\":\"no changes\"}");
            return;
        }
        String json;
        JsonDocument resp;
        resp["device_name"] = s_device_name;
        resp["relay_pin"] = s_relay_pin;
        resp["button_pin"] = s_button_pin;
        resp["led_enabled"] = s_led_enabled;
        resp["startup_mode"] = s_startup_mode;
        resp["status"] = "ok";
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
}

static void apply_timer(int action)
{
    console.printf("[%s] Timer action: %s\n", TAG, action ? "ON" : "OFF");
    set_relay(action == 1);
}

static unsigned long get_epoch(void)
{
    if (s_synced_epoch > 0)
        return s_synced_epoch + ((millis() - s_sync_millis) / 1000);
    return 0;
}

static void handle_api_devices(void)
{
    String json;
    JsonDocument doc;
    devices_load(doc);
    JsonDocument out;
    JsonArray arr = out.to<JsonArray>();
    for (JsonVariant v : doc.as<JsonArray>())
    {
        const char *id = v["id"] | "";
        if (strcmp(id, s_device_id) == 0)
            continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = id;
        obj["name"] = v["name"] | id;
    }
    serializeJson(out, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_timers(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        timer_to_json(doc);
        JsonObject cyc = doc["cyclic"].to<JsonObject>();
        cyc["enabled"] = cyclic_get_enabled();
        cyc["duration_min"] = cyclic_get_duration();
        JsonObject pulse = doc["pulse"].to<JsonObject>();
        pulse["enabled"] = timer_pulse_get_enabled();
        pulse["duration_min"] = timer_pulse_get_duration();
        JsonObject sync = doc["sync"].to<JsonObject>();
        sync["enabled"] = s_sync_cfg.enabled;
        sync["target_device_id"] = s_sync_cfg.target_device_id;
        sync["target_device_name"] = s_sync_cfg.target_device_name;
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
        if (doc.containsKey("cyclic"))
        {
            JsonObject c = doc["cyclic"];
            if (c.containsKey("enabled"))
                cyclic_set_enabled(c["enabled"].as<bool>());
            if (c.containsKey("duration_min"))
                cyclic_set_duration(c["duration_min"].as<uint16_t>());
        }
        if (doc.containsKey("pulse"))
        {
            JsonObject p = doc["pulse"];
            if (p.containsKey("enabled"))
                timer_pulse_set_enabled(p["enabled"].as<bool>());
            if (p.containsKey("duration_min"))
                timer_pulse_set_duration(p["duration_min"].as<uint16_t>());
        }
        if (doc.containsKey("sync"))
        {
            JsonObject s = doc["sync"];
            if (s.containsKey("enabled"))
                s_sync_cfg.enabled = s["enabled"].as<bool>();
            if (s.containsKey("target_device_id"))
            {
                const char *tid = s["target_device_id"] | "";
                strncpy(s_sync_cfg.target_device_id, tid, sizeof(s_sync_cfg.target_device_id) - 1);
                s_sync_cfg.target_device_id[sizeof(s_sync_cfg.target_device_id) - 1] = '\0';
            }
            else if (s.containsKey("target_id"))
            {
                const char *tid = s["target_id"] | "";
                strncpy(s_sync_cfg.target_device_id, tid, sizeof(s_sync_cfg.target_device_id) - 1);
                s_sync_cfg.target_device_id[sizeof(s_sync_cfg.target_device_id) - 1] = '\0';
            }
            const char *tn = s["target_device_name"] | s["target_name"] | "";
            strncpy(s_sync_cfg.target_device_name, tn, sizeof(s_sync_cfg.target_device_name) - 1);
            s_sync_cfg.target_device_name[sizeof(s_sync_cfg.target_device_name) - 1] = '\0';
            sync_save();
            if (strlen(s_sync_cfg.target_device_id) > 0)
                devices_add(s_sync_cfg.target_device_id, s_sync_cfg.target_device_name);
        }
        if (doc.containsKey("index"))
        {
            int idx = doc["index"];
            if (idx >= 0 && idx < MAX_TIMERS)
            {
                timer_config_t cfg;
                cfg.hour = doc["hour"] | 0;
                cfg.minute = doc["minute"] | 0;
                cfg.action = doc["action"] | 0;
                cfg.days_mask = doc["days_mask"] | 0;
                cfg.enabled = doc["enabled"] | true;
                timer_set(idx, &cfg);
            }
        }
        else if (doc.containsKey("hour"))
        {
            int idx = -1;
            for (int i = 0; i < MAX_TIMERS; i++)
            {
                timer_config_t tmp;
                if (timer_get(i, &tmp) && !tmp.enabled)
                {
                    idx = i;
                    break;
                }
            }
            if (idx < 0)
                idx = 0;
            timer_config_t cfg;
            cfg.hour = doc["hour"] | 0;
            cfg.minute = doc["minute"] | 0;
            cfg.action = doc["action"] | 0;
            cfg.days_mask = doc["days_mask"] | 0;
            cfg.enabled = doc["enabled"] | true;
            timer_set(idx, &cfg);
            console.printf("[%s] Timer %d set to %02d:%02d %s\n", TAG, idx, cfg.hour, cfg.minute,
                           cfg.action ? "ON" : "OFF");
        }
        else if (doc.containsKey("timers"))
        {
            timer_from_json(doc);
        }
        timer_save_littlefs();
        timer_save();
        String json;
        JsonDocument resp;
        resp["status"] = "ok";
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
}

static void handle_api_timer_next(void)
{
    unsigned long next_epoch = 0;
    uint8_t next_action = 0;
    timer_get_next(get_epoch(), s_timezone_offset, &next_epoch, &next_action);
    String json;
    JsonDocument doc;
    doc["has_next"] = (next_epoch > 0);
    if (next_epoch > 0)
    {
        doc["next_epoch"] = next_epoch;
        doc["next_action"] = next_action;
        time_t t = (time_t)next_epoch;
        doc["next_local"] = ctime(&t);
    }
    serializeJson(doc, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_restart(void)
{
    s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    console.println("API - reiniciando...");
    delay(500);
    ESP.restart();
}

static void handle_api_pair(void)
{
    s_radio.force_repair();
    s_server.send(200, "application/json", "{\"status\":\"pairing\"}");
}

static void handle_ota(void)
{
    if (!Update.hasError())
    {
        console.printf("[%s] OTA concluido, reiniciando...\n", TAG);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    }
    else
    {
        s_ota_in_progress = false;
        console.printf("[%s] OTA falhou\n", TAG);
        s_server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

static void handle_ota_upload(void)
{
    HTTPUpload &upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        s_ota_in_progress = true;
        s_ota_bytes = 0;
        console.printf("[%s] OTA update started: %s\n", TAG, upload.filename.c_str());
        if (!Update.begin(ESP.getFreeSketchSpace()))
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        s_ota_bytes += upload.currentSize;
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
            console.printf("[%s] OTA update success: %u bytes\n", TAG, s_ota_bytes);
        else
            Update.printError(Serial);
    }
}

static uint8_t get_sensor_type()
{
    return SENSOR_TYPE_LIGHT;
}

static uint8_t get_sensor_payload(uint8_t *buf, uint8_t max_len)
{
    payload_onoff_t pl;
    memset(&pl, 0, sizeof(pl));
    pl.state = s_relay_state ? 1 : 0;
    uint8_t len = sizeof(pl);
    if (len > max_len)
        len = max_len;
    memcpy(buf, &pl, len);
    if (len + 1 <= max_len)
    {
        buf[len] = (uint8_t)WiFi.channel();
        len++;
    }
    return len;
}

static void on_command(uint8_t command)
{
    console.printf("[%s] Command received: %d\n", TAG, command);
    set_relay(command == 0x01);
    // if (s_radio.is_paired()) {
    s_radio.publish_state();
    //}
}

static void on_paired(uint8_t slot)
{
    console.printf("[%s] Paired, slot %d\n", TAG, slot);
}

static void on_restart()
{
    console.printf("[%s] Restart command received\n", TAG);
    delay(100);
    ESP.restart();
}

static void on_forward(const uint8_t *data, size_t len, const uint8_t *mac)
{
#ifdef REPEATER_ENABLED
    if (repeater_is_enabled() && mac_is_nonzero(s_gateway_mac) && !mac_equal(mac, s_gateway_mac))
    {
        repeater_forward(mac, data, len, s_gateway_mac, s_broadcast_mac,
                         repeater_send_adapter, TAG);
    }
#endif
}

static void on_pairing_failed()
{
    console.printf("[%s] Pairing failed on ch %d — trying next AP...\n", TAG, WiFi.channel());
    if (mywifi_try_next_bssid())
    {
        console.printf("[%s] Reconnecting, will retry pairing\n", TAG);
    }
    else
    {
        console.printf("[%s] No other APs found, will retry on current\n", TAG);
    }
}

#ifdef TCP_ENABLED
static void tcp_poll_pending_commands()
{
    if (!s_radio.is_paired())
        return;
    if (WiFi.status() != WL_CONNECTED)
        return;
    if (strlen(s_device_id) == 0)
        return;

    String url = String("http://") + WiFi.gatewayIP().toString() + ":" + String(DASHBOARD_PORT) + "/node/command/" + s_device_id;
    WiFiClient client;
    HTTPClient http;
    if (!http.begin(client, url))
        return;
    http.setTimeout(1500);
    int code = http.GET();
    if (code != 200)
    {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err)
        return;

    if (doc["command"].is<const char *>())
    {
        const char *cmd = doc["command"];
        if (strcmp(cmd, "on") == 0)
        {
            console.printf("[%s] TCP cmd: ON\n", TAG);
            set_relay(true);
            s_radio.publish_state();
        }
        else if (strcmp(cmd, "off") == 0)
        {
            console.printf("[%s] TCP cmd: OFF\n", TAG);
            set_relay(false);
            s_radio.publish_state();
        }
    }
}
#endif

void setup(void)
{
    Serial.begin(115200);
    delay(1000);
    console.begin();
    s_start_time = millis();

    if (!LittleFS.begin())
    {
        console.printf("[%s] LittleFS mount failed, formatting...\n", TAG);
        LittleFS.format();
        LittleFS.begin();
    }

    uint32_t id = chip_id();
    snprintf(s_device_id, sizeof(s_device_id), "agri_%06x", id);

    espnow_load_device_name(s_device_name, sizeof(s_device_name));
    timer_init(EEPROM_TIMER_BASE, MAX_TIMERS);
    if (timer_load_littlefs())
    {
        console.printf("[%s] timer_load_littlefs: OK\n", TAG);
    }
    else
    {
        console.printf("[%s] timer_load_littlefs: FAIL, migrating EEPROM\n", TAG);
        timer_save();
        timer_save_littlefs();
    }
    timer_save();
    sync_load();

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  AgriSense Lamp " FW_VERSION "\n");
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();
    console.printf("============================================\n");

    WiFi.hostname(strcmp(s_device_name, DEVICE_NAME) == 0 ? s_device_id : s_device_name);

    hwifi_begin();

    uint8_t my_mac[6];
    WiFi.macAddress(my_mac);
    s_radio.set_mac(my_mac);
    s_radio.set_device_name(s_device_name);
    s_radio.callbacks = {get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, on_forward, on_pairing_failed};
    s_radio.set_pair_interval(ESPNOW_PAIR_INTERVAL_MS);
    s_radio.set_heartbeat_interval(HEARTBEAT_INTERVAL);
    s_radio.set_state_interval(STATE_UPDATE_INTERVAL);
#ifdef TCP_ENABLED
    s_radio.set_device_id(s_device_id);
#endif
    s_radio.load_gateway_mac();
    s_radio.begin();
    memcpy(s_gateway_mac, s_radio.gateway_mac(), 6);

    // ── Registrar rotas do servidor ──
    s_server.on("/", handle_root);
    s_server.on("/docs", []()
                { serve_pgm_page(s_server, (const char *)FPSTR(PAGE_DOCS)); });
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/relay", handle_api_relay);

#ifdef REPEATER_ENABLED
    s_server.on("/api/repeater", HTTP_ANY, handle_api_repeater);
#endif
    s_server.on("/api/pin", HTTP_ANY, []()
                { handle_api_pin(s_server); });
#ifdef PINS_ENABLED
    s_server.on("/api/pins", HTTP_GET, handle_api_pins);
#endif
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/timers", HTTP_ANY, handle_api_timers);
    s_server.on("/api/devices", HTTP_GET, handle_api_devices);
    s_server.on("/api/timer/next", handle_api_timer_next);
    s_server.on("/api/debug", HTTP_GET, []()
                {
        String json;
        String fs;
        if (LittleFS.exists("/timers.json")) {
            File f = LittleFS.open("/timers.json", "r");
            if (f) {
                fs = f.readString();
                f.close();
            }
        }
        JsonDocument doc;
        timer_to_json(doc);
        String ram;
        serializeJson(doc, ram);
        String resp = "{\"littlefs\": " + (fs.length() ? fs : "null") + ", \"ram\": " + ram + "}";
        s_server.send(200, "application/json", resp); });
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.on("/api/pair", HTTP_POST, handle_api_pair);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);

#ifdef ALEXA_ENABLED
    // Device adicionado ANTES de begin (padrão referência)
    s_alexa_dev = new EspalexaDevice(s_device_name, alexa_callback, EspalexaDeviceType::onoff);
    s_alexa.addDevice(s_alexa_dev);
    s_alexa.setDiscoverable(true);
    // s_alexa.begin() chamado no WiFi connect (precisa de IP para UDP)
#endif

    // onNotFound registrado ANTES de server->begin() (padrão referência)
    s_server.onNotFound([]() {
        if (s_alexa_initialized &&
            s_alexa.handleAlexaApiCall(s_server.uri(), s_server.arg("plain")))
            return;
        s_server.send(404, "text/plain", "Not found");
    });

    // begin() DEPOIS de todas as rotas e onNotFound (padrão referência)
    s_server.begin();

    ota_setup(s_device_id);

    console.printf("  => Terminal:  'h' comando de ajuda\n");

    /* Check for REPEATER_MAC from config.h */
#ifdef REPEATER_ENABLED
    if (strlen(REPEATER_MAC) > 0 && mac_parse(REPEATER_MAC, s_gateway_mac))
    {
        repeater_set_enabled(true);
        repeater_save_enable();
        s_radio.set_gateway_mac(s_gateway_mac);
        s_radio.save_gateway_mac();
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Using repeater MAC: %s\n", TAG, mac_str);
    }
    else
#endif
    {
        memcpy(s_gateway_mac, s_radio.gateway_mac(), 6);
    }

    console.printf("  Timers:  %d configurados\n", MAX_TIMERS);

    console.printf("============================================\n");
    console.printf("  Pronto! Pressione 'h' para ajuda\n");
    console.printf("  Telnet: %s:23\n", WiFi.localIP().toString().c_str());
    console.printf("============================================\n\n");
}

void loop(void)
{
    console.loop();
    if (Serial.available() > 0)
    {
        handle_console(Serial.read());
    }
    if (console.telnet_available() > 0)
    {
        handle_console(console.telnet_read());
    }
    handle_wifi();
    ota_handle();
#ifdef ALEXA_ENABLED
    if (s_alexa_initialized)
    {
        s_alexa.loop();
    }
    else
    {
        s_server.handleClient();
    }
#else
    s_server.handleClient();
#endif

    if (s_ota_in_progress)
    {
        // Pausa ESP-NOW / envios durante OTA para evitar WDT e interrupção do upload
        yield();
        return;
    }

    {
        bool btn = digitalRead(s_button_pin);
        unsigned long now = millis();
        // Ignorar GPIO nos primeiros 2s apos boot (float/ruido)
        if (now - s_start_time > 2000 && btn != s_button_last && now - s_button_last_ms > 100)
        {
            s_button_last_ms = now;
            s_button_last = btn;
            if (btn == LOW)
            {
                if (now - s_btn_press_start > 3000)
                    s_btn_press_count = 0;
                if (s_btn_press_count == 0)
                    s_btn_press_start = now;
                s_btn_press_count++;
                if (s_btn_press_count >= 3)
                {
                    console.printf("[%s] 3 presses detected, restarting...\n", TAG);
                    delay(100);
                    ESP.restart();
                }
                toggle_relay();
                console.printf("[%s] Button press -> relay %s\n", TAG, s_relay_state ? "ON" : "OFF");
            }
        }
    }

    unsigned long now = millis();

    s_radio.loop();
    if (s_radio.is_paired())
    {
        memcpy(s_gateway_mac, s_radio.gateway_mac(), 6);
    }

    {
        static unsigned long last_timer_check = 0;
        if (now - last_timer_check > TIMER_CHECK_INTERVAL_MS)
        {
            last_timer_check = now;
            if (WiFi.status() == WL_CONNECTED)
            {
                int action = timer_check(get_epoch(), s_timezone_offset);
                if (action >= 0)
                {
                    apply_timer(action);
                    s_radio.publish_state();
                }
            }
        }
    }

    {
        static unsigned long last_cyclic_check = 0;
        if (now - last_cyclic_check > CYCLIC_CHECK_INTERVAL_MS)
        {
            last_cyclic_check = now;
            int8_t cyc_action = cyclic_check(now, s_relay_state);
            if (cyc_action == 1)
            {
                set_relay(true);
                console.printf("[%s] Cyclic ON\n", TAG);
                s_radio.publish_state();
            }
            else if (cyc_action == -1)
            {
                set_relay(false);
                console.printf("[%s] Cyclic OFF\n", TAG);
                s_radio.publish_state();
            }
        }
    }

#ifdef LED_PIN
    static unsigned long last_led = 0;
    if (!s_led_enabled)
    {
        digitalWrite(LED_PIN, LED_OFF);
    }
    else if (s_config_portal_active)
    {
        digitalWrite(LED_PIN, LED_ON);
    }
    else if (WiFi.status() != WL_CONNECTED)
    {
        if (now - last_led >= LED_BLINK_WIFI_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else if (!s_radio.is_paired() && s_radio.has_gateway())
    {
        if (now - last_led >= LED_BLINK_GATEWAY_MS)
        {
            last_led = now;
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        }
    }
    else
    {
        digitalWrite(LED_PIN, LED_OFF);
    }
#endif
}
