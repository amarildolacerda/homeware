#pragma once

#ifndef DEVICE_NAME
#define DEVICE_NAME "LoRa Switch"
#endif

#ifndef RELAY_PIN
#define RELAY_PIN 12
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN 0
#endif
#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef RELAY_ON
#define RELAY_ON HIGH
#endif

#ifndef WIFI_CONFIG_PORTAL_SSID
#define WIFI_CONFIG_PORTAL_SSID "LoRa_Switch_Config"
#endif
#ifndef WIFI_CONFIG_PORTAL_PASS
#define WIFI_CONFIG_PORTAL_PASS "password123"
#endif

#ifndef STATE_UPDATE_INTERVAL
#define STATE_UPDATE_INTERVAL 60000
#endif
#ifndef HEARTBEAT_INTERVAL
#define HEARTBEAT_INTERVAL 60000
#endif
#ifndef LORA_PAIR_INTERVAL_MS
#define LORA_PAIR_INTERVAL_MS 5000
#endif
#ifndef LORA_MAX_PAIR_ATTEMPTS
#define LORA_MAX_PAIR_ATTEMPTS 20
#endif

#ifndef LED_BLINK_WIFI_MS
#define LED_BLINK_WIFI_MS 250
#endif
#ifndef LED_BLINK_PAIRED_MS
#define LED_BLINK_PAIRED_MS 1000
#endif

#ifndef DASHBOARD_PORT
#define DASHBOARD_PORT 80
#endif

#ifndef EEPROM_RELAY_STATE
#define EEPROM_RELAY_STATE 200
#endif
