#include "captive_portal.h"
#include "config.h"
#include "console.h"
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
    // Implemented in Task 3
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
