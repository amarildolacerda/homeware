#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#ifndef STATIC_WIFI
#include <WiFiManager.h>
#endif
#include <EEPROM.h>
#include <espnow.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "espnow_protocol.h"
#include "console.h"
#include "pages.h"

static const char *TAG = "repeater";

#define GATEWAY_TIMEOUT_MS 60000

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
static bool s_monitor = true;
static unsigned long s_last_gateway_comm = 0; // last successful comm with gateway
static unsigned long s_last_gateway_ack = 0;  // last ACK from gateway to THIS repeater (uplink proof)
static char s_device_name[32] = DEVICE_NAME;
static uint8_t s_my_mac[6];
static uint32_t s_espnow_tx_count = 0;
static uint32_t s_espnow_rx_count = 0;

typedef struct {
    uint16_t sequence;
    uint8_t mac[6];
    unsigned long time;
} seq_entry_t;

static seq_entry_t s_seq_cache[SEQ_CACHE_SIZE];
static uint8_t s_cache_idx = 0;

/* Forward queue: esp_now_send() must be called from loop(), not from the
   recv callback (ISR context), otherwise sends may be silently dropped
   when the ESP-NOW stack can't process the TX queue fast enough. */
#define FWD_BUF_SIZE 192
#define FWD_QUEUE_SIZE 6

typedef struct {
    uint8_t dest[6];
    uint8_t data[FWD_BUF_SIZE];
    uint8_t len;
    uint16_t sequence;
    uint8_t from[6];
    bool from_gateway;
    bool to_all_clients;
} fwd_entry_t;

static fwd_entry_t s_fwd_queue[FWD_QUEUE_SIZE];
static volatile uint8_t s_fwd_head = 0;
static volatile uint8_t s_fwd_tail = 0;

static bool fwd_push(const uint8_t *dest, const uint8_t *data, uint8_t len,
                     uint16_t seq, const uint8_t *from, bool from_gw,
                     bool to_all)
{
    uint8_t next = (s_fwd_head + 1) % FWD_QUEUE_SIZE;
    if (next == s_fwd_tail) return false;
    fwd_entry_t *e = &s_fwd_queue[s_fwd_head];
    memcpy(e->dest, dest, 6);
    uint8_t clen = len < FWD_BUF_SIZE ? len : FWD_BUF_SIZE;
    memcpy(e->data, data, clen);
    e->len = clen;
    e->sequence = seq;
    memcpy(e->from, from, 6);
    e->from_gateway = from_gw;
    e->to_all_clients = to_all;
    s_fwd_head = next;
    return true;
}

static bool fwd_pop(fwd_entry_t *out)
{
    if (s_fwd_tail == s_fwd_head) return false;
    memcpy(out, &s_fwd_queue[s_fwd_tail], sizeof(fwd_entry_t));
    s_fwd_tail = (s_fwd_tail + 1) % FWD_QUEUE_SIZE;
    return true;
}

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

    /* Learn unknown peers as clients (except gateway and GW_ANNOUNCE) */
    if ((!s_gateway_configured || !mac_equal(mac, s_gateway_mac)) && data[0] != ESPNOW_MSG_GW_ANNOUNCE)
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
        if (!s_gateway_configured || !mac_equal(mac, s_gateway_mac))
        {
            mac_copy(s_gateway_mac, mac);
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

    /* NAK se não temos gateway (SPEC_ESPNOW §13.2) */
    if (msg_type == ESPNOW_MSG_PAIR_REQUEST && len >= sizeof(espnow_pair_request_t))
    {
        if (!s_gateway_configured || (millis() - s_last_gateway_comm > GATEWAY_TIMEOUT_MS))
        {
            espnow_pair_request_t *req = (espnow_pair_request_t *)data;
            espnow_nak_t nak;
            memset(&nak, 0, sizeof(nak));
            nak.msg_type = ESPNOW_MSG_NAK;
            nak.sequence = req->sequence;
            mac_copy(nak.target_mac, req->sensor_mac);
            nak.reason = NAK_REASON_NO_GATEWAY;
            esp_now_send(s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
            console.printf("[%s] NAK sent: no gateway for PAIR_REQUEST from %02X:%02X:%02X:%02X:%02X:%02X\n",
                           TAG, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;  /* não repassa */
        }
        /* Se tem gateway, deixa o fluxo normal de forwarding repassar */
    }

    /* Restart command targeting this repeater */
    if (msg_type == ESPNOW_MSG_RESTART && len >= sizeof(espnow_restart_t))
    {
        espnow_restart_t *rst = (espnow_restart_t *)data;
        if (mac_equal(rst->target_mac, s_my_mac))
        {
            console.printf("[%s] Restart command received, rebooting...\n", TAG);
            /* Enviar último status antes de resetar */
            send_repeater_status();
            delay(50);
            delay(100);
            ESP.restart();
        }
    }

    if (s_gateway_configured && mac_equal(mac, s_gateway_mac))
    {
        /* ACK from gateway to THIS repeater proves the uplink (gateway is
           actually receiving our REPEATER_STATUS / forwarded frames). Use it
           for the reachability feedback instead of downlink-only comm. */
        if (msg_type == ESPNOW_MSG_ACK && len >= sizeof(espnow_ack_t))
        {
            espnow_ack_t *ack = (espnow_ack_t *)data;
            uint8_t my_mac[6];
            WiFi.macAddress(my_mac);
            /* Only count as uplink proof if the ACK actually comes FROM the
               gateway (frame sender == gateway MAC). Other clients (lights)
               also broadcast ACKs for frames we forward, which would give a
               false "gateway reachable" reading. */
            if (mac_equal(mac, s_gateway_mac) && mac_equal(ack->sensor_mac, my_mac))
            {
                s_last_gateway_ack = millis();
                if (s_monitor)
                    console.printf("[%s] ACK do gateway (uplink OK) seq=%u status=%d\n",
                                   TAG, ack->sequence, ack->status);
                return;
            }
        }

        /* From gateway → forward to client (deferred to loop) */
        s_last_gateway_comm = millis();
        uint8_t client_mac[6];
        if (lookup_sequence(sequence, client_mac))
            fwd_push(client_mac, data, len, sequence, mac, true, false);
        else if (s_client_count > 0)
            fwd_push(s_bcast_addr, data, len, sequence, mac, true, true);
    }
    else if (s_gateway_configured)
    {
        /* From client → forward to gateway via broadcast (deferred to loop).
           ESP8266→ESP32 unicast is dropped by the radio (AGENTS.md rule 18). */
        cache_sequence(sequence, mac);
        s_last_gateway_comm = millis();
        fwd_push(s_bcast_addr, data, len, sequence, mac, false, false);
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
    doc["gateway_reachable"] = (s_gateway_configured && s_last_gateway_ack > 0) ?
        (int)((millis() - s_last_gateway_ack) / 1000) < 90 : false;
    doc["gateway_last_comm_s"] = (s_gateway_configured && s_last_gateway_ack > 0) ?
        (int)((millis() - s_last_gateway_ack) / 1000) : -1;
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    doc["fw_version"] = FW_VERSION;
    doc["device_id"] = String("esp8266_") + String(ESP.getChipId(), HEX);
    doc["device_name"] = s_device_name;
    JsonArray arr = doc["client_list"].to<JsonArray>();
    for (int i = 0; i < s_client_count; i++)
    {
        bool dup = false;
        for (int j = 0; j < i; j++)
            if (memcmp(s_client_peers[j], s_client_peers[i], 6) == 0) { dup = true; break; }
        if (dup) continue;
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
    char local_mac[18];
    WiFi.macAddress().toCharArray(local_mac, sizeof(local_mac));
    console.printf("  MAC:       %s\n", local_mac);
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
    case 'l':
    case 'L':
        console.printf("\n--- Clientes (%d) ---\n", s_client_count);
        for (int i = 0; i < s_client_count; i++) {
            char mac_str[18];
            mac_to_str(s_client_peers[i], mac_str, sizeof(mac_str));
            console.printf("  %d: %s\n", i + 1, mac_str);
        }
        console.printf("---------------------\n\n");
        break;
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
        console.printf("  l    - listar clientes\n");
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
    WiFi.macAddress(s_my_mac);

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
        console.printf("  Telnet:    %s:23\n", WiFi.localIP().toString().c_str());

        String ota_host = String("repeater_") + String(ESP.getChipId(), HEX);
        ArduinoOTA.setHostname(ota_host.c_str());
        ArduinoOTA.onStart([]() { console.printf("[%s] OTA update start\n", TAG); });
        ArduinoOTA.onEnd([]() { console.printf("[%s] OTA update end\n", TAG); });
        ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
            console.printf("[%s] OTA progress: %u%%\r", TAG, (p * 100) / t);
        });
        ArduinoOTA.onError([](ota_error_t err) { console.printf("[%s] OTA error: %d\n", TAG, err); });
        ArduinoOTA.begin();
        console.printf("[%s] OTA ready: %s.local\n", TAG, ota_host.c_str());
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

static void process_forward_queue(void)
{
    fwd_entry_t e;
    while (fwd_pop(&e))
    {
        if (e.to_all_clients)
        {
            for (int i = 0; i < s_client_count; i++)
            {
                esp_now_send(s_client_peers[i], e.data, e.len);
                s_forwarded++;
            }
            if (s_monitor && s_client_count > 0)
            {
                char m1[18];
                mac_to_str(e.from, m1, sizeof(m1));
                console.printf("[%s] GW→BCAST seq=%u %s (%d clients)\n",
                               TAG, e.sequence, m1, s_client_count);
            }
        }
        else
        {
            esp_now_send(e.dest, e.data, e.len);
            s_forwarded++;
            if (s_monitor)
            {
                char m1[18], m2[18];
                mac_to_str(e.from, m1, sizeof(m1));
                mac_to_str(e.dest, m2, sizeof(m2));
                console.printf("[%s] %s seq=%u %s → %s\n",
                               TAG, e.from_gateway ? "GW→CLI" : "CLI→GW",
                               e.sequence, m1, m2);
            }
        }
    }
}

void loop(void)
{
    process_forward_queue();
    console.loop();
    if (Serial.available() > 0)
        handle_serial(Serial.read());
    int tc = console.telnet_read();
    if (tc >= 0)
        handle_serial((char)tc);
    if (WiFi.status() == WL_CONNECTED)
    {
        ArduinoOTA.handle();
        s_server.handleClient();
    }
    maintain_wifi_connection();

    unsigned long now = millis();

    /* Gateway reachability feedback (UPLINK).
       s_last_gateway_ack is set only when the gateway sends us an ACK for a
       frame WE sent — i.e. the gateway is actually receiving our ESP-NOW
       frames. Downlink-only comm (hearing the gateway retransmit client
       frames) is NOT enough: ESP-NOW range can be asymmetric, so a repeater
       may hear the gateway but the gateway can't hear the repeater. We signal
       that with a fast-blinking LED + log so the unit can be repositioned.
       We do NOT clear the gateway config here so the operator can watch the
       LED while moving the device. */
    bool gw_reachable = s_gateway_configured && (s_last_gateway_ack > 0) &&
                        (now - s_last_gateway_ack < 90000);
    static bool s_gw_unreach_logged = false;
    if (s_gateway_configured && !gw_reachable && !s_gw_unreach_logged)
    {
        console.printf("[%s] AVISO: gateway NAO recebe este repeater (uplink ESP-NOW ausente ha >90s). Reposicione o repeater.\n", TAG);
        s_gw_unreach_logged = true;
    }
    else if (gw_reachable && s_gw_unreach_logged)
    {
        console.printf("[%s] Gateway recebendo o repeater novamente (uplink OK).\n", TAG);
        s_gw_unreach_logged = false;
    }

#ifdef LED_PIN
    /* LED: slow heartbeat when gateway reachable, fast blink when not */
    static unsigned long last_blink = 0;
    unsigned long blink_period = gw_reachable ? 2000 : 250;
    if (now - last_blink > blink_period)
    {
        last_blink = now;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
#endif

    /* Re-add peers periodically if WiFi channel changes. The broadcast peer
       MUST follow the current WiFi channel too, otherwise every forwarded
       frame and REPEATER_STATUS is sent on a stale channel and the gateway
       (ESP32) never receives it. ESP-NOW peers are bound to a channel. */
    static unsigned long last_chk = 0;
    if (now - last_chk > 60000)
    {
        last_chk = now;
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_del_peer(broadcast_mac);
        esp_now_add_peer(broadcast_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
        if (s_gateway_configured)
        {
            esp_now_del_peer(s_gateway_mac);
            esp_now_add_peer(s_gateway_mac, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
        }
    }

    /* Gateway timeout: if no communication for 60s, notify clients (§13.3) and
       clear config so the repeater rediscovers the gateway. */
    static unsigned long s_last_gateway_check = 0;
    if (now - s_last_gateway_check > 5000)
    {
        s_last_gateway_check = now;
        if (s_gateway_configured && s_last_gateway_comm > 0 && (now - s_last_gateway_comm > GATEWAY_TIMEOUT_MS))
        {
            console.printf("[%s] Gateway lost! Notifying clients...\n", TAG);
            espnow_nak_t nak;
            memset(&nak, 0, sizeof(nak));
            nak.msg_type = ESPNOW_MSG_NAK;
            nak.sequence = 0;
            memset(nak.target_mac, 0xFF, 6);  /* broadcast */
            nak.reason = NAK_REASON_GATEWAY_LOST;
            esp_now_send(s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
            s_gateway_configured = false;
            memset(s_gateway_mac, 0, sizeof(s_gateway_mac));
            /* Clear from EEPROM */
            EEPROM.begin(EEPROM_SIZE);
            EEPROM.write(EEPROM_GATEWAY_MAC_ADDR, 0);
            EEPROM.commit();
            EEPROM.end();
        }
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
