#include "captive_portal.h"
#include "config.h"
#include "common_console.h"
#include "web_server.h"
#include "platform.h"
#include <WiFiUdp.h>

static WiFiUDP s_dns;
static bool s_submitted = false;
static unsigned long s_start = 0;

void captive_portal_start() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_CONFIG_PORTAL_SSID, WIFI_CONFIG_PORTAL_PASS);
    s_dns.begin(53);
    s_start = millis();
    console.printf("[PORTAL] AP '%s' IP %s\n",
        WIFI_CONFIG_PORTAL_SSID, WiFi.softAPIP().toString().c_str());
}

void captive_dns_poll() {
    if (s_dns.parsePacket() == 0) return;
    static uint8_t buf[512];
    int len = s_dns.read(buf, sizeof(buf));
    if (len < 12) return;
    IPAddress ap = WiFi.softAPIP();
    // Header: copy ID; set QR=1, AA=1, RA=1, RCODE=0
    buf[2] = 0x81;
    buf[3] = 0x80;
    buf[6] = 0x00; buf[7] = 0x01; // ANCOUNT = 1
    buf[8] = 0x00; buf[9] = 0x00;  // NSCOUNT = 0
    buf[10] = 0x00; buf[11] = 0x00; // ARCOUNT = 0
    // Find end of question section
    int pos = 12;
    while (pos < len && buf[pos] != 0) pos += buf[pos] + 1;
    pos += 1 + 4; // null terminator + QTYPE(2) + QCLASS(2)
    if (pos + 16 > sizeof(buf)) return;
    // Answer: name pointer 0xC00C, TYPE A, CLASS IN, TTL 60, RDLEN 4, RDATA = AP IP
    buf[pos++] = 0xC0; buf[pos++] = 0x0C;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x00; buf[pos++] = 0x3C;
    buf[pos++] = 0x00; buf[pos++] = 0x04;
    buf[pos++] = ap[0]; buf[pos++] = ap[1]; buf[pos++] = ap[2]; buf[pos++] = ap[3];
    s_dns.beginPacket(s_dns.remoteIP(), 53);
    s_dns.write(buf, pos);
    s_dns.endPacket();
}

bool captive_portal_submitted() {
    return s_submitted;
}

void captive_portal_set_submitted() {
    s_submitted = true;
}

void captive_portal_run() {
    while (true) {
        web_server_handle_client();
        captive_dns_poll();
        if (s_submitted) {
            console.println("[PORTAL] Config salva, reiniciando...");
            delay(300);
            ESP.restart();
        }
        if (millis() - s_start > 300000) {
            console.println("[PORTAL] Timeout, reiniciando...");
            delay(300);
            ESP.restart();
        }
        yield();
    }
}
