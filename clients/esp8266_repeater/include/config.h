#pragma once

#ifndef FW_VERSION
#define FW_VERSION "v0.0.18"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP-NOW Repeater"
#endif

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef SEQ_CACHE_SIZE
#define SEQ_CACHE_SIZE 16
#endif

#ifndef MAX_PEERS
#define MAX_PEERS 10
#endif

#ifndef DASHBOARD_PORT
#define DASHBOARD_PORT 80
#endif

#ifndef WIFI_CONFIG_PORTAL_SSID
#define WIFI_CONFIG_PORTAL_SSID "Repeater-Config"
#endif

#ifndef WIFI_CONFIG_PORTAL_PASS
#define WIFI_CONFIG_PORTAL_PASS "password123"
#endif

#define EEPROM_SIZE 64
#define EEPROM_MAGIC 0xBB
#define EEPROM_GATEWAY_MAC_ADDR 0
