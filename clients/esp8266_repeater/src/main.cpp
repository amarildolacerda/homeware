#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#ifndef STATIC_WIFI
#include <WiFiManager.h>
#endif
#include <EEPROM.h>
#include <espnow.h>
#include "config.h"
#include "espnow_protocol.h"
#include "pages.h"

static const char *TAG = "repeater";

static uint8_t s_gateway_mac[6];
static bool s_gateway_configured = false;
static unsigned long s_start_time = 0;
static unsigned long s_last_activity = 0;
static bool s_has_activity = false;
static int s_forwarded = 0;
static int s_received = 0;
static bool s_monitor = false;
static unsigned long s_last_gateway_comm = 0; // last successful comm with gateway

typedef struct {
    uint16_t sequence;
    uint8_t mac[6];
    unsigned long time;
} seq_entry_t;

static seq_entry_t s_seq_cache[SEQ_CACHE_SIZE];
static uint8_t s_cache_idx = 0;

/* Peers known to be behind this repeater (learned dynamically) */
static uint8_t s_client_peers[MAX_PEERS][6];
static int s_client_count = 0;

static void save_gateway_mac(void);
static bool load_gateway_mac(void);

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    if (status != 0)
        Serial.printf("[%s] Send fail to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
                      TAG, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], status);
}

static void cache_sequence(uint16_t seq, const uint8_t *mac)
{
    uint8_t i = s_cache_idx % SEQ_CACHE_SIZE;
    s_seq_cache[i].sequence = seq;
    memcpy(s_seq_cache[i].mac, mac, 6);
    s_seq_cache[i].time = millis();
    s_cache_idx++;
}

static bool lookup_sequence(uint16_t seq, uint8_t *mac_out)
{
    for (int i = 0; i < SEQ_CACHE_SIZE; i++)
    {
        int idx = (s_cache_idx - 1 - i + SEQ_CACHE_SIZE * 2) % SEQ_CACHE_SIZE;
        if (s_seq_cache[idx].sequence == seq && s_seq_cache[idx].time > 0)
        {
            memcpy(mac_out, s_seq_cache[idx].mac, 6);
            s_seq_cache[idx].time = 0; /* consume */
            return true;
        }
    }
    return false;
}

static bool is_client_known(const uint8_t *mac)
{
    for (int i = 0; i < s_client_count; i++)
    {
        if (memcmp(s_client_peers[i], mac, 6) == 0)
            return true;
    }
    return false;
}

static void learn_client(const uint8_t *mac)
{
    if (is_client_known(mac)) return;
    if (s_client_count >= MAX_PEERS) return;
    memcpy(s_client_peers[s_client_count], mac, 6);
    esp_now_add_peer((uint8_t *)mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));
    Serial.printf("[%s] New client: %s\n", TAG, mac_str);
    s_client_count++;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    if (!data || len < 1) return;

    s_last_activity = millis();
    s_has_activity = true;
    s_received++;

    /* Learn unknown peers as clients (except gateway) */
    if (!s_gateway_configured || !mac_equal(mac, s_gateway_mac))
    {
        if (!is_client_known(mac))
            learn_client(mac);
    }

    uint8_t msg_type = data[0];
    uint16_t sequence = 0;

    /* Extract sequence from known message types */
    if (msg_type == ESPNOW_MSG_SENSOR_DATA && len >= 3)
    {
        sequence = data[1] | ((uint16_t)data[2] << 8);
    }
    else if (msg_type == ESPNOW_MSG_HEARTBEAT && len >= 3)
    {
        sequence = data[1] | ((uint16_t)data[2] << 8);
    }
    else if (msg_type == ESPNOW_MSG_PAIR_REQUEST && len >= 3)
    {
        espnow_pair_request_t *req = (espnow_pair_request_t *)data;
        sequence = req->sequence;
    }
    else if (msg_type == ESPNOW_MSG_ACK && len >= 3)
    {
        espnow_ack_t *ack = (espnow_ack_t *)data;
        sequence = ack->sequence;
    }
    else if (msg_type == ESPNOW_MSG_PAIR_RESPONSE && len >= 3)
    {
        espnow_pair_response_t *resp = (espnow_pair_response_t *)data;
        sequence = resp->sequence;
    }
    else if (msg_type == ESPNOW_MSG_GW_ANNOUNCE && len >= sizeof(espnow_gw_announce_t))
    {
        espnow_gw_announce_t *ann = (espnow_gw_announce_t *)data;
        if (!s_gateway_configured || !mac_equal(ann->gateway_mac, s_gateway_mac))
        {
            mac_copy(s_gateway_mac, ann->gateway_mac);
            s_gateway_configured = true;
            save_gateway_mac();
            esp_now_del_peer(s_gateway_mac);
            esp_now_add_peer(s_gateway_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            Serial.printf("[%s] Gateway discovered: %s\n", TAG, mac_str);
        }
        return;
    }

    if (s_gateway_configured && mac_equal(mac, s_gateway_mac))
    {
        /* From gateway → forward to client */
        s_last_gateway_comm = millis();
        uint8_t client_mac[6];
        if (lookup_sequence(sequence, client_mac))
        {
            esp_now_send(client_mac, data, len);
            s_forwarded++;
            if (s_monitor)
            {
                char m1[18], m2[18];
                mac_to_str(mac, m1, sizeof(m1));
                mac_to_str(client_mac, m2, sizeof(m2));
                Serial.printf("[%s] GW→CLI seq=%u %s → %s\n", TAG, sequence, m1, m2);
            }
        }
        else
        {
            for (int i = 0; i < s_client_count; i++)
            {
                esp_now_send(s_client_peers[i], data, len);
                s_forwarded++;
            }
            if (s_monitor)
                Serial.printf("[%s] GW→BCAST seq=%u (%d clients)\n", TAG, sequence, s_client_count);
        }
    }
    else if (s_gateway_configured)
    {
        /* From client → forward to gateway */
        cache_sequence(sequence, mac);
        esp_now_send(s_gateway_mac, data, len);
        s_forwarded++;
        s_last_gateway_comm = millis();
        if (s_monitor)
        {
            char m1[18], m2[18];
            mac_to_str(mac, m1, sizeof(m1));
            mac_to_str(s_gateway_mac, m2, sizeof(m2));
            Serial.printf("[%s] CLI→GW seq=%u %s → %s\n", TAG, sequence, m1, m2);
        }
    }
}

static bool parse_mac(const char *str, uint8_t *mac)
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

static void init_serial(void)
{
    Serial.begin(115200);
    delay(1000);
    s_start_time = millis();
    Serial.printf("\n============================================\n");
    Serial.printf("  ESP-NOW Repeater " FW_VERSION "\n");
    Serial.printf("============================================\n");
}

static bool load_gateway_mac(void)
{
    EEPROM.begin(EEPROM_SIZE);
    uint8_t magic = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR);
    if (magic != EEPROM_MAGIC) { EEPROM.end(); return false; }
    for (int i = 0; i < 6; i++)
        s_gateway_mac[i] = EEPROM.read(EEPROM_GATEWAY_MAC_ADDR + 1 + i);
    EEPROM.end();
    s_gateway_configured = true;
    char mac_str[18];
    mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
    Serial.printf("[%s] Gateway MAC loaded: %s\n", TAG, mac_str);
    return true;
}

static void save_gateway_mac(void)
{
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, EEPROM_MAGIC);
    for (int i = 0; i < 6; i++)
        EEPROM.write(EEPROM_GATEWAY_MAC_ADDR + 1 + i, s_gateway_mac[i]);
    EEPROM.commit();
    EEPROM.end();
    char mac_str[18];
    mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
    Serial.printf("[%s] Gateway MAC saved: %s\n", TAG, mac_str);
}

static bool wifi_setup(bool force_portal = false)
{
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.mode(WIFI_STA);

#ifdef STATIC_WIFI
    if (strlen(WIFI_SSID) > 0)
    {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        Serial.printf("[%s] WiFi: connecting to %s...\n", TAG, WIFI_SSID);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            retries++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[%s] WiFi: %s IP: %s\n", TAG, WIFI_SSID, WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.printf("[%s] WiFi: connection failed, continuing without WiFi\n", TAG);
    }
    else
    {
        Serial.printf("[%s] No WiFi SSID configured, continuing without WiFi\n", TAG);
    }
    return true;
#else

    if (!force_portal && load_gateway_mac() && WiFi.SSID() != "")
    {
        WiFiManager wm;
        wm.setTimeout(180);
        if (wm.autoConnect())
        {
            Serial.printf("[%s] WiFi: %s IP: %s\n", TAG, WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.printf("[%s] WiFi autoconnect failed\n", TAG);
    }

    Serial.printf("[%s] Starting config portal (SSID: %s)...\n", TAG, WIFI_CONFIG_PORTAL_SSID);
    WiFiManager wm;
    wm.setConfigPortalTimeout(300);
    wm.setConnectTimeout(20);

    char gw_mac_str[18] = "";
    WiFiManagerParameter custom_gw("gateway_mac", "Gateway MAC (XX:XX:XX:XX:XX:XX)", gw_mac_str, 18);
    wm.addParameter(&custom_gw);

    if (wm.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS))
    {
        if (strlen(custom_gw.getValue()) > 0)
        {
            if (parse_mac(custom_gw.getValue(), s_gateway_mac))
            {
                s_gateway_configured = true;
                save_gateway_mac();
            }
            else
            {
                Serial.printf("[%s] Invalid gateway MAC in config portal\n", TAG);
            }
        }
        Serial.printf("[%s] WiFi: %s IP: %s\n", TAG, WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.printf("[%s] Config portal timeout, continuing without WiFi\n", TAG);
    return true;
#endif
}

static bool init_espnow(void)
{
    if (esp_now_init() != 0)
    {
        Serial.printf("[%s] ESP-NOW init failed\n", TAG);
        return false;
    }
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_register_send_cb(espnow_send_cb);
    esp_now_register_recv_cb(espnow_recv_cb);

    /* Add gateway as peer */
    if (s_gateway_configured)
    {
        int ret = esp_now_add_peer(s_gateway_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
        if (ret != 0)
        {
            char mac_str[18];
            mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
            Serial.printf("[%s] Failed to add gateway peer %s: %d\n", TAG, mac_str, ret);
        }
    }

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_add_peer(broadcast_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);

    Serial.printf("[%s] ESP-NOW initialized\n", TAG);
    return true;
}

static void send_gw_discover(void)
{
    espnow_gw_discover_t disc = { .msg_type = ESPNOW_MSG_GW_DISCOVER };
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcast_mac, (uint8_t *)&disc, sizeof(disc));
    Serial.printf("[%s] GW_DISCOVER sent\n", TAG);
}

static ESP8266WebServer s_server(DASHBOARD_PORT);

static void handle_root(void)
{
    s_server.send(200, "text/html", FPSTR(PAGE_DASHBOARD));
}

static void handle_api_status(void)
{
    JsonDocument doc;
    char gw_str[18] = "-";
    if (s_gateway_configured)
        mac_to_str(s_gateway_mac, gw_str, sizeof(gw_str));
    doc["gateway"] = gw_str;
    doc["uptime_s"] = (millis() - s_start_time) / 1000;
    doc["channel"] = WiFi.channel();
    doc["rssi"] = WiFi.RSSI();
    doc["received"] = s_received;
    doc["forwarded"] = s_forwarded;
    doc["clients"] = s_client_count;
    doc["last_activity_s"] = s_has_activity ? (int)((millis() - s_last_activity) / 1000) : -1;
    JsonArray arr = doc["client_list"].to<JsonArray>();
    for (int i = 0; i < s_client_count; i++)
    {
        char mac_str[18];
        mac_to_str(s_client_peers[i], mac_str, sizeof(mac_str));
        arr.add(mac_str);
    }
    String json;
    serializeJson(doc, json);
    s_server.send(200, "application/json", json);
}

static void print_status(void)
{
    unsigned long up = (millis() - s_start_time) / 1000;
    Serial.printf("\n--- Repeater Status ---\n");
    Serial.printf("  Uptime:    %lums\n", up);
    Serial.printf("  Received:  %d\n", s_received);
    Serial.printf("  Forwarded: %d\n", s_forwarded);
    Serial.printf("  Clients:   %d\n", s_client_count);
    Serial.printf("  Monitor:   %s\n", s_monitor ? "ativo" : "desligado");
    Serial.printf("  Channel:   %d\n", WiFi.channel());
    if (s_gateway_configured)
    {
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        Serial.printf("  Gateway:   %s\n", mac_str);
    }
    for (int i = 0; i < s_client_count; i++)
    {
        char mac_str[18];
        mac_to_str(s_client_peers[i], mac_str, sizeof(mac_str));
        Serial.printf("  Client %d:  %s\n", i, mac_str);
    }
    Serial.printf("------------------------\n\n");
}

static void handle_serial(void)
{
    if (Serial.available() <= 0) return;
    char c = Serial.read();
    switch (c)
    {
    case 's':
    case 'S':
        print_status();
        break;
    case 'm':
    case 'M':
        s_monitor = !s_monitor;
        Serial.printf("[%s] Monitor %s\n", TAG, s_monitor ? "ativado" : "desativado");
        break;
    case 'c':
    case 'C':
        s_received = 0;
        s_forwarded = 0;
        Serial.printf("[%s] Estatisticas zeradas\n", TAG);
        break;
    case 'd':
    case 'D':
        Serial.printf("[%s] Enviando GW_DISCOVER...\n", TAG);
        send_gw_discover();
        break;
    case 'w':
    case 'W':
#ifdef STATIC_WIFI
        Serial.printf("[%s] WiFi configurado estaticamente via build flags\n", TAG);
#else
        Serial.printf("[%s] Abrindo portal de configuracao...\n", TAG);
        wifi_setup(true);
#endif
        break;
    case 'r':
    case 'R':
        ESP.restart();
        break;
    case 'h':
    case 'H':
    case '?':
        Serial.printf("\n--- Comandos ---\n");
        Serial.printf("  s    - status\n");
        Serial.printf("  d    - descobrir gateway (GW_DISCOVER)\n");
        Serial.printf("  w    - reconfigurar WiFi + gateway MAC\n");
        Serial.printf("  m    - monitor (liga/desliga log de pacotes)\n");
        Serial.printf("  c    - zerar contadores\n");
        Serial.printf("  r    - reset\n");
        Serial.printf("  h/?  - ajuda\n");
        Serial.printf("----------------\n\n");
        break;
    }
}

void setup(void)
{
    init_serial();
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

#ifdef STATIC_WIFI
    if (strlen(GATEWAY_MAC) > 0 && parse_mac(GATEWAY_MAC, s_gateway_mac))
    {
        s_gateway_configured = true;
        save_gateway_mac();
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        Serial.printf("[%s] Gateway MAC from config: %s\n", TAG, mac_str);
    }
#endif

    wifi_setup(false);

    init_espnow();

    bool has_wifi = (WiFi.status() == WL_CONNECTED);
    if (has_wifi)
    {
        s_server.on("/", handle_root);
        s_server.on("/api/status", handle_api_status);
        s_server.begin();
        Serial.printf("\n  Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
    }
    else
    {
        Serial.printf("\n  WiFi: nao conectado (somente ESP-NOW)\n");
    }

    if (s_gateway_configured)
        Serial.printf("  Repeater pronto! Pressione 'h' para ajuda\n\n");
    else
        Serial.printf("  Repeater sem gateway! Aguardando GW_DISCOVER... (use 'd' para buscar)\n\n");
}

void loop(void)
{
    handle_serial();
    if (WiFi.status() == WL_CONNECTED)
        s_server.handleClient();

    /* Slow heartbeat LED */
    static unsigned long last_blink = 0;
    unsigned long now = millis();
    if (now - last_blink > 2000)
    {
        last_blink = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    /* Re-add gateway periodically if WiFi channel changes */
    static unsigned long last_chk = 0;
    if (now - last_chk > 60000)
    {
        last_chk = now;
        if (s_gateway_configured)
        {
            esp_now_del_peer(s_gateway_mac);
            esp_now_add_peer(s_gateway_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
        }
    }

    /* Gateway timeout: if no communication for 60s, clear config and rediscover */
    if (s_gateway_configured && s_last_gateway_comm > 0 && (now - s_last_gateway_comm > 60000))
    {
        Serial.printf("[%s] Gateway timeout (no comm for 5min), clearing config\n", TAG);
        s_gateway_configured = false;
        memset(s_gateway_mac, 0, sizeof(s_gateway_mac));
        /* Clear from EEPROM */
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, 0);
        EEPROM.commit();
        EEPROM.end();
    }

    /* Gateway discovery: broadcast if no gateway configured */
    static unsigned long last_discover = 0;
    if (!s_gateway_configured && (now - last_discover > 30000))
    {
        last_discover = now;
        send_gw_discover();
    }

    delay(1);
}
