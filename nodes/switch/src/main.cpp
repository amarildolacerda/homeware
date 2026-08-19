#include <Arduino.h>
#include "platform.h"
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#ifdef ALEXA_ENABLED
#include <Espalexa.h>
#endif
#include <sys/time.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"
#include "radio_node_strategy.h"
#include "common_console.h"
#include "common_espnow.h"
#include "common_web.h"
#include "common_repeater.h"
#include "common_wifi.h"
#include "common_util.h"
#include "timer.h"
#include <LittleFS.h>

static const char *TAG = "agri-switch";

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_heartbeat = 0;
static unsigned long s_last_alexa_activity = 0;

static bool s_relay_state = false;
static int s_relay_pin = RELAY_PIN;
static int s_button_pin = BUTTON_PIN;
static int s_battery = 100;
static bool s_button_last = HIGH;
static unsigned long s_button_last_ms = 0;
static unsigned long s_start_time = 0;
static uint32_t s_on_count = 0;

static char s_device_name[32] = DEVICE_NAME;

static bool s_led_enabled = true;
static bool s_ota_in_progress = false;
static int s_startup_mode = 0; // 0=OFF, 1=ON, 2=LAST

static MyWebServer s_server(DASHBOARD_PORT);
#ifdef ALEXA_ENABLED
static Espalexa s_alexa;
static EspalexaDevice *s_alexa_dev = nullptr;
static bool s_alexa_initialized = false;
#endif

static NodeRadioType s_radio;

static unsigned long s_last_timer_check = 0;
static unsigned long s_last_cyclic_check = 0;
static int s_timezone_offset = -3;
static unsigned long s_synced_epoch = 0;

static unsigned long get_synced_epoch(void) {
    time_t t = time(NULL);
    if (t > 100000) return (unsigned long)t;
    if (s_synced_epoch > 0)
        return s_synced_epoch + (millis() / 1000);
    return 0;
}


// D1-MINI é invtido
#define LED_ON  LOW   // GPIO2 acende com LOW
#define LED_OFF HIGH  // GPIO2 apaga com HIGH


#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_RELAY_STATE_ADDR (EEPROM_NAME_ADDR + EEPROM_NAME_MAX + 1)
#define EEPROM_RELAY_PIN_ADDR (EEPROM_RELAY_STATE_ADDR + 1)
#define EEPROM_BUTTON_PIN_ADDR (EEPROM_RELAY_PIN_ADDR + 1)
#define EEPROM_LED_ENABLED_ADDR (EEPROM_BUTTON_PIN_ADDR + 1)
#define EEPROM_STARTUP_MODE_ADDR (EEPROM_LED_ENABLED_ADDR + 1)
/* EEPROM_SSID_ADDR/EEPROM_PASS_ADDR removidos — conflitavam com myWiFiManager
   (shared/src/shared_config.h usa offset 0 para SSID e 33 para PASS). */
#define EEPROM_MAGIC 0xAA

static bool s_pulse_enabled = false;
static uint16_t s_pulse_duration_min = PULSE_DEFAULT_DURATION_MIN;
static unsigned long s_pulse_on_time = 0;

static void set_relay(bool state);
static void on_timer_fire(uint8_t action)
{
    console.printf("[%s] Timer fired: action=%d\n", TAG, action);
    set_relay(action ? true : false);
    if (s_radio.is_paired())
    {
        s_radio.publish_state();
    }
}

static void save_relay_state(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_RELAY_STATE_ADDR, s_relay_state ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
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
        strncpy(out, "OnOff", max - 1);
        out[max - 1] = '\0';
        return;
    }
    out[j] = '\0';
}

static uint8_t get_sensor_type() {
    return SENSOR_TYPE_ONOFF;
}

static uint8_t get_sensor_payload(uint8_t* buf, uint8_t max_len) {
    payload_onoff_t pl;
    memset(&pl, 0, sizeof(pl));
    pl.state = s_relay_state ? 1 : 0;
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
    if (command == 0x01) {
        set_relay(true);
        if (s_radio.is_paired()) {
            s_radio.publish_state();
        }
    } else if (command == 0x00) {
        set_relay(false);
        if (s_radio.is_paired()) {
            s_radio.publish_state();
        }
    }
}

static void on_paired(uint8_t slot) {
    console.printf("[%s] Paired, slot %d\n", TAG, slot);
}

static void on_restart() {
    console.printf("[%s] Restart command received\n", TAG);
    if (s_radio.is_paired()) {
        s_radio.publish_state();
        delay(50);
    }
    delay(100);
    ESP.restart();
}

static void set_relay(bool state)
{
    if (state && !s_relay_state)
        s_on_count++;
    s_relay_state = state;
    digitalWrite(s_relay_pin, state ? RELAY_ON : !RELAY_ON);

#ifdef ALEXA_ENABLED
    if (s_alexa_dev)
    {
        s_alexa_dev->setValue(state ? 255 : 0);
        s_alexa_dev->setState(state);
    }
#endif

    save_relay_state();
    if (state && s_pulse_enabled)
        s_pulse_on_time = millis();
    if (!state)
        cyclic_reset();

    /* Publicar estado no gateway (regra 14). O publish e guardado pelo radio,
       entao no boot (nao registrado) e no-op. */
    if (s_radio.is_paired())
        s_radio.publish_state();
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
    WiFi.softAP(ssid, WIFI_CONFIG_PORTAL_PASS);
    console.printf("[%s] AP '%s' started, connect to configure WiFi\n", TAG, ssid);
}

static void on_wifi_connected(void)
{
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
        s_server.onNotFound([]()
                            {
            if (s_alexa_initialized &&
                s_alexa.handleAlexaApiCall(s_server.uri(), s_server.arg("plain")))
                return;
            s_server.send(404, "text/plain", "Not found"); });
    }
#endif
    console.printf("  => Terminal:  'h' comando de ajuda\n");
}

static void handle_api_wifi(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID();
        doc["configured"] = (WiFi.SSID().length() > 0);
        doc["ap_active"] = (mywifi_state() == WIFI_STATE_PORTAL);
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

            if (doc.containsKey("repeater_mac"))
            {
                uint8_t gw_mac[6];
                const char *mac_str = doc["repeater_mac"];
                if (strlen(mac_str) > 0 && mac_parse(mac_str, gw_mac))
                {
                    repeater_set_enabled(true);
                    repeater_save_enable();
                    s_radio.set_gateway_mac(gw_mac);
                    espnow_save_gateway_mac(gw_mac, TAG);
                }
            }

            if (doc.containsKey("channel"))
            {
                uint8_t ch = doc["channel"];
                if (ch > 0 && ch <= 13)
                    mywifi_save_channel(ch);
            }

            console.printf("[%s] WiFi credentials received, connecting to %s...\n", TAG, ssid);
            s_server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Connecting...\"}");
            mywifi_save_creds(ssid, pass);
            delay(100);
            WiFi.begin(ssid, pass);
        }
        else
        {
            s_server.send(400, "application/json", "{\"error\":\"missing ssid\"}");
        }
    }
}

static void handle_root(void)
{
    if (mywifi_state() == WIFI_STATE_PORTAL)
        s_server.send(200, "text/html", FPSTR(PAGE_WIFI_CONFIG));
    else
        serve_pgm_page(s_server, PAGE_DASHBOARD);
}

static void handle_api_state(void)
{
    String json;
    {
        JsonDocument doc;
        doc["state"] = s_relay_state;
        doc["button"] = (digitalRead(s_button_pin) == LOW);
        doc["battery"] = s_battery;
        doc["device_id"] = getDeviceId();
        doc["device_name"] = s_device_name;
        doc["gateway_connected"] = s_radio.is_paired();
        doc["paired"] = s_radio.is_paired();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
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
#if defined(ARDUINO_ARCH_ESP32)
        doc["platform"] = "esp32";
#else
        doc["platform"] = "esp8266";
#endif
        doc["current_epoch"] = get_synced_epoch();
        doc["pulse_enabled"] = s_pulse_enabled;
        doc["pulse_duration_min"] = s_pulse_duration_min;
        if (s_pulse_enabled && s_relay_state)
            doc["pulse_remaining_s"] = (s_pulse_duration_min * 60000 - (millis() - s_pulse_on_time)) / 1000;
        doc["type"] = "onoff";
        doc["tx_count"] = s_radio.tx_count();
        doc["rx_count"] = s_radio.rx_count();
        doc["free_heap"] = ESP.getFreeHeap();
        doc["on_count"] = s_on_count;
#ifdef REPEATER_ENABLED
        doc["repeater_supported"] = true;
        doc["repeater_enabled"] = repeater_is_enabled();
        doc["repeater_fwd"] = repeater_get_fwd_count();
        {
            JsonArray arr = doc["repeater_clients"].to<JsonArray>();
            int n = repeater_get_client_count();
            const repeater_client_t *clients = repeater_get_clients();
            for (int i = 0; i < n; i++) {
                JsonObject c = arr.add<JsonObject>();
                char mac[18];
                snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                         clients[i].mac[0], clients[i].mac[1], clients[i].mac[2],
                         clients[i].mac[3], clients[i].mac[4], clients[i].mac[5]);
                c["mac"] = mac;
                c["packets"] = clients[i].pkt_count;
            }
        }
#endif
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

static void handle_console(char c)
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
        console.printf("\n--- Controle do OnOff ---\n");
        toggle_relay();
        console.printf("  OnOff: %s\n", s_relay_state ? "LIGADO" : "DESLIGADO");
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
        console.printf("  Hostname: %s.local\n", getDeviceId());
        console.printf("  Port:     8266 (ArduinoOTA)\n");
        console.printf("  PlatformIO CLI:\n");
        console.printf("    pio run -t upload --upload-port %s.local\n", getDeviceId());
        console.printf("  espota.py:\n");
        console.printf("    espota.py -i %s.local -p 8266 -f firmware.bin\n", getDeviceId());
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
    case 't':
    case 'T':
    {
        console.printf("\n--- Timers ---\n");
        timer_config_t cfg;
        for (int i = 0; i < MAX_TIMERS; i++)
        {
            if (timer_get(i, &cfg))
            {
                const char *days = cfg.days_mask == 0 ? "todos" : "dias";
                console.printf("  %d: %02d:%02d %s [%s] %s\n",
                               i, cfg.hour, cfg.minute,
                               cfg.action ? "ON " : "OFF",
                               cfg.enabled ? "ativado" : "desativado",
                               days);
            }
        }
        console.printf("---------------\n\n");
        break;
    }
    case 'h':
    case 'H':
    case '?':
        console.printf("\n--- Comandos ---\n");
        console.printf("  l    - liga/desliga onoff\n");
        console.printf("  0    - desligar\n");
        console.printf("  1    - ligar\n");
        console.printf("  r    - reset\n");
        console.printf("  s    - status do dispositivo\n");
        console.printf("  p    - resetar par e tentar parear\n");
        console.printf("  t    - listar timers\n");
        console.printf("  i    - ativar/desativar pulse\n");
        console.printf("  u    - info OTA\n");
        console.printf("  a    - info Alexa\n");
        console.printf("  h/?  - esta ajuda\n");
        console.printf("  Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
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
    case 'i':
    case 'I':
        s_pulse_enabled = !s_pulse_enabled;
        if (s_pulse_enabled && s_relay_state)
            s_pulse_on_time = millis();
        timer_pulse_set_enabled(s_pulse_enabled);
        timer_save_littlefs();
        console.printf("[%s] Pulse %s (%d min)\n", TAG, s_pulse_enabled ? "ativado" : "desativado", s_pulse_duration_min);
        break;
    case 's':
    case 'S':
    {
        unsigned long up = (millis() - s_start_time) / 1000;
        console.printf("\n--- Status ---\n");
        console.printf("  Dispositivo: %s\n", getDeviceId());
        console.printf("  Nome:        %s\n", s_device_name);
        console.printf("  OnOff:       %s\n", s_relay_state ? "LIGADO" : "DESLIGADO");
        console.printf("  Bateria:     %d %%\n", s_battery);

#ifdef LED_PIN
        console.printf("  Led:         %s\n", digitalRead(LED_PIN) == LED_ON ? "LIGADO" : "DESLIGADO");
#endif

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
        console.printf("  Dashboard:   http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
        console.printf("  Alexa:       %s (ativo)\n", s_device_name);
        if (repeater_is_enabled())
        {
            char mac_str[18];
            mac_to_str(s_radio.gateway_mac(), mac_str, sizeof(mac_str));
            console.printf("  Repeater:    %s\n", mac_str);
        }
        console.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
        console.printf("  Uptime:      %lu s\n", up);
        console.printf("  Timers:      %d configurados\n", MAX_TIMERS);
        console.printf("  Epoch:       %lu\n", get_synced_epoch());
        console.printf("  Pulse:       %s (%d min)\n", s_pulse_enabled ? "ON" : "OFF", s_pulse_duration_min);
        if (s_pulse_enabled && s_relay_state)
            console.printf("  Pulso rest.: %lu s\n", (s_pulse_duration_min * 60000 - (millis() - s_pulse_on_time)) / 1000);
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

static void handle_api_timers(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        timer_to_json(doc);
        JsonObject c = doc["cyclic"].to<JsonObject>();
        c["enabled"] = cyclic_get_enabled();
        c["duration_min"] = cyclic_get_duration();
        JsonObject p = doc["pulse"].to<JsonObject>();
        p["enabled"] = s_pulse_enabled;
        p["duration_min"] = s_pulse_duration_min;
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
        bool ok = true;
        if (doc.containsKey("cyclic")) {
            JsonObject c = doc["cyclic"];
            if (c.containsKey("enabled")) cyclic_set_enabled(c["enabled"]);
            if (c.containsKey("duration_min")) cyclic_set_duration(c["duration_min"]);
        }
        if (doc.containsKey("pulse")) {
            JsonObject p = doc["pulse"];
            if (p.containsKey("enabled")) {
                s_pulse_enabled = p["enabled"];
                timer_pulse_set_enabled(s_pulse_enabled);
                if (s_pulse_enabled && s_relay_state) s_pulse_on_time = millis();
            }
            if (p.containsKey("duration_min")) {
                int val = p["duration_min"];
                if (val < PULSE_MIN_MINUTES) val = PULSE_MIN_MINUTES;
                if (val > PULSE_MAX_MINUTES) val = PULSE_MAX_MINUTES;
                s_pulse_duration_min = (uint16_t)val;
                timer_pulse_set_duration(s_pulse_duration_min);
            }
        }
        int timer_index = doc["index"] | -1;
        if (doc.containsKey("cyclic") || doc.containsKey("pulse")) {
            /* cyclic/pulse-only update, no timer set needed */
        } else if (timer_index >= 0) {
            timer_config_t cfg;
            cfg.hour = doc["hour"] | 0;
            cfg.minute = doc["minute"] | 0;
            cfg.action = doc["action"] | 0;
            cfg.days_mask = doc["days_mask"] | 0;
            cfg.enabled = doc["enabled"] | false;
            ok = timer_set(timer_index, &cfg);
        } else if (doc.containsKey("timers")) {
            ok = timer_from_json(doc);
        } else {
            timer_config_t cfg;
            cfg.hour = doc["hour"] | 0;
            cfg.minute = doc["minute"] | 0;
            cfg.action = doc["action"] | 0;
            cfg.days_mask = doc["days_mask"] | 0;
            cfg.enabled = doc["enabled"] | false;
            for (timer_index = 0; timer_index < MAX_TIMERS; timer_index++) {
                timer_config_t tmp;
                timer_get(timer_index, &tmp);
                if (!tmp.enabled) { ok = timer_set(timer_index, &cfg); break; }
            }
            if (timer_index >= MAX_TIMERS) ok = false;
        }
        if (ok)
        {
            timer_save_littlefs();
            timer_save();
            String json;
            JsonDocument resp;
            resp["status"] = "ok";
            serializeJson(resp, json);
            s_server.send(200, "application/json", json);
        }
        else
        {
            s_server.send(400, "application/json", "{\"error\":\"invalid timer config\"}");
        }
    }
}

static void handle_api_timer_next(void)
{
    unsigned long epoch = get_synced_epoch();
    if (!epoch)
    {
        s_server.send(503, "application/json", "{\"error\":\"no time sync\"}");
        return;
    }
    unsigned long next_epoch = 0;
    uint8_t next_action = 0;
    timer_get_next(epoch, s_timezone_offset, &next_epoch, &next_action);
    String json;
    JsonDocument doc;
    if (next_epoch > 0)
    {
        doc["has_next"] = true;
        doc["next_epoch"] = next_epoch;
        doc["next_action"] = next_action;
    }
    else
    {
        doc["has_next"] = false;
    }
    serializeJson(doc, json);
    s_server.send(200, "application/json", json);
}

static void handle_api_pulse(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["enabled"] = s_pulse_enabled;
        doc["duration_minutes"] = s_pulse_duration_min;
        doc["remaining_s"] = (s_pulse_enabled && s_relay_state) ?
            ((s_pulse_duration_min * 60000 - (millis() - s_pulse_on_time)) / 1000) : 0;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    }
    else if (s_server.method() == HTTP_POST)
    {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) { s_server.send(400, "application/json", "{\"error\":\"invalid JSON\"}"); return; }
        if (doc.containsKey("enabled"))
            s_pulse_enabled = doc["enabled"];
        if (doc.containsKey("duration_minutes"))
        {
            int val = doc["duration_minutes"];
            if (val < PULSE_MIN_MINUTES) val = PULSE_MIN_MINUTES;
            if (val > PULSE_MAX_MINUTES) val = PULSE_MAX_MINUTES;
            s_pulse_duration_min = (uint16_t)val;
        }
        if (s_pulse_enabled && s_relay_state)
            s_pulse_on_time = millis();
        timer_pulse_set_enabled(s_pulse_enabled);
        timer_pulse_set_duration(s_pulse_duration_min);
        timer_save_littlefs();
        String json;
        JsonDocument resp;
        resp["status"] = "ok";
        resp["enabled"] = s_pulse_enabled;
        resp["duration_minutes"] = s_pulse_duration_min;
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
}

#ifdef REPEATER_ENABLED
static void handle_api_repeater(void)
{
    if (s_server.method() == HTTP_GET) {
        String json;
        JsonDocument doc;
        doc["enabled"] = repeater_is_enabled();
        doc["fwd_count"] = repeater_get_fwd_count();
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    } else if (s_server.method() == HTTP_POST) {
        String body = s_server.arg("plain");
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            s_server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"invalid JSON\"}");
            return;
        }
        if (doc.containsKey("enabled")) {
            repeater_set_enabled(doc["enabled"]);
            repeater_save_enable();
        }
        String json;
        JsonDocument resp;
        resp["status"] = "ok";
        resp["enabled"] = repeater_is_enabled();
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
}
#endif

static void handle_api_restart(void)
{
    s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    delay(500);
    ESP.restart();
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
        s_ota_in_progress = true;
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

    getDeviceId(); // initializes device ID from chip_id()

    espnow_load_device_name(s_device_name, sizeof(s_device_name));

    console.printf("\n");
    console.printf("============================================\n");
    console.printf("  AgriSense Switch " FW_VERSION "\n");
    console.printf("  Device: %s\n", getDeviceId());
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();
    console.printf("============================================\n");

    mywifi_begin(false);

    uint8_t my_mac[6];
    WiFi.macAddress(my_mac);
    s_radio.set_mac(my_mac);
    s_radio.set_device_name(s_device_name);
    s_radio.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr, on_pairing_failed };
    s_radio.load_gateway_mac();
    s_radio.begin();

#ifdef ALEXA_ENABLED
    s_alexa_dev = new EspalexaDevice(s_device_name, alexa_callback, EspalexaDeviceType::onoff);
    s_alexa.addDevice(s_alexa_dev);
    console.printf("[%s] Alexa device created: %s (begin() postponed until WiFi connects)\n", TAG, s_device_name);
#endif

    s_server.on("/", handle_root);
    s_server.on("/docs", []()
                { serve_pgm_page(s_server, PAGE_DOCS); });
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/relay", handle_api_relay);
    s_server.on("/api/pin", HTTP_ANY, []() { handle_api_pin(s_server); });
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.on("/api/timers", HTTP_ANY, handle_api_timers);
    s_server.on("/api/timer/next", handle_api_timer_next);
    s_server.on("/api/pulse", HTTP_ANY, handle_api_pulse);
#ifdef REPEATER_ENABLED
    s_server.on("/api/repeater", HTTP_ANY, handle_api_repeater);
#endif
    /* s_server.begin() is called by Espalexa internally */

    ArduinoOTA.setHostname(getDeviceId());
    ArduinoOTA.onStart([]()
                       { console.printf("[%s] OTA update start\n", TAG); });
    ArduinoOTA.onEnd([]()
                     { console.printf("[%s] OTA update end\n", TAG); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { console.printf("[%s] OTA progress: %u%%\r", TAG, (progress * 100) / total); });
    ArduinoOTA.onError([](ota_error_t error)
                       { console.printf("[%s] OTA error: %d\n", TAG, error); });
    ArduinoOTA.begin();
    console.printf("[%s] OTA ready: %s.local\n", TAG, getDeviceId());

    console.printf("  => Terminal:  'h' comando de ajuda\n");

    if (!LittleFS.begin()) {
        console.printf("[%s] LittleFS mount failed, formatting...\n", TAG);
        LittleFS.format();
        LittleFS.begin();
    }

    repeater_init(EEPROM_REPEATER_EN_ADDR);

    /* Check for REPEATER_MAC from config.h */
    if (strlen(REPEATER_MAC) > 0)
    {
        uint8_t gw_mac[6];
        if (mac_parse(REPEATER_MAC, gw_mac))
        {
            repeater_set_enabled(true);
            repeater_save_enable();
            s_radio.set_gateway_mac(gw_mac);
            char mac_str[18];
            mac_to_str(gw_mac, mac_str, sizeof(mac_str));
            console.printf("[%s] Using repeater MAC: %s\n", TAG, mac_str);
        }
    }

    timer_init(EEPROM_TIMER_BASE, MAX_TIMERS);
    if (!timer_load_littlefs()) {
        timer_save();
        timer_save_littlefs();
    }
    timer_save();
    console.printf("[%s] Timer module initialized (LittleFS)\n", TAG);

    s_pulse_enabled = timer_pulse_get_enabled();
    s_pulse_duration_min = timer_pulse_get_duration();
    console.printf("[%s] Pulse: %s (%d min)\n", TAG, s_pulse_enabled ? "ON" : "OFF", s_pulse_duration_min);

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
    {
        static wifi_state_t s_prev_wifi_state = WIFI_STATE_DISCONNECTED;
        mywifi_loop();
        wifi_state_t cur = mywifi_state();
        if (cur == WIFI_STATE_CONNECTED && s_prev_wifi_state != WIFI_STATE_CONNECTED) {
            on_wifi_connected();
        }
        s_prev_wifi_state = cur;
    }
    ArduinoOTA.handle();
#ifdef ALEXA_ENABLED
    s_alexa.loop();
#endif
    s_server.handleClient();

    if (s_ota_in_progress)
    {
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
                toggle_relay();
                console.printf("[%s] Button press -> relay %s\n", TAG, s_relay_state ? "ON" : "OFF");
            }
        }
    }

    unsigned long now = millis();

    s_radio.loop();

    if (now - s_last_timer_check > TIMER_CHECK_INTERVAL_MS)
    {
        s_last_timer_check = now;
        unsigned long epoch = get_synced_epoch();
        int8_t timer_action = epoch ? timer_check(epoch, s_timezone_offset) : -1;
        if (timer_action >= 0)
        {
            on_timer_fire((uint8_t)timer_action);
        }
    }

    if (now - s_last_cyclic_check > CYCLIC_CHECK_INTERVAL_MS)
    {
        s_last_cyclic_check = now;
        int8_t ca = cyclic_check(now, s_relay_state);
        if (ca == 1) { console.printf("[%s] Cyclic ON\n", TAG); set_relay(true); }
        else if (ca == -1) { console.printf("[%s] Cyclic OFF\n", TAG); set_relay(false); }
    }

    if (s_pulse_enabled && s_relay_state && (now - s_pulse_on_time > (unsigned long)s_pulse_duration_min * 60000))
    {
        console.printf("[%s] Pulse timeout (%d min), turning OFF\n", TAG, s_pulse_duration_min);
        set_relay(false);
        if (s_radio.is_paired())
        {
            s_radio.publish_state();
        }
    }

    if (now - s_last_heartbeat > HEARTBEAT_INTERVAL)
    {
        s_last_heartbeat = now;
        console.printf("[%s] RSSI=%d dBm  up=%lus\n", TAG, WiFi.RSSI(), (millis() - s_start_time) / 1000);
    }

#ifdef LED_PIN
    static unsigned long last_led = 0;
    if (!s_led_enabled)
    {
        digitalWrite(LED_PIN, LED_OFF);
    }
    else if (mywifi_state() == WIFI_STATE_PORTAL)
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
        digitalWrite(LED_PIN, LED_OFF);
    }
#endif

}
