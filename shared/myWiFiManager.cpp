#include "myWiFiManager.h"
#include <EEPROM.h>

static wifi_state_t s_state = WIFI_STATE_DISCONNECTED;
static char s_ssid[33] = {0};
static unsigned long s_last_attempt = 0;
static unsigned long s_portal_start = 0;
static bool s_portal_active = false;

#if defined(ARDUINO_ARCH_ESP32)
  static bool s_reconnect_active = false;
  static unsigned long s_reconnect_deadline = 0;
#else
  static WiFiManager s_wm;
#endif

// --- EEPROM helpers autocontidos (nao dependem do gateway) ---
static bool sh_creds_load(char *ssid, char *pass) {
    EEPROM.begin(EEPROM_SIZE);
    bool valid = false;
    int pos = 0;
    for (int i = 0; i < EEPROM_WIFI_SSID_SIZE; i++) {
        uint8_t c = EEPROM.read(EEPROM_WIFI_SSID_OFFSET + i);
        if (c == 0) { valid = pos > 0; break; }
        if (c < 32 || c > 126) break;
        ssid[pos++] = (char)c;
    }
    ssid[pos] = '\0';
    for (int i = 0; i < EEPROM_WIFI_PASS_SIZE; i++)
        pass[i] = EEPROM.read(EEPROM_WIFI_PASS_OFFSET + i);
    pass[EEPROM_WIFI_PASS_SIZE - 1] = '\0';
    EEPROM.end();
    return valid && strlen(ssid) > 0;
}

static void sh_net_load(int *mode, char *ip, char *gw, char *mask, char *dns) {
    EEPROM.begin(EEPROM_SIZE);
    *mode = EEPROM.read(EEPROM_WIFI_MODE_OFFSET) == WIFI_MODE_STATIC ? WIFI_MODE_STATIC : WIFI_MODE_DHCP;
    auto read_str = [](int off, int size, char *buf) {
        int pos = 0;
        for (int i = 0; i < size - 1; i++) {
            uint8_t c = EEPROM.read(off + i);
            if (c == 0) break;
            if (c < 32 || c > 126) break;
            buf[pos++] = (char)c;
        }
        buf[pos] = '\0';
    };
    read_str(EEPROM_WIFI_IP_OFFSET, EEPROM_WIFI_IP_SIZE, ip);
    read_str(EEPROM_WIFI_GW_OFFSET, EEPROM_WIFI_GW_SIZE, gw);
    read_str(EEPROM_WIFI_MASK_OFFSET, EEPROM_WIFI_MASK_SIZE, mask);
    read_str(EEPROM_WIFI_DNS_OFFSET, EEPROM_WIFI_DNS_SIZE, dns);
    EEPROM.end();
}

static void apply_static_ip() {
    int mode = WIFI_MODE_DHCP;
    char ip[16], gw[16], mask[16], dns[16];
    sh_net_load(&mode, ip, gw, mask, dns);
    if (mode != WIFI_MODE_STATIC || strlen(ip) == 0) return;
    IPAddress ipa, gwa, maska, dnsa;
    if (!ipa.fromString(ip)) return;
    maska = IPAddress(255, 255, 255, 0);
    if (mask[0]) maska.fromString(mask);
    gwa = INADDR_NONE;
    if (gw[0]) gwa.fromString(gw);
    dnsa = gwa;
    if (dns[0]) dnsa.fromString(dns);
    WiFi.config(ipa, gwa, maska, dnsa);
}

bool mywifi_begin(bool force_portal) {
    char saved_ssid[EEPROM_WIFI_SSID_SIZE];
    char saved_pass[EEPROM_WIFI_PASS_SIZE];
    bool have_creds = sh_creds_load(saved_ssid, saved_pass);
    if (saved_ssid[0]) strncpy(s_ssid, saved_ssid, sizeof(s_ssid) - 1);

#if defined(ARDUINO_ARCH_ESP32)
    WiFi.mode(WIFI_STA);
    apply_static_ip();
    if (!force_portal && have_creds) {
        WiFi.begin(saved_ssid, saved_pass);
        s_state = WIFI_STATE_CONNECTING;
        s_last_attempt = millis();
        return true;
    }
    s_state = WIFI_STATE_PORTAL;
    s_portal_active = true;
    s_portal_start = millis();
    return false;
#else
    if (!force_portal && have_creds) {
        WiFi.mode(WIFI_STA);
        apply_static_ip();
        WiFi.begin(saved_ssid, saved_pass);
        s_state = WIFI_STATE_CONNECTING;
        s_last_attempt = millis();
        return true;
    }
    s_state = WIFI_STATE_PORTAL;
    s_portal_active = true;
    s_portal_start = millis();
    s_wm.setConfigPortalTimeout(300);
    s_wm.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS);
    return false;
#endif
}

void mywifi_loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (s_state != WIFI_STATE_CONNECTED) s_state = WIFI_STATE_CONNECTED;
        s_portal_active = false;
        return;
    }

    if (s_state == WIFI_STATE_PORTAL) {
        if (s_portal_active && millis() - s_portal_start > 300000)
            s_portal_active = false;
#if !defined(ARDUINO_ARCH_ESP32)
        s_wm.process();
#endif
        return;
    }

    static unsigned long last_attempt = 0;
    if (!s_reconnect_active) {
        if (millis() - last_attempt < 30000) return;
        last_attempt = millis();
        WiFi.mode(WIFI_STA);
        apply_static_ip();
        char ssid[EEPROM_WIFI_SSID_SIZE];
        char pass[EEPROM_WIFI_PASS_SIZE];
        if (sh_creds_load(ssid, pass)) {
            WiFi.begin(ssid, pass);
        } else {
            WiFi.begin();
        }
        s_reconnect_active = true;
        s_reconnect_deadline = millis() + 15000;
    } else if (millis() >= s_reconnect_deadline) {
        s_reconnect_active = false;
    }
}

wifi_state_t mywifi_state() { return s_state; }
const char* mywifi_ssid() { return s_ssid; }

bool mywifi_portal(char *name_buf, size_t name_size, void (*on_name)(const char*)) {
#if defined(ARDUINO_ARCH_ESP32)
    return false; // ESP32 portal tratado separadamente
#else
    WiFiManager wm;
    wm.setConfigPortalTimeout(300);
    WiFiManagerParameter custom_dev_name("dev_name", "Device Name",
                                         name_buf ? name_buf : "", name_size > 0 ? (int)name_size : 32);
    wm.addParameter(&custom_dev_name);
    if (wm.startConfigPortal(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS)) {
        if (name_buf && on_name) {
            const char *v = custom_dev_name.getValue();
            if (v && v[0]) {
                strncpy(name_buf, v, name_size - 1);
                name_buf[name_size - 1] = '\0';
                on_name(name_buf);
            }
        }
        return true;
    }
    return false;
#endif
}

void mywifi_espnow_mac(uint8_t *out) {
    uint8_t wifi_mac[6];
    WiFi.macAddress(wifi_mac);
    memcpy(out, wifi_mac, 6);
    out[0] ^= 0x02; // MAC alt do ESP-NOW
}
