#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <espnow.h>
#include <Espalexa.h>
#include <sys/time.h>
#include "config.h"
#include "pages.h"
#include "espnow_protocol.h"
#include "console.h"
#include "timer.h"

static const char *TAG = "esp8266-onoff";

static unsigned long s_last_state_update = 0;
static unsigned long s_last_telemetry_update = 0;
static unsigned long s_last_reconnect_attempt = 0;
static unsigned long s_last_espnow_send = 0;
static unsigned long s_last_espnow_pair = 0;
static unsigned long s_last_heartbeat = 0;
static unsigned long s_last_alexa_activity = 0;
static unsigned long s_send_deadline = 0;
static int s_send_retries_left = 0;
static unsigned long s_pair_wait_until = 0;

static bool s_gateway_connected = false;
static bool s_paired = false;
static uint8_t s_gateway_mac[6];
static uint16_t s_sequence = 0;
static uint16_t s_assigned_slot = 0;
static int s_pair_attempts = 0;
static bool s_ack_received = false;
static bool s_send_pending = false;
static bool s_espnow_ready = false;

static bool s_relay_state = false;
static int s_relay_pin = RELAY_PIN;
static int s_button_pin = BUTTON_PIN;
static int s_battery = 100;
static bool s_button_last = HIGH;
static unsigned long s_button_last_ms = 0;
static unsigned long s_start_time = 0;
static unsigned long s_last_send_ms = 0;
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;
static uint32_t s_on_count = 0;

static char s_device_id[32];
static char s_device_name[32] = DEVICE_NAME;

static unsigned long s_wifi_config_start_time = 0;
static bool s_config_portal_active = false;
static bool s_use_repeater = false;
static bool s_led_enabled = true;
static int s_startup_mode = 0; // 0=OFF, 1=ON, 2=LAST
static uint8_t s_my_mac[6];
static unsigned long s_wifi_connect_start = 0;
static bool s_wifi_connected = false;

static ESP8266WebServer s_server(DASHBOARD_PORT);
static Espalexa s_alexa;
static EspalexaDevice *s_alexa_dev = nullptr;

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static unsigned long s_last_timer_check = 0;
static int s_timezone_offset = -3;
static unsigned long s_synced_epoch = 0;

static unsigned long get_synced_epoch(void) {
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
#define EEPROM_SSID_ADDR 64
#define EEPROM_SSID_MAX 32
#define EEPROM_PASS_ADDR (EEPROM_SSID_ADDR + EEPROM_SSID_MAX)
#define EEPROM_PASS_MAX 64
#define EEPROM_MAGIC 0xAA
#define EEPROM_PULSE_ENABLED_ADDR 224
#define EEPROM_PULSE_DURATION_ADDR 225

static bool s_pulse_enabled = false;
static uint16_t s_pulse_duration_min = PULSE_DEFAULT_DURATION_MIN;
static unsigned long s_pulse_on_time = 0;

static void set_relay(bool state);
static bool espnow_send_data(void);

static void on_timer_fire(uint8_t action)
{
    console.printf("[%s] Timer fired: action=%d\n", TAG, action);
    set_relay(action ? true : false);
    if (s_paired)
    {
        s_last_espnow_send = 0;
        espnow_send_data();
    }
}

static void save_gateway_mac(const uint8_t *mac)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, EEPROM_MAGIC);
    for (int i = 0; i < 6; i++)
        EEPROM.write(EEPROM_GATEWAY_MAC_ADDR + 1 + i, mac[i]);
    EEPROM.commit();
    EEPROM.end();
}

static bool load_gateway_mac(void)
{
    EEPROM.begin(EEPROM_SIZE);
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

static void save_pulse_config(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_PULSE_ENABLED_ADDR, s_pulse_enabled ? 1 : 0);
    EEPROM.write(EEPROM_PULSE_DURATION_ADDR, s_pulse_duration_min & 0xFF);
    EEPROM.write(EEPROM_PULSE_DURATION_ADDR + 1, (s_pulse_duration_min >> 8) & 0xFF);
    EEPROM.commit();
    EEPROM.end();
}

static void load_pulse_config(void)
{
    EEPROM.begin(EEPROM_SIZE);
    s_pulse_enabled = EEPROM.read(EEPROM_PULSE_ENABLED_ADDR) ? true : false;
    uint8_t lo = EEPROM.read(EEPROM_PULSE_DURATION_ADDR);
    uint8_t hi = EEPROM.read(EEPROM_PULSE_DURATION_ADDR + 1);
    EEPROM.end();
    uint16_t val = (uint16_t)lo | ((uint16_t)hi << 8);
    if (val >= PULSE_MIN_MINUTES && val <= PULSE_MAX_MINUTES)
        s_pulse_duration_min = val;
    else
        s_pulse_duration_min = PULSE_DEFAULT_DURATION_MIN;
}

static void save_device_name(const char *name)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_NAME_ADDR, 0xFF);
    for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
    {
        EEPROM.write(EEPROM_NAME_ADDR + 1 + i, name[i]);
        if (name[i] == '\0')
            break;
    }
    EEPROM.write(EEPROM_NAME_ADDR + EEPROM_NAME_MAX, '\0');
    EEPROM.commit();
    EEPROM.end();
}

static bool is_valid_name(const char *s)
{
    if (!s || s[0] == '\0')
        return false;
    for (int i = 0; s[i]; i++)
    {
        char c = s[i];
        if (c < 32 || c > 126)
            return false;
    }
    return true;
}

static void load_device_name(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t marker = EEPROM.read(EEPROM_NAME_ADDR);
    if (marker == 0xFF)
    {
        char buf[EEPROM_NAME_MAX];
        for (int i = 0; i < EEPROM_NAME_MAX - 1; i++)
        {
            buf[i] = EEPROM.read(EEPROM_NAME_ADDR + 1 + i);
            if (buf[i] == '\0')
                break;
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

static void save_wifi_credentials(const char *ssid, const char *pass)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_SSID_ADDR, 0xFF);
    EEPROM.write(EEPROM_PASS_ADDR, 0xFF);
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

static bool load_wifi_credentials(char *ssid, size_t ssid_size, char *pass, size_t pass_size)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t marker = EEPROM.read(EEPROM_SSID_ADDR);
    bool found = false;
    if (marker == 0xFF)
    {
        char buf[64];
        for (int i = 0; i < EEPROM_SSID_MAX - 1; i++)
        {
            buf[i] = EEPROM.read(EEPROM_SSID_ADDR + 1 + i);
            if (buf[i] == '\0')
                break;
        }
        buf[EEPROM_SSID_MAX - 1] = '\0';
        if (strlen(buf) > 0)
        {
            strncpy(ssid, buf, ssid_size - 1);
            ssid[ssid_size - 1] = '\0';
            found = true;
        }
        marker = EEPROM.read(EEPROM_PASS_ADDR);
        if (marker == 0xFF)
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

static bool mac_parse(const char *str, uint8_t *mac)
{
    int vals[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &vals[0], &vals[1], &vals[2],
               &vals[3], &vals[4], &vals[5]) != 6)
        return false;
    for (int i = 0; i < 6; i++)
        mac[i] = (uint8_t)vals[i];
    return true;
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
        strncpy(out, "OnOff", max - 1);
        out[max - 1] = '\0';
        return;
    }
    out[j] = '\0';
}

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    s_espnow_tx_count++;
    if (status != 0)
    {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[%s] ESPNOW send failed to %s: status=%d\n", TAG, mac_str, status);
    }
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    s_espnow_rx_count++;
    if (!data || len < 1)
        return;

    switch (data[0])
    {
    case ESPNOW_MSG_PAIR_RESPONSE:
    {
        if (len < sizeof(espnow_pair_response_t))
            return;
        espnow_pair_response_t *resp = (espnow_pair_response_t *)data;
        if (resp->status == PAIR_STATUS_OK)
        {
            if (!s_use_repeater)
            {
                mac_copy(s_gateway_mac, mac);
                save_gateway_mac(mac);
            }
            s_assigned_slot = resp->assigned_slot;
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
    case ESPNOW_MSG_COMMAND:
    {
        if (len < sizeof(espnow_command_t))
            return;
        espnow_command_t *cmd = (espnow_command_t *)data;
        if (mac_equal(cmd->target_mac, s_my_mac))
        {
            console.printf("[%s] Command for me: state=%d\n", TAG, cmd->command);
            set_relay(cmd->command ? true : false);
        }
        break;
    }
    case ESPNOW_MSG_RESTART:
    {
        if (len < sizeof(espnow_restart_t)) return;
        espnow_restart_t *rst = (espnow_restart_t *)data;
        if (mac_equal(rst->target_mac, s_my_mac))
        {
            console.printf("[%s] Restart command received, rebooting...\n", TAG);
            if (s_paired) {
                espnow_send_heartbeat();
                delay(50);
            }
            delay(100);
            ESP.restart();
        }
        break;
    }
    case ESPNOW_MSG_NAK:
    {
        if (len < sizeof(espnow_nak_t)) return;
        espnow_nak_t *nak = (espnow_nak_t *)data;
        if (nak->reason == NAK_REASON_GATEWAY_LOST)
        {
            console.printf("[%s] Gateway lost notification (NAK), re-pairing...\n", TAG);
            s_paired = false;
            s_gateway_connected = false;
            s_pair_attempts = 0;
        }
        break;
    }
    case ESPNOW_MSG_TIME_SYNC:
    {
        if (len < sizeof(espnow_time_sync_t))
            return;
        espnow_time_sync_t *ts = (espnow_time_sync_t *)data;
        s_synced_epoch = ts->epoch_seconds;
        struct timeval tv = { (time_t)s_synced_epoch, 0 };
        settimeofday(&tv, NULL);
        console.printf("[%s] Time sync: epoch=%lu seq=%d\n", TAG, s_synced_epoch, ts->sequence);
        break;
    }
    case ESPNOW_MSG_ACK:
    {
        if (len < sizeof(espnow_ack_t))
            return;
        espnow_ack_t *ack = (espnow_ack_t *)data;
        console.printf("[%s] ACK received: status=%d seq=%d slot=%d\n", TAG, ack->status, ack->sequence, ack->assigned_slot);
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

    /* Repeater: forward messages between clients and gateway */
    if (s_paired && s_use_repeater)
    {
        if (mac_equal(mac, s_gateway_mac))
        {
            /* From gateway → broadcast (other devices check target_mac) */
            esp_now_send(s_broadcast_mac, data, len);
        }
        else
        {
            /* From client → forward to gateway */
            esp_now_send(s_gateway_mac, data, len);
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
    if (!s_espnow_ready)
        return false;
    esp_now_del_peer((uint8_t *)mac);
    int ch = ESP_NOW_CHANNEL;
    int ret = esp_now_add_peer((uint8_t *)mac, ESP_NOW_ROLE_COMBO, ch, NULL, 0);
    if (ret != 0)
    {
        char mac_str[18];
        mac_to_str(mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Failed to add peer %s: %d\n", TAG, mac_str, ret);
    }
    return (ret == 0);
}

#define ESPNOW_HEADER_FIXED_SIZE (sizeof(espnow_header_t) - sizeof(((espnow_header_t *)0)->payload))

static bool espnow_send_data(void)
{
    if (!s_paired || !s_espnow_ready)
        return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_onoff_t) + 4 + 2];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_ONOFF;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();

    payload_onoff_t *pl = (payload_onoff_t *)hdr->payload;
    pl->state = s_relay_state ? 1 : 0;

    IPAddress ip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_onoff_t);
    ip_ptr[0] = ip[0];
    ip_ptr[1] = ip[1];
    ip_ptr[2] = ip[2];
    ip_ptr[3] = ip[3];

    uint16_t free_heap = ESP.getFreeHeap();
    uint8_t *fh_ptr = hdr->payload + sizeof(payload_onoff_t) + 4;
    fh_ptr[0] = free_heap & 0xFF;
    fh_ptr[1] = (free_heap >> 8) & 0xFF;

    hdr->payload_len = sizeof(payload_onoff_t) + 4 + 2;

    if (!espnow_add_peer(s_gateway_mac))
    {
        console.printf("[%s] Failed to add gateway peer\n", TAG);
        return false;
    }

    s_ack_received = false;
    s_send_pending = true;
    {
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Sending data to %s (%d bytes)\n", TAG, mac_str, sizeof(buf));
    }
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
    if (!s_paired || !s_espnow_ready)
        return false;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_HEARTBEAT;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_ONOFF;
    hdr->battery_pct = (uint8_t)s_battery;
    hdr->rssi = (int16_t)WiFi.RSSI();
    hdr->payload_len = 0;

    if (!espnow_add_peer(s_gateway_mac))
        return false;

    s_ack_received = false;
    return esp_now_send(s_gateway_mac, buf, sizeof(buf)) == 0;
}

static bool espnow_send_pair_request(void)
{
    if (!s_espnow_ready)
        return false;

    uint8_t buf[sizeof(espnow_pair_request_t)];
    memset(buf, 0, sizeof(buf));

    espnow_pair_request_t *req = (espnow_pair_request_t *)buf;
    req->msg_type = ESPNOW_MSG_PAIR_REQUEST;
    req->sequence = s_sequence++;
    WiFi.macAddress(req->sensor_mac);
    req->sensor_type = SENSOR_TYPE_ONOFF;
    uint32_t ver = 0x000A000B;
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';

    if (!espnow_add_peer(s_broadcast_mac))
        return false;

    s_ack_received = false;
    int ret = esp_now_send(s_broadcast_mac, buf, sizeof(buf));
    if (ret != 0)
    {
        console.printf("[%s] Pair request send failed: %d\n", TAG, ret);
        return false;
    }
    return true;
}

static void set_relay(bool state)
{
    if (state && !s_relay_state)
        s_on_count++;
    s_relay_state = state;
    digitalWrite(s_relay_pin, state ? RELAY_ON : !RELAY_ON);
    save_relay_state();
    if (state && s_pulse_enabled)
        s_pulse_on_time = millis();
    s_last_espnow_send = 0;
}

static void toggle_relay(void)
{
    set_relay(!s_relay_state);
}

static void alexa_callback(EspalexaDevice *d)
{
    bool state = (d->getValue() > 0);
    s_last_alexa_activity = millis();
    console.printf("[%s] Alexa: %s -> %s\n", TAG, s_device_name, state ? "ON" : "OFF");
    set_relay(state);
    if (s_paired)
    {
        s_last_espnow_send = 0;
        espnow_send_data();
    }
}

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

static void hwifi_begin(void)
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.setOutputPower(20.5);

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
            WiFi.softAPdisconnect(true);
            s_config_portal_active = false;
        }
        if (!s_wifi_connected)
        {
            s_wifi_connected = true;
            console.printf("[%s] WiFi connected: %s\n", TAG, WiFi.localIP().toString().c_str());
            console.printf("  => Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
          #ifdef HABILITA_ALEXA
            console.printf("  => Alexa:     \"Alexa, ligue %s\"\n", s_device_name);
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
                if (is_valid_name(new_name) && strcmp(s_device_name, new_name) != 0)
                {
                    strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                    s_device_name[sizeof(s_device_name) - 1] = '\0';
                    save_device_name(s_device_name);
                }
            }

            if (doc.containsKey("repeater_mac"))
            {
                const char *mac_str = doc["repeater_mac"];
                if (strlen(mac_str) > 0 && mac_parse(mac_str, s_gateway_mac))
                {
                    s_use_repeater = true;
                    s_paired = true;
                    save_gateway_mac(s_gateway_mac);
                }
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

static void serve_pgm_page(const char *page)
{
    size_t total = strlen_P(page);
    WiFiClient cl = s_server.client();
    cl.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "));
    cl.print(total);
    cl.print(F("\r\nConnection: close\r\n\r\n"));
    PGM_P src = page;
    char buf[256];
    while (total > 0)
    {
        size_t chunk = total > sizeof(buf) ? sizeof(buf) : total;
        memcpy_P(buf, src, chunk);
        cl.write((const uint8_t *)buf, chunk);
        src += chunk;
        total -= chunk;
        yield();
    }
}

static void handle_root(void)
{
    if (s_config_portal_active)
        s_server.send(200, "text/html", FPSTR(PAGE_WIFI_CONFIG));
    else
        serve_pgm_page(PAGE_DASHBOARD);
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
        doc["gateway_connected"] = s_gateway_connected;
        doc["paired"] = s_paired;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["uptime_s"] = (millis() - s_start_time) / 1000;
        if (s_last_send_ms)
            doc["last_send_s"] = (millis() - s_last_send_ms) / 1000;
        doc["slot"] = s_assigned_slot;
        doc["alexa_connected"] = (s_last_alexa_activity > 0 && (millis() - s_last_alexa_activity < 600000));
#ifdef LED_PIN
        doc["led_enabled"] = (s_led_enabled ? "true" : "false");
        doc["led_state"] = (digitalRead(LED_PIN) == LED_ON ? "LIGADO" : "DESLIGADO");
#endif
        doc["fw_version"] = FW_VERSION;
        doc["current_epoch"] = get_synced_epoch();
        doc["pulse_enabled"] = s_pulse_enabled;
        doc["pulse_duration_min"] = s_pulse_duration_min;
        if (s_pulse_enabled && s_relay_state)
            doc["pulse_remaining_s"] = (s_pulse_duration_min * 60000 - (millis() - s_pulse_on_time)) / 1000;
        doc["tx_count"] = s_espnow_tx_count;
        doc["rx_count"] = s_espnow_rx_count;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["on_count"] = s_on_count;
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
        console.printf("--------------------------\n\n");
        break;
    }
    case '0':
        set_relay(false);
        console.printf("[%s] Relay OFF\n", TAG);
        if (s_paired)
        {
            s_last_espnow_send = 0;
            espnow_send_data();
        }
        break;
    case '1':
        set_relay(true);
        console.printf("[%s] Relay ON\n", TAG);
        if (s_paired)
        {
            s_last_espnow_send = 0;
            espnow_send_data();
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
   #ifdef HABILITA_ALEXA     
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
        save_pulse_config();
        console.printf("[%s] Pulse %s (%d min)\n", TAG, s_pulse_enabled ? "ativado" : "desativado", s_pulse_duration_min);
        break;
    case 's':
    case 'S':
    {
        unsigned long up = (millis() - s_start_time) / 1000;
        console.printf("\n--- Status ---\n");
        console.printf("  Dispositivo: %s\n", s_device_id);
        console.printf("  Nome:        %s\n", s_device_name);
        console.printf("  OnOff:       %s\n", s_relay_state ? "LIGADO" : "DESLIGADO");
        console.printf("  Bateria:     %d %%\n", s_battery);

#ifdef LED_PIN
        console.printf("  Led:         %s\n", digitalRead(LED_PIN) == LED_ON ? "LIGADO" : "DESLIGADO");
#endif

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
        console.printf("  Dashboard:   http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
        console.printf("  Alexa:       %s (ativo)\n", s_device_name);
        if (s_use_repeater)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
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
            if (is_valid_name(new_name) && strcmp(s_device_name, new_name) != 0)
            {
                strncpy(s_device_name, new_name, sizeof(s_device_name) - 1);
                s_device_name[sizeof(s_device_name) - 1] = '\0';
                save_device_name(s_device_name);
                if (s_alexa_dev)
                    s_alexa_dev->setName(s_device_name);
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
        int timer_index = doc["index"] | -1;
        bool ok;
        if (timer_index >= 0) {
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
            // single timer without index — find first empty slot or append
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
        save_pulse_config();
        String json;
        JsonDocument resp;
        resp["status"] = "ok";
        resp["enabled"] = s_pulse_enabled;
        resp["duration_minutes"] = s_pulse_duration_min;
        serializeJson(resp, json);
        s_server.send(200, "application/json", json);
    }
}

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
    console.printf("  ESP8266 OnOff " FW_VERSION "\n");
    console.printf("  Device: %s\n", s_device_id);
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n");

    randomSeed(analogRead(A0));
    init_hardware();
    console.printf("============================================\n");

    hwifi_begin();

    espnow_init_client();
    WiFi.macAddress(s_my_mac);

    s_alexa_dev = new EspalexaDevice(s_device_name, alexa_callback, EspalexaDeviceType::onoff);
    s_alexa.addDevice(s_alexa_dev);
    s_alexa.begin(&s_server);
    console.printf("[%s] Alexa Hue Bridge: %s ready\n", TAG, s_device_name);

    s_server.on("/", handle_root);
    s_server.on("/docs", []()
                { serve_pgm_page(PAGE_DOCS); });
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/relay", handle_api_relay);
    s_server.on("/api/pin", HTTP_ANY, handle_api_pin);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.on("/api/timers", HTTP_ANY, handle_api_timers);
    s_server.on("/api/timer/next", handle_api_timer_next);
    s_server.on("/api/pulse", HTTP_ANY, handle_api_pulse);
    /* s_server.begin() is called by Espalexa internally */

    ArduinoOTA.setHostname(s_device_id);
    ArduinoOTA.onStart([]()
                       { console.printf("[%s] OTA update start\n", TAG); });
    ArduinoOTA.onEnd([]()
                     { console.printf("[%s] OTA update end\n", TAG); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          { console.printf("[%s] OTA progress: %u%%\r", TAG, (progress * 100) / total); });
    ArduinoOTA.onError([](ota_error_t error)
                       { console.printf("[%s] OTA error: %d\n", TAG, error); });
    ArduinoOTA.begin();
    console.printf("[%s] OTA ready: %s.local\n", TAG, s_device_id);

    console.printf("  => Terminal:  'h' comando de ajuda\n");

    /* Check for REPEATER_MAC from config.h */
    if (strlen(REPEATER_MAC) > 0 && mac_parse(REPEATER_MAC, s_gateway_mac))
    {
        s_use_repeater = true;
        s_paired = true;
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Using repeater MAC: %s\n", TAG, mac_str);
    }
    else if (load_gateway_mac())
    {
        console.printf("[%s] Gateway MAC loaded from EEPROM\n", TAG);
        s_paired = true;
    }
    else
    {
        console.printf("[%s] No saved gateway MAC, will pair\n", TAG);
    }

    timer_init(EEPROM_TIMER_BASE, MAX_TIMERS);
    console.printf("[%s] Timer module initialized\n", TAG);
    load_pulse_config();
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
    handle_wifi();
    ArduinoOTA.handle();
    s_alexa.loop();

    {
        bool btn = digitalRead(s_button_pin);
        unsigned long now = millis();
        if (btn != s_button_last && now - s_button_last_ms > 50)
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

    if (!s_paired)
    {
        if (s_pair_wait_until > 0 && now < s_pair_wait_until)
            return;
        if (now - s_last_espnow_pair > ESPNOW_PAIR_INTERVAL_MS)
        {
            s_last_espnow_pair = now;
            s_pair_attempts++;
            console.printf("[%s] Pair attempt %d/%d\n", TAG, s_pair_attempts, ESPNOW_MAX_PAIR_ATTEMPTS);
            espnow_send_pair_request();
            if (s_pair_attempts >= ESPNOW_MAX_PAIR_ATTEMPTS)
            {
                s_pair_attempts = 0;
                s_pair_wait_until = now + 5000;
                console.printf("[%s] Max pair attempts, waiting 60s\n", TAG);
            }
        }
        return;
    }

    if (s_send_pending)
    {
        if (s_ack_received)
        {
            s_send_pending = false;
            s_gateway_connected = true;
            s_last_send_ms = millis();
        }
        else if (now > s_send_deadline)
        {
            if (s_send_retries_left > 0)
            {
                s_send_retries_left--;
                s_ack_received = false;
                if (espnow_send_data())
                    s_send_deadline = millis() + ESPNOW_ACK_TIMEOUT_MS;
                else
                    s_send_pending = false;
            }
            else
            {
                s_send_pending = false;
                s_gateway_connected = false;
                console.printf("[%s] Send failed, re-pairing\n", TAG);
                s_paired = false;
                s_pair_attempts = 0;
                s_last_espnow_pair = 0;
            }
        }
    }

    if (!s_send_pending && now - s_last_espnow_send > STATE_UPDATE_INTERVAL)
    {
        s_last_espnow_send = now;
        s_ack_received = false;
        if (espnow_send_data())
        {
            s_send_pending = true;
            s_send_deadline = now + ESPNOW_ACK_TIMEOUT_MS;
            s_send_retries_left = ESPNOW_SEND_RETRIES;
        }
    }

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

    if (s_pulse_enabled && s_relay_state && (now - s_pulse_on_time > (unsigned long)s_pulse_duration_min * 60000))
    {
        console.printf("[%s] Pulse timeout (%d min), turning OFF\n", TAG, s_pulse_duration_min);
        set_relay(false);
        if (s_paired)
        {
            s_last_espnow_send = 0;
            if (espnow_send_data())
                s_last_send_ms = millis();
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
        digitalWrite(LED_PIN, LED_OFF);
    }
#endif

}
