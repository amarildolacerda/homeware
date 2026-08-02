#pragma once
#include <Arduino.h>
#include "shared_config.h"
#include "tcp_protocol.h"
#include "espnow_protocol.h"

#define DEVICE_NAME "AgriSense TCP Node"
#define DEVICE_TYPE "tcp_node"
#define SENSOR_TYPE SENSOR_TYPE_TEMP_HUM

// WiFi
#define WIFI_CONFIG_PORTAL_SSID "AgriSense-TCP-Setup"
#define WIFI_CONFIG_PORTAL_PASS "agrisense"
#define WIFI_CONNECT_TIMEOUT 120000

// Hub connection defaults (overridden by EEPROM or UDP discovery)
#define HUB_IP_DEFAULT "192.168.1.100"
#define HUB_PORT TCP_HTTP_PORT

// UDP Discovery
#define DISCOVERY_INTERVAL 10000
#define MAX_DISCOVERY_RETRIES 20

// Intervals (ms)
#define STATE_UPDATE_INTERVAL 10000
#define HEARTBEAT_INTERVAL 30000
#define RECONNECT_INTERVAL 5000

// Retry
#define MAX_RETRIES 5
#define RETRY_BASE_DELAY 1000
#define RETRY_MAX_DELAY 30000
#define HTTP_TIMEOUT_MS 5000
#define HUB_FALLBACK_RETRIES 3

// LED
#define LED_PIN LED_BUILTIN
#define LED_ON LOW
#define LED_OFF HIGH
#define LED_BLINK_WIFI_MS 500
#define LED_BLINK_GATEWAY_MS 200

// EEPROM TCP-specific (shared offsets in shared_config.h)
#define EEPROM_HUB_IP_OFFSET 96
#define EEPROM_HUB_IP_VALID 112
#define EEPROM_RELAY_STATE 200

// Console
#define CONSOLE_BANNER "AgriSense TCP Node"
