#include <Arduino.h>
#include "platform.h"
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <EEPROM.h>
#include "config.h"
#include "shared_config.h"
#include "espnow_protocol.h"
#include "common_console.h"
#include "common_espnow.h"
#include "common_util.h"
#include "radio_node_strategy.h"
#include "dashboard.h"

#ifdef TCP_ENABLED
#define WIFI_ENABLED
#endif

#if defined(WIFI_ENABLED) && defined(ESPNOW_ENABLED)
#error "Escolha entre WIFI_ENABLED (TCP) e ESPNOW_ENABLED (mutuamente exclusivos)"
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_sleep.h>
#endif

/*
power: battery
O objetivo é fazer acquisição do sinal no pino ACQUERY_PIN (GPIO4) e enviar via ESP-NOW para o gateway. O estado do sensor é 1 quando o pino está HIGH, e 0 quando está LOW. O estado é mantido por ACQUIRY_HOLD_TIME_MS (30s) após o pino voltar a LOW.
Funciona somente indicando se esta ligado ou desligado, não é um sensor de presença completo. O dispositivo envia o estado do sensor e a duração da ocupação (em segundos) para o gateway. O gateway pode então processar os dados e enviar para o servidor.
O dispositivo entra em deep sleep por s_interval_s segundos (default 60s) após enviar
*/

static const char *TAG = "agri-acquiry-bat";

enum PowerMode { BATTERY_MODE, POWERED_MODE };
static PowerMode s_power_mode = BATTERY_MODE;

static PowerMode detect_power_source() {
#if defined(POWERED_USB) && POWERED_USB
    return POWERED_MODE;
#elif POWER_DETECT_PIN >= 0
#if defined(ARDUINO_ARCH_ESP32)
    pinMode(POWER_DETECT_PIN, INPUT_PULLDOWN);
#else
    pinMode(POWER_DETECT_PIN, INPUT);
#endif
    delay(10);
    int v = digitalRead(POWER_DETECT_PIN);
    if (POWER_DETECT_ACTIVE_HIGH) return (v == HIGH) ? POWERED_MODE : BATTERY_MODE;
    return (v == LOW) ? POWERED_MODE : BATTERY_MODE;
#else
    return BATTERY_MODE;
#endif
}

enum State
{
    STATE_INIT,
    STATE_WAIT_RADIO,
    STATE_WAIT_PAIR,
    STATE_READ_SENSOR,
    STATE_SEND_DATA,
    STATE_WAIT_ACK,
    STATE_SLEEP
};

static State s_state = STATE_INIT;
static unsigned long s_state_start = 0;

static bool s_espnow_ready = false;
#ifdef TCP_ENABLED
static NodeRadioType s_wifi_radio;
#endif
static char s_device_name[32] = DEVICE_NAME;
static uint16_t s_sequence = 0;

static uint8_t s_gateway_mac[6];
static bool s_has_gateway = false;
static uint16_t s_assigned_slot = 0;

static uint8_t s_my_mac[6];
static uint8_t s_espnow_mac[6];
static uint8_t s_acquiry_state = 0;
static unsigned long s_acquiry_start_time = 0;
static int s_battery = 100;
static int s_read_attempts = 0;
static bool s_read_valid = false;

static unsigned long s_active_start = 0;
static bool s_config_mode = false;

static bool s_ack_received = false;
static int s_pair_attempts = 0;
static int s_data_retries = 0;
static bool s_suspend = false;
static bool s_suspend_from_telnet = false;
static uint32_t s_tx_count = 0;
static uint32_t s_rx_count = 0;

// Dual Power — Powered Mode (dashboard web + OTA, sem deep sleep)
static AcqStatus s_status = {};
static unsigned long s_last_read_ms = 0;
static unsigned long s_last_send_ms = 0;
static unsigned long s_last_led_ms = 0;
static unsigned long s_last_reconnect_ms = 0;

static uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define EEPROM_GATEWAY_MAC_ADDR 0
#define EEPROM_GATEWAY_MAC_SIZE 6
#define EEPROM_NAME_ADDR 10
#define EEPROM_NAME_MAX 48
#define EEPROM_INTERVAL_ADDR 60

static int s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

#define WAIT_PAIR_TIMEOUT_MS 10000
#define PAIR_RETRY_INTERVAL_MS 10000
#define ACK_TIMEOUT_MS 10000
#define MAX_DATA_RETRIES 2
#define MAX_READ_RETRIES 3

#ifdef TCP_ENABLED
static uint8_t get_sensor_type(void)
{
    return SENSOR_TYPE_ACQUIRY;
}

static uint8_t get_sensor_payload(uint8_t *buf, uint8_t max_len)
{
    if (max_len < sizeof(payload_int_t))
        return 0;
    payload_int_t pl = {};
    pl.value = s_acquiry_state;
    pl.accepts_command = 0; // somente leitura: não aceita comando on/off do hub
    strncpy(pl.label, s_acquiry_state ? ACQ_LABEL_ON : ACQ_LABEL_OFF, sizeof(pl.label) - 1);
    pl.label[sizeof(pl.label) - 1] = '\0';
    memcpy(buf, &pl, sizeof(pl));
    return sizeof(pl);
}

static void on_command(uint8_t command)
{
    console.printf("[%s] Command received: %u\n", TAG, command);
}

static void on_paired(uint8_t slot)
{
    console.printf("[%s] Registered with hub, slot %u\n", TAG, slot);
}

static void on_restart(void)
{
    console.printf("[%s] Restart command received\n", TAG);
    delay(100);
    ESP.restart();
}
#endif

/* Bateria via ADC. Desabilitada por padrão (BATTERY_ADC_PIN < 0 em config.h)
   enquanto o divisor de tensão da placa final não é definido — nesse caso
   reporta 100% sem simulação/drift. */
static int read_battery(void)
{
#if defined(BATTERY_ADC_PIN) && (BATTERY_ADC_PIN >= 0)
    int raw = analogRead(BATTERY_ADC_PIN);
    if (raw < BATTERY_ADC_MIN)
        raw = BATTERY_ADC_MIN;
    if (raw > BATTERY_ADC_MAX)
        raw = BATTERY_ADC_MAX;
    // ADC -> tensão no pino (0..3.3V) -> tensão da bateria via divisor
    float v_pin = (float)(raw - BATTERY_ADC_MIN) * 3.3f / (float)(BATTERY_ADC_MAX - BATTERY_ADC_MIN);
    float v_bat = v_pin * (float)BATTERY_DIVIDER_RATIO;
    // faixa útil da Li-ion: 3.3V (0%) .. 4.2V (100%), clamp 0..100
    float pct = (v_bat - 3.3f) * 100.0f / (4.2f - 3.3f);
    if (pct < 0.0f)
        pct = 0.0f;
    if (pct > 100.0f)
        pct = 100.0f;
    return (int)(pct + 0.5f);
#else
    return 100; // ADC não habilitado (BATTERY_ADC_PIN < 0)
#endif
}

static void read_acquiry(bool verbose = true)
{
    // Semântica nível+hold (ver cabeçalho): HIGH → estado 1; LOW → mantém 1 por
    // ACQUIRY_HOLD_TIME_MS e volta a 0. A versão anterior usava borda com
    // `static old = !val` — após deep sleep os statics zeram e o primeiro call
    // sempre detectava "borda" falsa (pino em GND lia 1).
    int acquiry_val = digitalRead(ACQUERY_PIN);
    if (acquiry_val)
    {
        s_acquiry_state = 1;
        s_acquiry_start_time = millis();
    }
    else if (s_acquiry_state && (millis() - s_acquiry_start_time > ACQUIRY_HOLD_TIME_MS))
    {
        s_acquiry_state = 0;
    }
    s_battery = read_battery();
    s_status.value = s_acquiry_state;
    s_status.label = s_acquiry_state ? ACQ_LABEL_ON : ACQ_LABEL_OFF;
    s_status.battery = s_battery;
    if (verbose)
        console.printf("Sensor: acquiry=%d bat=%d%%\n", s_acquiry_state, s_battery);
    s_read_valid = true;
}

// --- Dual Power: Powered Mode ---

// Sincroniza o estado do node com o AcqStatus publicado pelo dashboard
static void dashboard_sync(void)
{
#ifdef TCP_ENABLED
    s_status.paired = s_wifi_radio.is_paired();
    s_status.slot = s_wifi_radio.assigned_slot();
    s_status.tx_count = s_wifi_radio.tx_count();
    s_status.rx_count = s_wifi_radio.rx_count();
#else
    s_status.paired = s_has_gateway;
    s_status.slot = s_assigned_slot;
    s_status.tx_count = s_tx_count;
    s_status.rx_count = s_rx_count;
#endif
    s_status.device_name = s_device_name;
    s_status.interval_s = s_interval_s;
}

// Callbacks do dashboard (/api/settings) — persistência + estado local
static void cfg_on_name(const char *name)
{
    strncpy(s_device_name, name, sizeof(s_device_name) - 1);
    s_device_name[sizeof(s_device_name) - 1] = '\0';
    espnow_save_device_name(s_device_name);
    console.printf("[%s] Nome alterado: %s\n", TAG, s_device_name);
}

static void cfg_on_interval(int seconds)
{
    s_interval_s = seconds;
    EEPROM.begin(128);
    EEPROM.put(EEPROM_INTERVAL_ADDR, s_interval_s);
    EEPROM.commit();
    EEPROM.end();
    console.printf("[%s] Intervalo: %ds\n", TAG, s_interval_s);
}

static const AcqConfigCb s_cfg_cb = {cfg_on_name, cfg_on_interval};

#ifdef TCP_ENABLED
// Estado completo via TCP (JSON com battery/ip/heap; o rádio publica value/label a cada estado)
static void send_state_payload(void)
{
    JsonDocument doc;
    doc["device_id"] = getDeviceId();
    doc["state"] = s_acquiry_state != 0;
    doc["battery_pct"] = s_battery;
    doc["fw_version"] = FW_VERSION;
    doc["ip"] = WiFi.localIP().toString();
    doc["free_heap"] = ESP.getFreeHeap();
    String payload;
    serializeJson(doc, payload);
    s_ack_received = s_wifi_radio.send_to_hub("/node/state", payload);
}
#endif

#if !defined(TCP_ENABLED) && defined(ARDUINO_ARCH_ESP32)
extern "C" void espnow_send_cb(const uint8_t *mac, esp_now_send_status_t status)
{
    (void)mac;
    (void)status;
}
#elif !defined(TCP_ENABLED)
extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    (void)status;
}
#endif

#ifndef TCP_ENABLED
static void handle_espnow_recv(const uint8_t *mac, const uint8_t *data, int len)
{
    if (!data || len < 1)
        return;
    size_t data_len = (size_t)len;
    s_rx_count++;
    switch (data[0])
    {
    case MSG_PAIR_RESPONSE:
    {
        if (data_len < sizeof(espnow_pair_response_t))
            return;
        const espnow_pair_response_t *resp = (const espnow_pair_response_t *)data;
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
    case MSG_ACK:
    {
        if (data_len < sizeof(espnow_ack_t))
            return;
        const espnow_ack_t *ack = (const espnow_ack_t *)data;
        if (ack->status == PAIR_STATUS_OK)
            s_ack_received = true;
        break;
    }
    case MSG_RESTART:
    {
        if (data_len < sizeof(espnow_restart_t))
            return;
        const espnow_restart_t *rst = (const espnow_restart_t *)data;
        if (memcmp(rst->target_mac, s_my_mac, 6) == 0)
        {
            console.printf("[%s] Restart command from gateway\n", TAG);
            delay(100);
            ESP.restart();
        }
        break;
    }
    }
}

#if defined(ARDUINO_ARCH_ESP32)
extern "C" void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len)
{
    handle_espnow_recv(mac, data, len);
}
#else
extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    handle_espnow_recv(mac, data, len);
}
#endif

/* Converte FW_VERSION ("v1.2.28") nos 4 bytes de firmware_version
   (bytes = 1, 2, 28, 0). Fallback para a constante legada se o parse falhar. */
static uint32_t parse_firmware_version(void)
{
    int major = 0, minor = 0, patch = 0;
    if (sscanf(FW_VERSION, "v%d.%d.%d", &major, &minor, &patch) == 3 &&
        major >= 0 && minor >= 0 && patch >= 0 &&
        major <= 255 && minor <= 255 && patch <= 255)
    {
        return ((uint32_t)major << 24) | ((uint32_t)minor << 16) | ((uint32_t)patch << 8);
    }
    return 0x000A000B;
}

static bool espnow_send_pair_request(void)
{
    if (!s_espnow_ready)
        return false;
    uint8_t buf[sizeof(espnow_pair_request_t)];
    memset(buf, 0, sizeof(buf));
    espnow_pair_request_t *req = (espnow_pair_request_t *)buf;
    req->msg_type = MSG_PAIR_REQUEST;
    req->sequence = s_sequence++;
    WiFi.macAddress(req->sensor_mac);
    req->sensor_type = SENSOR_TYPE_ACQUIRY;
    uint32_t ver = parse_firmware_version();
    req->firmware_version[0] = (uint8_t)(ver >> 24);
    req->firmware_version[1] = (uint8_t)(ver >> 16);
    req->firmware_version[2] = (uint8_t)(ver >> 8);
    req->firmware_version[3] = (uint8_t)(ver);
    strncpy(req->device_name, s_device_name, sizeof(req->device_name) - 1);
    req->device_name[sizeof(req->device_name) - 1] = '\0';
    if (!espnow_client_add_peer(s_broadcast_mac, TAG))
        return false;
    return espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG);
}
#endif

#ifndef TCP_ENABLED
static bool espnow_send_data(void)
{
    if (!s_espnow_ready)
        return false;
    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_int_t) + 4];
    memset(buf, 0, sizeof(buf));
    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = MSG_SENSOR_DATA;
    hdr->sequence = s_sequence++;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_ACQUIRY;
    hdr->battery_pct = (uint8_t)s_battery;
    // Core retorna 31 (fail) quando não conectado → enviar 0 explícito (lib-1)
    hdr->rssi = (WiFi.status() == WL_CONNECTED) ? (int16_t)WiFi.RSSI() : 0;
    payload_int_t *pl = (payload_int_t *)hdr->payload;
    pl->value = s_acquiry_state;
    pl->accepts_command = 0; // somente leitura: não aceita comando on/off do hub
    strncpy(pl->label, s_acquiry_state ? ACQ_LABEL_ON : ACQ_LABEL_OFF, sizeof(pl->label) - 1);
    pl->label[sizeof(pl->label) - 1] = '\0';
    IPAddress ip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_int_t);
    ip_ptr[0] = ip[0];
    ip_ptr[1] = ip[1];
    ip_ptr[2] = ip[2];
    ip_ptr[3] = ip[3];
    hdr->payload_len = sizeof(payload_int_t) + 4;
    if (!espnow_client_add_peer(s_broadcast_mac, TAG))
        return false;
    if (espnow_send_wrapper(s_broadcast_mac, buf, sizeof(buf), TAG))
    {
        s_tx_count++;
        return true;
    }
    return false;
}
#endif

static void do_deep_sleep(int sleep_s)
{
    console.printf("[%s] Deep sleep %ds\n", TAG, sleep_s);
    Serial.flush();
    delay(200);
#if defined(ARDUINO_ARCH_ESP32)
    esp_err_t result = esp_sleep_enable_timer_wakeup((uint64_t)sleep_s * 1000000ULL);
    if (result != ESP_OK)
    {
        console.printf("[%s] Failed to configure deep sleep timer: %d\n", TAG, result);
        return;
    }
    esp_deep_sleep_start();
#else
    ESP.deepSleep((uint64_t)sleep_s * 1000000ULL, WAKE_RF_DEFAULT);
#endif
}

static void init_hardware(void)
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    pinMode(ACQUERY_PIN, INPUT_PULLUP);
    pinMode(CFG_BUTTON_PIN, INPUT_PULLUP);
}

static bool wifi_setup(bool force_config_portal)
{
    WiFiManager wifiManager;
    wifiManager.setConnectTimeout(20); // conexão limitada a 20s (STA e portal)
    if (!force_config_portal && WiFi.SSID() != "")
    {
        // Caminho normal: credenciais salvas. O portal só é aberto se esta
        // tentativa falhar (ou se não houver credenciais salvas / tecla).
        wifiManager.setTimeout(180);
        wifiManager.setConnectRetries(1);
        console.printf("[%s] Connecting to saved WiFi: %s\n", TAG, WiFi.SSID().c_str());
        if (wifiManager.autoConnect(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
        {
            console.printf("[%s] WiFi connected! IP: %s\n", TAG, WiFi.localIP().toString().c_str());
            return true;
        }
        console.printf("[%s] Failed to connect to saved WiFi\n", TAG);
    }
    else
    {
        console.printf("[%s] %s\n", TAG,
                       force_config_portal ? "Portal forcado pela tecla" : "Sem credenciais salvas");
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

// --- Dual Power: Powered Mode (USB/fonte) — WiFi contínuo + dashboard + OTA ---
static void setup_powered(void)
{
    // WiFi contínuo: conexão normal; falha → portal; após expirar, segue
    // offline e deixa powered_maintain_wifi() tentar novamente no loop.
    if (!wifi_setup(false))
    {
        console.printf("[%s] Portal expirou, iniciando offline\n", TAG);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        console.printf("[%s] Telnet: %s:23\n", TAG, WiFi.localIP().toString().c_str());
    }
#ifdef ESP32
    WiFi.setSleep(false);
#else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

#ifdef TCP_ENABLED
    read_acquiry();
    dashboard_sync();
    uint8_t my_mac[6];
    WiFi.macAddress(my_mac);
    s_wifi_radio.set_mac(my_mac);
    s_wifi_radio.set_device_name(s_device_name);
    s_wifi_radio.set_device_id(getDeviceId());
    s_wifi_radio.set_state_interval(STATE_UPDATE_INTERVAL);
    s_wifi_radio.set_heartbeat_interval(HEARTBEAT_INTERVAL);
    s_wifi_radio.callbacks = {get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr};
    s_wifi_radio.begin();
    console.printf("[%s] Powered Mode: TCP pronto\n", TAG);
#else
    s_espnow_ready = espnow_client_init(TAG);
    if (s_espnow_ready)
    {
        esp_now_register_send_cb(espnow_send_cb);
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    WiFi.macAddress(s_my_mac);
    memcpy(s_espnow_mac, s_my_mac, 6);
    s_espnow_mac[0] ^= 0x02;
    char mac_str[18];
    mac_to_str(s_espnow_mac, mac_str, sizeof(mac_str));
    console.printf("[%s] My MAC: %s, ESP-NOW: %s\n", TAG, WiFi.macAddress().c_str(), mac_str);
    s_has_gateway = espnow_load_gateway_mac(s_gateway_mac, TAG);
    read_acquiry();
    dashboard_sync();
    console.printf("[%s] Powered Mode: ESP-NOW pronto (ch %d)\n", TAG, WiFi.channel());
#endif

    dashboard_begin(&s_status, &s_cfg_cb);
    console.printf("[%s] Powered Mode ativo — sem deep sleep\n", TAG);
}

void setup(void)
{
    Serial.begin(115200);
    delay(500);
    s_power_mode = detect_power_source();
    Serial.printf("Power mode: %s\n", s_power_mode == POWERED_MODE ? "POWERED" : "BATTERY");
    s_active_start = millis();

#if !defined(ARDUINO_ARCH_ESP32)
    WiFi.forceSleepWake();
    delay(10);
#endif
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    console.begin();
    console.set_banner(DEVICE_NAME);

    getDeviceId(); // initializes device ID from chip_id()
    espnow_load_device_name(s_device_name, sizeof(s_device_name));
    EEPROM.begin(128);
    EEPROM.get(EEPROM_INTERVAL_ADDR, s_interval_s);
    if (s_interval_s < DEEP_SLEEP_INTERVAL_MIN || s_interval_s > DEEP_SLEEP_INTERVAL_MAX)
        s_interval_s = DEEP_SLEEP_INTERVAL_DEFAULT;

    console.printf("\n============================================\n");
    console.printf("  AgriSense Acquiry Bat " FW_VERSION "\n");
    console.printf("  Device: %s\n", getDeviceId());
    console.printf("  Nome:   %s\n", s_device_name);
    console.printf("============================================\n\n");

    randomSeed(analogRead(A0));
    init_hardware();

    // Dual Power: Powered Mode tem fluxo próprio (dashboard + OTA, sem deep sleep)
    if (s_power_mode == POWERED_MODE)
    {
        setup_powered();
        return;
    }

    bool button_pressed = (digitalRead(CFG_BUTTON_PIN) == LOW);
#ifndef TCP_ENABLED
    s_has_gateway = espnow_load_gateway_mac(s_gateway_mac, TAG);
#endif
    bool force_config_portal = button_pressed;
#ifdef WIFI_ENABLED
    if (!wifi_setup(force_config_portal))
    {
        console.printf("[%s] Config portal closed, sleeping\n", TAG);
        do_deep_sleep(s_interval_s);
        return;
    }
#else
    // ESP-NOW puro: não conecta ao AP (sem IP). Portal WiFiManager só pela
    // tecla, para configurar device_name/intervalo sem depender do STA.
    if (force_config_portal)
    {
        if (!wifi_setup(true))
        {
            console.printf("[%s] Config portal closed, sleeping\n", TAG);
            do_deep_sleep(s_interval_s);
            return;
        }
    }
#endif

#ifdef ESP32
    WiFi.setSleep(false);
#else
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

#ifdef WIFI_ENABLED
    if (WiFi.status() != WL_CONNECTED)
        WiFi.begin();
    unsigned long wifi_deadline = millis() + WIFI_CONNECT_TIMEOUT_MS;
    while (WiFi.status() != WL_CONNECTED && millis() < wifi_deadline)
    {
        delay(50);
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        console.printf("[%s] WiFi timeout, sleeping\n", TAG);
        do_deep_sleep(s_interval_s);
        return;
    }
    console.printf("[%s] WiFi OK, IP: %s\n", TAG, WiFi.localIP().toString().c_str());
#else
    // ESP-NOW puro: conectar ao AP SÓ para herdar o canal (regra 25). Sem
    // portal e sem exigir IP — se falhar, segue no canal atual (sem dormir).
    // Pesquisa lib-1 (core 3.1.2): STA com creds salvas reconecta sozinho e
    // pula de canal (quebra ESP-NOW de canal fixo) → autoReconnect off;
    // WiFi.disconnect() apaga as creds → usar disconnect(false, false).
    WiFi.setAutoReconnect(false);
    if (WiFi.SSID() != "")
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            WiFi.begin(); // creds salvas: associa e herda o canal do AP
            unsigned long ch_deadline = millis() + WIFI_INHERIT_TIMEOUT_MS;
            while (WiFi.status() != WL_CONNECTED && millis() < ch_deadline)
            {
                delay(50);
            }
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            console.printf("[%s] Channel inherited from AP: %d, IP: %s\n",
                           TAG, WiFi.channel(), WiFi.localIP().toString().c_str());
        }
        else
        {
            // Para retries/scans que pulariam de canal; mantém as creds salvas.
            WiFi.disconnect(false, false);
            console.printf("[%s] AP join timeout, keeping channel %d\n", TAG, WiFi.channel());
        }
    }
    else
    {
        console.printf("[%s] No saved WiFi, ESP-NOW on channel %d\n", TAG, WiFi.channel());
    }
#endif

#ifdef TCP_ENABLED
    read_acquiry();
    uint8_t my_mac[6];
    WiFi.macAddress(my_mac);
    s_wifi_radio.set_mac(my_mac);
    s_wifi_radio.set_device_name(s_device_name);
    s_wifi_radio.set_device_id(getDeviceId());
    s_wifi_radio.set_state_interval(STATE_UPDATE_INTERVAL);
    s_wifi_radio.set_heartbeat_interval(HEARTBEAT_INTERVAL);
    s_wifi_radio.callbacks = {get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr};
    s_wifi_radio.begin();
    s_state = STATE_WAIT_PAIR;
    s_state_start = millis();
    console.printf("[%s] WiFi/TCP ready, hub registration pending\n", TAG);
#else
    s_espnow_ready = espnow_client_init(TAG);
    if (s_espnow_ready)
    {
        esp_now_register_send_cb(espnow_send_cb);
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    WiFi.macAddress(s_my_mac);
    memcpy(s_espnow_mac, s_my_mac, 6);
    s_espnow_mac[0] ^= 0x02;
    char mac_str[18];
    mac_to_str(s_espnow_mac, mac_str, sizeof(mac_str));
    console.printf("[%s] My MAC: %s, ESP-NOW: %s\n", TAG, WiFi.macAddress().c_str(), mac_str);
    s_state_start = millis();
    s_state = STATE_WAIT_RADIO;
#endif
}

// Menu serial/telnet — compartilhado entre Battery e Powered Mode
static void run_console_command(char c)
{
    switch (c)
    {
    case 's':
    {
        read_acquiry();
        console.printf("WiFi: %s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        console.printf("Modo: %s\n", s_power_mode == POWERED_MODE ? "POWERED" : "BATTERY");
        console.printf("Intervalo: %ds\n", s_interval_s);
        break;
    }
    case 'i':
    {
        console.printf("Intervalo atual: %ds\n", s_interval_s);
        console.printf("Digite novo intervalo (%d-%d s): ", DEEP_SLEEP_INTERVAL_MIN, DEEP_SLEEP_INTERVAL_MAX);
        unsigned long timeout = millis() + 10000;
        int idx = 0;
        char buf[8] = {0};
        while (millis() < timeout && idx < 7)
        {
            console.loop();
            int ch = Serial.read();
            if (ch == -1)
                ch = console.telnet_read();
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
        console.printf("s=status i=intervalo r=restart p=parear h=ajuda\n");
        if (s_power_mode == POWERED_MODE && WiFi.status() == WL_CONNECTED)
            console.printf("Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
        break;
    case 'p':
        console.printf("[%s] Re-pairing...\n", TAG);
#ifdef TCP_ENABLED
        s_wifi_radio.force_repair();
#else
        s_has_gateway = false;
        memset(s_gateway_mac, 0, sizeof(s_gateway_mac));
        s_assigned_slot = 0;
#endif
        s_suspend = false;
        s_state = STATE_INIT;
        break;
    default:
        break;
    }
}

// --- Dual Power: Powered Mode loop (sem deep sleep; dashboard/OTA/console ativos) ---
static void powered_maintain_wifi(void)
{
    unsigned long now = millis();
    if (now - s_last_reconnect_ms < POWERED_WIFI_RETRY_MS)
        return;
    s_last_reconnect_ms = now;
    console.printf("[%s] WiFi caiu, reconectando\n", TAG);
    WiFi.begin(); // creds salvas
}

static void loop_powered(void)
{
    console.loop();
    dashboard_loop();

    // Console interativa sem suspender (não há deep sleep)
    while (Serial.available() > 0)
        run_console_command((char)Serial.read());
    int tc;
    while ((tc = console.telnet_read()) >= 0)
        run_console_command((char)tc);

    if (WiFi.status() != WL_CONNECTED)
        powered_maintain_wifi();

    unsigned long now = millis();
#ifdef TCP_ENABLED
    s_wifi_radio.loop();
    // Leitura frequente mantém value/label frescos para o publish do rádio
    if (now - s_last_read_ms >= POWERED_READ_INTERVAL_MS)
    {
        s_last_read_ms = now;
        read_acquiry(false);
        dashboard_sync();
    }
#else
    unsigned long period = (unsigned long)s_interval_s * 1000UL;
    if (period < POWERED_SEND_MIN_INTERVAL_MS)
        period = POWERED_SEND_MIN_INTERVAL_MS;
    if (now - s_last_send_ms >= period)
    {
        s_last_send_ms = now;
        read_acquiry(false);
        dashboard_sync();
        // Mesma sequência do ciclo de bateria: pair request + dados (broadcast)
        delay(200);
        espnow_send_pair_request();
        delay(100);
        if (s_read_valid)
        {
            s_ack_received = false;
            espnow_send_data();
            s_status.last_send_ms = millis();
        }
    }
#endif

    // LED: aceso (LOW) com WiFi; blink sem conexão
    if (WiFi.status() == WL_CONNECTED)
        digitalWrite(LED_PIN, LOW);
    else if (now - s_last_led_ms >= POWERED_LED_BLINK_MS)
    {
        s_last_led_ms = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}

void loop(void)
{
    // Dual Power: Powered Mode não usa a máquina de estados de deep sleep
    if (s_power_mode == POWERED_MODE)
    {
        loop_powered();
        return;
    }
    console.loop();
#ifdef TCP_ENABLED
    s_wifi_radio.loop();
#endif
    static bool s_first_loop = true;
    if (s_first_loop)
    {
        console.printf("[%s] Aguardando tecla: %s\n", TAG, "h for help");
        s_first_loop = false;
    }
    delay(50); // tick curto: todas as transições são por millis() (STATE_WAIT_ACK não pode busy-spin)
    if ((Serial.available() > 0 || console.telnet_available() > 0 || console.telnet_connected()) && !s_suspend)
    {
        delay(1);
        while (Serial.available())
            Serial.read();
        while (console.telnet_available())
            console.telnet_read();
        s_suspend = true;
        s_suspend_from_telnet = console.telnet_connected();
        console.printf("[%s] Suspenso - tecla pressionada%s\n", TAG, s_suspend_from_telnet ? " (telnet)" : "");
    }
    if (s_suspend)
    {
        if (s_suspend_from_telnet && !console.telnet_connected())
        {
            console.printf("[%s] Telnet desconectado, restart\n", TAG);
            delay(100);
            ESP.restart();
        }
        static bool menu_printed = false;
        if (!menu_printed)
        {
            console.printf("\nComandos: s=status, i=intervalo, r=restart, p=parear, h=ajuda\n> ");
            menu_printed = true;
        }
        int c = Serial.read();
        if (c == -1)
            c = console.telnet_read();
        if (c != -1)
        {
            menu_printed = false;
            console.printf("\n");
            run_console_command((char)c);
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
    case STATE_WAIT_RADIO:
        if (now - s_state_start >= ESPNOW_STARTUP_WAIT_MS)
        {
            console.printf("[%s] Fim da espera de inicializacao ESP-NOW\n", TAG);
            s_state = STATE_INIT;
        }
        break;

    case STATE_INIT:
#ifdef TCP_ENABLED
        s_state_start = now;
        s_pair_attempts = 1;
        s_read_attempts = 0;
        s_state = s_wifi_radio.is_paired() ? STATE_READ_SENSOR : STATE_WAIT_PAIR;
#else
        console.printf("[%s] Sending pair request\n", TAG);
        espnow_send_pair_request();
        s_pair_attempts = 1;
        s_state_start = now;
        s_read_attempts = 0;
        if (s_has_gateway)
            s_state = STATE_READ_SENSOR;
        else
            s_state = STATE_WAIT_PAIR;
#endif
        break;

    case STATE_READ_SENSOR:
        read_acquiry();
        if (s_read_valid)
        {
            s_state = STATE_SEND_DATA;
        }
        else
        {
            s_read_attempts++;
            if (s_read_attempts >= MAX_READ_RETRIES)
            {
                console.printf("[%s] Leitura invalida apos %d tentativas\n", TAG, s_read_attempts);
                s_data_retries = 0;
                s_state = STATE_SLEEP;
            }
            else
            {
                console.printf("[%s] Retentando leitura (%d/%d)...\n", TAG, s_read_attempts, MAX_READ_RETRIES);
                s_state_start = now;
                s_state = STATE_READ_SENSOR;
            }
        }
        break;

    case STATE_WAIT_PAIR:
#ifdef TCP_ENABLED
        if (s_wifi_radio.is_paired())
        {
            s_state = STATE_READ_SENSOR;
        }
        else if (now - s_state_start > PAIR_RETRY_INTERVAL_MS)
        {
            s_pair_attempts++;
            if (s_pair_attempts >= ESPNOW_MAX_PAIR_ATTEMPTS)
            {
                console.printf("[%s] Hub registration failed after %d attempts\n", TAG, s_pair_attempts);
                s_state = STATE_SLEEP;
            }
            else
            {
                console.printf("[%s] Waiting for hub registration (%d/%d)\n", TAG, s_pair_attempts, ESPNOW_MAX_PAIR_ATTEMPTS);
                s_state_start = now;
            }
        }
#else
        if (s_has_gateway)
        {
            s_state = STATE_READ_SENSOR;
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
#endif
        break;

    case STATE_SEND_DATA:
#ifdef TCP_ENABLED
        if (!s_read_valid)
        {
            console.printf("[%s] Leitura invalida, pulando envio\n", TAG);
            s_data_retries = 0;
            s_state = STATE_SLEEP;
            break;
        }
        digitalWrite(LED_PIN, LOW);
        send_state_payload();
        digitalWrite(LED_PIN, HIGH);
#else
        delay(200);
        espnow_send_pair_request();
        delay(100);
        if (!s_read_valid)
        {
            console.printf("[%s] Leitura invalida, pulando envio\n", TAG);
            s_data_retries = 0;
            s_state = STATE_SLEEP;
            break;
        }
        digitalWrite(LED_PIN, LOW);
        s_ack_received = false;
        espnow_send_data();
        delay(LED_BLINK_SEND_MS);
        digitalWrite(LED_PIN, HIGH);
#endif
        s_data_retries = 0;
        s_state_start = now;
        s_state = STATE_WAIT_ACK;
        break;

    case STATE_WAIT_ACK:
        if (s_ack_received)
        {
            console.printf("[%s] ACK recebido\n", TAG);
            s_data_retries = 0;
            s_state = STATE_SLEEP;
        }
        else if (now - s_state_start > ACK_TIMEOUT_MS)
        {
            s_data_retries++;
            if (s_data_retries >= MAX_DATA_RETRIES)
            {
                console.printf("[%s] Sem ACK apos %d tentativas\n", TAG, s_data_retries);
                s_state = STATE_SLEEP;
            }
            else
            {
                console.printf("[%s] Reenviando dados (tentativa %d/%d)\n", TAG, s_data_retries, MAX_DATA_RETRIES);
                read_acquiry();
                if (!s_read_valid)
                {
                    console.printf("[%s] Leitura invalida no reenvio\n", TAG);
                    s_state = STATE_SLEEP;
                    break;
                }
#ifdef TCP_ENABLED
                send_state_payload();
#else
                s_ack_received = false;
                espnow_send_data();
#endif
                s_state_start = now;
            }
        }
        break;

    case STATE_SLEEP:
        if (s_read_attempts >= MAX_READ_RETRIES || s_data_retries >= MAX_DATA_RETRIES)
            do_deep_sleep(DEEP_SLEEP_RETRY_INTERVAL);
        else
            do_deep_sleep(s_interval_s);
        break;
    }
}