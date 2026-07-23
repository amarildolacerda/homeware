#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include <espnow.h>
#include "config.h"
#include "shared_config.h"
#include "espnow_protocol.h"
#include "common_console.h"
#include "common_espnow.h"

static const char *TAG = "esp8266-soil";

enum State {
    STATE_INIT,
    STATE_WAIT_PAIR,
    STATE_SEND_DATA,
    STATE_SLEEP
};

static State s_state = STATE_INIT;
static unsigned long s_state_start = 0;

static bool s_espnow_ready = false;
static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;
static uint16_t s_sequence = 0;

static uint8_t s_gateway_mac[6];
static bool s_has_gateway = false;
static uint16_t s_assigned_slot = 0;

static uint8_t s_my_mac[6];
static uint16_t s_soil_raw = 0;
static uint8_t s_soil_pct = 0;
static int s_battery = 100;

static unsigned long s_active_start = 0;
static bool s_config_mode = false;

static bool s_ack_received = false;
static int s_pair_attempts = 0;
static bool s_suspend = false;

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_INTERVAL_ADDR 60

static int s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

#define WAIT_PAIR_TIMEOUT_MS 5000
#define PAIR_RETRY_INTERVAL_MS 3000

static void read_sensor(void)
{
    s_soil_raw = analogRead(SOIL_ADC_PIN);
    s_soil_pct = (uint8_t)constrain(map(s_soil_raw, 0, 1024, 0, 100), 0, 100);
    static int counter = 0;
    counter++;
    if (counter > 100) {
        counter = 0;
        s_battery = max(0, s_battery - 1);
    }
                    console.printf("Sensor: raw=%d pct=%d%% bat=%d%%\n", s_soil_raw, s_soil_pct, s_battery);

}

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    (void)status;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
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
            espnow_save_gateway_mac(mac, TAG);
            s_has_gateway = true;
            char mac_str[18];
            mac_to_str(mac, mac_str, sizeof(mac_str));
            console.printf("[%s] Paired with gateway %s slot %d\n", TAG, mac_str, s_assigned_slot);
        }
        break;
    }
    case ESPNOW_MSG_ACK:
    {
        if (len < sizeof(espnow_ack_t)) return;
        espnow_ack_t *ack = (espnow_ack_t *)data;
        if (mac_equal(ack->sensor_mac, s_my_mac))
            s_ack_received = ack->status == PAIR_STATUS_OK;
        break;
    }
    }
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
    req->sensor_type = SENSOR_TYPE_SOIL_MOISTURE;
    uint32_t ver = 0x000A000B;
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';
    if (!espnow_client_add_peer(s_broadcast_mac, TAG)) return false;
    return espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG);
}

static bool espnow_send_data(void)
{
    if (!s_espnow_ready) return false;
    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_soil_moisture_t) + 4];
    memset(buf, 0, sizeof(buf));
    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_SOIL_MOISTURE;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();
    payload_soil_moisture_t *pl = (payload_soil_moisture_t *)hdr->payload;
    pl->raw_adc = s_soil_raw;
    pl->moisture_pct = s_soil_pct;
    IPAddress ip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_soil_moisture_t);
    ip_ptr[0] = ip[0]; ip_ptr[1] = ip[1]; ip_ptr[2] = ip[2]; ip_ptr[3] = ip[3];
    hdr->payload_len = sizeof(payload_soil_moisture_t) + 4;
    if (!espnow_client_add_peer(s_broadcast_mac, TAG)) return false;
    return espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG);
}

static void do_deep_sleep(void)
{
    console.printf("[%s] Deep sleep %ds\n", TAG, s_interval_s);
    Serial.flush();
    delay(200);
    ESP.deepSleep((uint64_t)s_interval_s * 1000000ULL, WAKE_RF_DEFAULT);
}

static void init_hardware(void)
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    pinMode(SOIL_CFG_BUTTON_PIN, INPUT_PULLUP);
}

static bool wifi_setup(bool force_config_portal)
{
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20);
    if (!force_config_portal && WiFi.SSID() != "")
    {
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(1);
        console.printf("[%s] Connecting to saved WiFi: %s\n", TAG, WiFi.SSID().c_str());
        if (wifiManager.autoConnect())
        {
            console.printf("[%s] WiFi connected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            return true;
        }
        console.printf("[%s] Failed to connect to saved WiFi\n", TAG);
    }
    console.printf("[%s] Starting config portal (%ds)...\n", TAG, WIFI_CONFIG_PORTAL_TIMEOUT_S);
    s_config_mode = true;
    wifiManager.setConfigPortalTimeout(WIFI_CONFIG_PORTAL_TIMEOUT_S);
    char interval_str[8];
    snprintf(interval_str, sizeof(interval_str), "%d", s_interval_s);
    WiFiManagerParameter custom_dev_name("dev_name", "Device Name", s_device_name, 32);
    WiFiManagerParameter custom_interval("interval", "Interval (s)", interval_str, 5);
    wifiManager.addParameter(&custom_dev_name);
    wifiManager.addParameter(&custom_interval);
    if (wifiManager.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_dev_name.getValue()) > 0 && strcmp(s_device_name, custom_dev_name.getValue()) != 0)
        {
            strncpy(s_device_name, custom_dev_name.getValue(), sizeof(s_device_name) - 1);
            s_device_name[sizeof(s_device_name) - 1] = '\0';
            espnow_save_device_name(s_device_name);
        }
        int new_interval = atoi(custom_interval.getValue());
        if (new_interval >= DEEP_SLEEP_INTERVAL_MIN && new_interval <= DEEP_SLEEP_INTERVAL_MAX)
        {
            s_interval_s = new_interval;
            EEPROM.begin(128);
            EEPROM.put(EEPROM_INTERVAL_ADDR, s_interval_s);
            EEPROM.commit();
            EEPROM.end();
        }
        s_config_mode = false;
        return true;
    }
    s_config_mode = false;
    return false;
}

void setup(void)
{
    Serial.begin(115200);
    delay(500);
    console.begin();
    s_active_start = millis();

    WiFi.forceSleepWake();
    delay(10);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);

    uint32_t chip_id = ESP.getChipId();
    snprintf(s_device_id, sizeof(s_device_id), "esp8266_%06x", chip_id);
    espnow_load_device_name(s_device_name, sizeof(s_device_name));
    EEPROM.begin(128);
    EEPROM.get(EEPROM_INTERVAL_ADDR, s_interval_s);
    if (s_interval_s < DEEP_SLEEP_INTERVAL_MIN || s_interval_s > DEEP_SLEEP_INTERVAL_MAX)
        s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

    console.printf("\n============================================\n");
    console.printf("  ESP8266 Soil Moisture " FW_VERSION "\n");
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n\n");

    randomSeed(analogRead(A0));
    init_hardware();

    bool button_pressed = (digitalRead(SOIL_CFG_BUTTON_PIN) == LOW);
    s_has_gateway = espnow_load_gateway_mac(s_gateway_mac, TAG);

    if (button_pressed || !s_has_gateway)
    {
        if (!wifi_setup(true))
        {
            console.printf("[%s] Config portal closed, sleeping\n", TAG);
            do_deep_sleep();
            return;
        }
    }

    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.begin();
    unsigned long wifi_deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < wifi_deadline)
    {
        delay(50);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        console.printf("[%s] WiFi timeout, sleeping\n", TAG);
        do_deep_sleep();
        return;
    }
    console.printf("[%s] WiFi OK, IP: %s\n", TAG, WiFi.localIP().toString().c_str());

    s_espnow_ready = espnow_client_init(TAG);
    if (s_espnow_ready)
    {
        esp_now_register_send_cb(espnow_send_cb);
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    WiFi.macAddress(s_my_mac);
}

void loop(void)
{
    console.loop();
    static bool s_first_loop = true;
    if (s_first_loop)
    {
        console.printf("[%s] Aguardando tecla: %s\n", TAG, "h for help");
        s_first_loop = false;
    } else {
        delay(100);
    }
    delay(1000);
    if (Serial.available() > 0 && !s_suspend)
    {
        delay(1);
        while (Serial.available()) Serial.read();
        s_suspend = true;
        console.printf("[%s] Suspenso - tecla pressionada\n", TAG);
    }
    if (s_suspend)
    {
        static bool menu_printed = false;
        if (!menu_printed)
        {
            console.printf("\nComandos: s=status, i=intervalo, r=restart, h=ajuda\n> ");
            menu_printed = true;
        }
        int c = Serial.read();
        if (c != -1)
        {
            menu_printed = false;
            console.printf("\n");
            switch (c)
            {
            case 's':
            {
                read_sensor();
                console.printf("WiFi: %s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
                console.printf("Intervalo: %ds\n", s_interval_s);
                break;
            }
            case 'i':
            {
                console.printf("Intervalo atual: %ds\n", s_interval_s);
                console.printf("Digite novo intervalo (30-3600s): ");
                unsigned long timeout = millis() + 10000;
                int idx = 0;
                char buf[8] = {0};
                while (millis() < timeout && idx < 7)
                {
                    console.loop();
                    int ch = Serial.read();
                    if (ch >= '0' && ch <= '9')
                    {
                        buf[idx++] = (char)ch;
                        Serial.write(ch);
                    }
                    else if (ch == '\n' || ch == '\r')
                        break;
                    delay(10);
                }
                if (idx > 0)
                {
                    int val = atoi(buf);
                    if (val >= DEEP_SLEEP_INTERVAL_MIN && val <= DEEP_SLEEP_INTERVAL_MAX)
                    {
                        s_interval_s = val;
                        EEPROM.begin(128);
                        EEPROM.put(EEPROM_INTERVAL_ADDR, s_interval_s);
                        EEPROM.commit();
                        EEPROM.end();
                        console.printf("\nIntervalo alterado para %ds\n", s_interval_s);
                    }
                    else
                        console.printf("\nInvalido (%d-%d)\n", DEEP_SLEEP_INTERVAL_MIN, DEEP_SLEEP_INTERVAL_MAX);
                }
                else
                    console.printf("\nCancelado\n");
                break;
            }
            case 'r':
                console.printf("Restart...\n");
                delay(100);
                ESP.restart();
                break;
            case 'h':
                console.printf("s=status i=intervalo r=restart h=ajuda\n");
                break;
            }
            console.printf("> ");
            menu_printed = true;
        }
        delay(50);
        console.loop();
        return;
    }
    unsigned long now = millis();

    switch (s_state)
    {
    case STATE_INIT:
        console.printf("[%s] Sending pair request\n", TAG);
        espnow_send_pair_request();
        s_pair_attempts = 1;
        s_state_start = now;
        if (s_has_gateway)
            s_state = STATE_SEND_DATA;
        else
            s_state = STATE_WAIT_PAIR;
        break;

    case STATE_WAIT_PAIR:
        if (s_has_gateway)
        {
            s_state = STATE_SEND_DATA;
        }
        else if (now - s_state_start > PAIR_RETRY_INTERVAL_MS)
        {
            s_pair_attempts++;
            if (s_pair_attempts >= ESPNOW_MAX_PAIR_ATTEMPTS)
            {
                console.printf("[%s] Pair failed after %d attempts\n", TAG, s_pair_attempts);
                s_state = STATE_SLEEP;
            }
            else
            {
                console.printf("[%s] Pair attempt %d/%d\n", TAG, s_pair_attempts, ESPNOW_MAX_PAIR_ATTEMPTS);
                espnow_send_pair_request();
                s_state_start = now;
            }
        }
        break;

    case STATE_SEND_DATA:
        delay(200);
        espnow_send_pair_request();
        delay(100);
        read_sensor();
        digitalWrite(LED_PIN, LOW);
        espnow_send_data();
        delay(LED_BLINK_SEND_MS);
        digitalWrite(LED_PIN, HIGH);
        s_state = STATE_SLEEP;
        break;

    case STATE_SLEEP:
        do_deep_sleep();
        break;
    }
}
