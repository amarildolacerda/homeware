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
#include "console.h"
#include "pages.h"

static const char *TAG = "repeater";

static uint8_t s_gateway_mac[6];
// Broadcast address used for all gateway-bound traffic. ESP8266→ESP32 ESP-NOW
// unicast is dropped by the radio (see homeware AGENTS.md rule 18), so the
// repeater must reach the ESP32 gateway via broadcast.
static uint8_t s_bcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_gateway_configured = false;
static unsigned long s_start_time = 0;
static unsigned long s_last_activity = 0;
static bool s_has_activity = false;
static int s_forwarded = 0;
static int s_received = 0;
static bool s_monitor = false;
static unsigned long s_last_gateway_comm = 0; // last successful comm with gateway
static char s_device_name[32] = DEVICE_NAME;
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;

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
static bool wifi_setup(bool force_portal);

extern "C" void espnow_send_cb(uint8_t *mac, uint8_t status)
{
    (void)mac;
    s_espnow_tx_count++;
    if (status != 0)
        console.printf("[%s] Send fail to %02X:%02X:%02X:%02X:%02X:%02X status=%d\n",
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
    console.printf("[%s] New client: %s\n", TAG, mac_str);
    s_client_count++;
}

extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len)
{
    if (!data || len < 1) return;

    s_espnow_rx_count++;
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
            console.printf("[%s] Gateway discovered: %s\n", TAG, mac_str);
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
                console.printf("[%s] GW→CLI seq=%u %s → %s\n", TAG, sequence, m1, m2);
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
                console.printf("[%s] GW→BCAST seq=%u (%d clients)\n", TAG, sequence, s_client_count);
        }
    }
    else if (s_gateway_configured)
    {
        /* From client → forward to gateway.
           Must be broadcast: ESP8266→ESP32 unicast is dropped by the radio
           (AGENTS.md rule 18). The gateway processes it by the sensor_mac in
           the frame header, so targeting the gateway MAC is unnecessary. */
        cache_sequence(sequence, mac);
        esp_now_send(s_bcast_addr, data, len);
        s_forwarded++;
        s_last_gateway_comm = millis();
        if (s_monitor)
        {
            char m1[18], m2[18];
            mac_to_str(mac, m1, sizeof(m1));
            mac_to_str(s_gateway_mac, m2, sizeof(m2));
            console.printf("[%s] CLI→GW seq=%u %s → %s\n", TAG, sequence, m1, m2);
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
    console.begin();
    s_start_time = millis();
    console.printf("\n============================================\n");
    console.printf("  ESP-NOW Repeater " FW_VERSION "\n");
    console.printf("============================================\n");
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
    console.printf("[%s] Gateway MAC loaded: %s\n", TAG, mac_str);
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
    console.printf("[%s] Gateway MAC saved: %s\n", TAG, mac_str);
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

static void save_device_name(const char *name)
{
    EEPROM.begin(EEPROM_SIZE);
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

enum WifiState { WIFI_IDLE, WIFI_RECONNECTING };
static WifiState s_wifi_state = WIFI_IDLE;
static unsigned long s_wifi_reconnect_deadline = 0;
static unsigned long s_last_config_attempt = 0;
static unsigned long s_last_reconnect_attempt = 0;

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

static bool wifi_setup(bool force_portal = false)
{
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.mode(WIFI_STA);

#ifdef STATIC_WIFI
    if (strlen(WIFI_SSID) > 0)
    {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        console.printf("[%s] WiFi: connecting to %s...\n", TAG, WIFI_SSID);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            retries++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            console.printf("[%s] WiFi: %s IP: %s\n", TAG, WIFI_SSID, WiFi.localIP().toString().c_str());
            return true;
        }
        console.printf("[%s] WiFi: connection failed, continuing without WiFi\n", TAG);
    }
    else
    {
        console.printf("[%s] No WiFi SSID configured, continuing without WiFi\n", TAG);
    }
    return true;
#else

    // Load any previously saved gateway MAC (sets s_gateway_configured when present).
    load_gateway_mac();

    if (!force_portal)
    {
        WiFiManager wm;
        wm.setTimeout(180);
        // autoConnect() uses WiFiManager's persisted credentials; it does NOT
        // require a gateway MAC. Do not gate on WiFi.SSID() here (it is empty
        // before autoConnect runs), otherwise the device always falls into the
        // config portal even when valid WiFi credentials are already saved.
        if (wm.autoConnect())
        {
            console.printf("[%s] WiFi: %s IP: %s\n", TAG, WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            return true;
        }
        console.printf("[%s] WiFi autoconnect failed\n", TAG);
    }

    console.printf("[%s] Starting config portal (SSID: %s)...\n", TAG, WIFI_CONFIG_PORTAL_SSID);
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
                console.printf("[%s] Invalid gateway MAC in config portal\n", TAG);
            }
        }
        console.printf("[%s] WiFi: %s IP: %s\n", TAG, WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        return true;
    }

    console.printf("[%s] Config portal timeout, continuing without WiFi\n", TAG);
    return true;
#endif
}

static bool init_espnow(void)
{
    if (esp_now_init() != 0)
    {
        console.printf("[%s] ESP-NOW init failed\n", TAG);
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
            console.printf("[%s] Failed to add gateway peer %s: %d\n", TAG, mac_str, ret);
        }
    }

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_add_peer(broadcast_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);

    console.printf("[%s] ESP-NOW initialized\n", TAG);
    return true;
}

static void send_gw_discover(void)
{
    espnow_gw_discover_t disc = { .msg_type = ESPNOW_MSG_GW_DISCOVER };
    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(broadcast_mac, (uint8_t *)&disc, sizeof(disc));
    console.printf("[%s] GW_DISCOVER sent\n", TAG);
}

static void send_repeater_status(void)
{
    if (!s_gateway_configured) return;

    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t) + 4 + 2];
    memset(buf, 0, sizeof(buf));

    espnow_header_t *hdr = (espnow_header_t *)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_REPEATER_STATUS;
    hdr->sequence = 0;
    WiFi.macAddress(hdr->sensor_mac);
    hdr->sensor_type = SENSOR_TYPE_REPEATER;
    hdr->battery_pct = 0;
    hdr->rssi = (int16_t)WiFi.RSSI();

    payload_repeater_status_t *pl = (payload_repeater_status_t *)hdr->payload;
    pl->received = s_received;
    pl->forwarded = s_forwarded;
    pl->client_count = s_client_count;
    pl->channel = WiFi.channel();
    pl->rssi = WiFi.RSSI();
    pl->uptime_s = (uint32_t)((millis() - s_start_time) / 1000);
    pl->free_heap = ESP.getFreeHeap();
    pl->ack_failures = 0;

    // Gateway convention: append IP(4) + free_heap(2) at the payload tail.
    IPAddress lip = WiFi.localIP();
    uint8_t *ip_ptr = hdr->payload + sizeof(payload_repeater_status_t);
    ip_ptr[0] = lip[0]; ip_ptr[1] = lip[1]; ip_ptr[2] = lip[2]; ip_ptr[3] = lip[3];
    uint16_t fh = ESP.getFreeHeap();
    uint8_t *fh_ptr = hdr->payload + sizeof(payload_repeater_status_t) + 4;
    fh_ptr[0] = fh & 0xFF; fh_ptr[1] = (fh >> 8) & 0xFF;

    hdr->payload_len = sizeof(payload_repeater_status_t) + 4 + 2;

    // Broadcast: ESP8266→ESP32 ESP-NOW unicast is dropped by the radio
    // (AGENTS.md rule 18). The gateway ACKs this via broadcast.
    esp_now_send(s_bcast_addr, buf, sizeof(buf));
}

static ESP8266WebServer s_server(DASHBOARD_PORT);

static void handle_api_settings(void)
{
    if (s_server.method() == HTTP_GET)
    {
        JsonDocument doc;
        doc["device_name"] = s_device_name;
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    }
    else if (s_server.method() == HTTP_POST)
    {
        JsonDocument doc;
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
    serve_pgm_page(PAGE_DASHBOARD);
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
    doc["tx_count"] = s_espnow_tx_count;
    doc["rx_count"] = s_espnow_rx_count;
    doc["last_activity_s"] = s_has_activity ? (int)((millis() - s_last_activity) / 1000) : -1;
    doc["ip"] = WiFi.localIP().toString();
    doc["fw_version"] = FW_VERSION;
    doc["device_id"] = String("esp8266_") + String(ESP.getChipId(), HEX);
    doc["device_name"] = s_device_name;
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
    console.printf("\n--- Repeater Status ---\n");
    console.printf("  Uptime:    %lums\n", up);
    console.printf("  Received:  %d\n", s_received);
    console.printf("  Forwarded: %d\n", s_forwarded);
    console.printf("  Clients:   %d\n", s_client_count);
    console.printf("  Monitor:   %s\n", s_monitor ? "ativo" : "desligado");
    console.printf("  Channel:   %d\n", WiFi.channel());
    if (s_gateway_configured)
    {
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("  Gateway:   %s\n", mac_str);
    }
    for (int i = 0; i < s_client_count; i++)
    {
        char mac_str[18];
        mac_to_str(s_client_peers[i], mac_str, sizeof(mac_str));
        console.printf("  Client %d:  %s\n", i, mac_str);
    }
    console.printf("------------------------\n\n");
}

static void handle_serial(char c)
{
    switch (c)
    {
    case 's':
    case 'S':
        print_status();
        break;
    case 'm':
    case 'M':
        s_monitor = !s_monitor;
        console.printf("[%s] Monitor %s\n", TAG, s_monitor ? "ativado" : "desativado");
        break;
    case 'c':
    case 'C':
        s_received = 0;
        s_forwarded = 0;
        console.printf("[%s] Estatisticas zeradas\n", TAG);
        break;
    case 'd':
    case 'D':
        console.printf("[%s] Enviando GW_DISCOVER...\n", TAG);
        send_gw_discover();
        break;
    case 'w':
    case 'W':
#ifdef STATIC_WIFI
        console.printf("[%s] WiFi configurado estaticamente via build flags\n", TAG);
#else
        console.printf("[%s] Abrindo portal de configuracao...\n", TAG);
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
        console.printf("\n--- Comandos ---\n");
        console.printf("  s    - status\n");
        console.printf("  d    - descobrir gateway (GW_DISCOVER)\n");
        console.printf("  w    - reconfigurar WiFi + gateway MAC\n");
        console.printf("  m    - monitor (liga/desliga log de pacotes)\n");
        console.printf("  c    - zerar contadores\n");
        console.printf("  r    - reset\n");
        console.printf("  h/?  - ajuda\n");
        console.printf("----------------\n\n");
        break;
    }
}

void setup(void)
{
    init_serial();
#ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif

    load_device_name();

#ifdef STATIC_WIFI
    if (strlen(GATEWAY_MAC) > 0 && parse_mac(GATEWAY_MAC, s_gateway_mac))
    {
        s_gateway_configured = true;
        save_gateway_mac();
        char mac_str[18];
        mac_to_str(s_gateway_mac, mac_str, sizeof(mac_str));
        console.printf("[%s] Gateway MAC from config: %s\n", TAG, mac_str);
    }
#endif

    wifi_setup(false);

    init_espnow();

    bool has_wifi = (WiFi.status() == WL_CONNECTED);
    if (has_wifi)
    {
        s_server.on("/", handle_root);
        s_server.on("/docs", []() { serve_pgm_page(PAGE_DOCS); });
        s_server.on("/api/status", handle_api_status);
        s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
        s_server.on("/api/restart", HTTP_POST, []() {
            s_server.send(200, "application/json", "{\"ok\":true}");
            delay(500);
            ESP.restart();
        });
        s_server.on("/api/wifi", HTTP_GET, []() {
            DynamicJsonDocument doc(256);
            doc["ssid"] = WiFi.SSID();
            doc["ip"] = WiFi.localIP().toString();
            doc["rssi"] = WiFi.RSSI();
            doc["device_name"] = s_device_name;
            String json;
            serializeJson(doc, json);
            s_server.send(200, "application/json", json);
        });
        s_server.on("/api/wifi", HTTP_POST, []() {
            s_server.send(200, "application/json", "{\"ok\":true}");
        });
        s_server.begin();
        console.printf("\n  Dashboard: http://%s:%d\n", WiFi.localIP().toString().c_str(), DASHBOARD_PORT);
    }
    else
    {
        console.printf("\n  WiFi: nao conectado (somente ESP-NOW)\n");
    }

    if (s_gateway_configured)
        console.printf("  Repeater pronto! Pressione 'h' para ajuda\n\n");
    else
        console.printf("  Repeater sem gateway! Aguardando GW_DISCOVER... (use 'd' para buscar)\n\n");
}

void loop(void)
{
    console.loop();
    if (Serial.available() > 0)
        handle_serial(Serial.read());
    int tc = console.telnet_read();
    if (tc >= 0)
        handle_serial((char)tc);
    if (WiFi.status() == WL_CONNECTED)
        s_server.handleClient();
    maintain_wifi_connection();

#ifdef LED_PIN
    /* Slow heartbeat LED */
    static unsigned long last_blink = 0;
    unsigned long now = millis();
    if (now - last_blink > 2000)
    {
        last_blink = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
#else
    unsigned long now = millis();
#endif

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
        console.printf("[%s] Gateway timeout (no comm for 1min), clearing config\n", TAG);
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

    /* Send status to gateway every 30s */
    static unsigned long last_status = 0;
    if (s_gateway_configured && (now - last_status > 30000))
    {
        last_status = now;
        send_repeater_status();
    }

    yield();
}
