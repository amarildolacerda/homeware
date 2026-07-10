#pragma once

#ifndef FW_VERSION
#define FW_VERSION "v0.0.21"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "OnOff"
#endif

#ifndef RELAY_PIN
#define RELAY_PIN 15
#endif

#ifndef BUTTON_PIN
#define BUTTON_PIN 4
#endif

#ifndef RELAY_ON
#define RELAY_ON HIGH
#endif

#ifndef LED_PIN
#define LED_PIN 2
#endif

#ifndef WIFI_CONFIG_PORTAL_SSID
#define WIFI_CONFIG_PORTAL_SSID "OnOff-Config"
#endif

#ifndef WIFI_CONFIG_PORTAL_PASS
#define WIFI_CONFIG_PORTAL_PASS "password123"
#endif

#ifndef STATE_UPDATE_INTERVAL
#define STATE_UPDATE_INTERVAL 10000
#endif

#ifndef HEARTBEAT_INTERVAL
#define HEARTBEAT_INTERVAL 60000
#endif

#ifndef ESPNOW_PAIR_INTERVAL_MS
#define ESPNOW_PAIR_INTERVAL_MS 5000
#endif

#ifndef ESPNOW_MAX_PAIR_ATTEMPTS
#define ESPNOW_MAX_PAIR_ATTEMPTS 10
#endif

#ifndef ESPNOW_ACK_TIMEOUT_MS
#define ESPNOW_ACK_TIMEOUT_MS 500
#endif

#ifndef ESPNOW_SEND_RETRIES
#define ESPNOW_SEND_RETRIES 3
#endif

#ifndef LED_BLINK_WIFI_MS
#define LED_BLINK_WIFI_MS 250
#endif

#ifndef LED_BLINK_GATEWAY_MS
#define LED_BLINK_GATEWAY_MS 500
#endif

#ifndef DASHBOARD_PORT
#define DASHBOARD_PORT 80
#endif

#ifndef ESP_NOW_CHANNEL
#define ESP_NOW_CHANNEL 0
#endif

/* Timer */
#ifndef MAX_TIMERS
#define MAX_TIMERS 6
#endif
#define TIMER_CHECK_INTERVAL_MS 10000
#define EEPROM_TIMEZONE_ADDR 160
#define EEPROM_TIMER_BASE 161

/* Pulse (auto-OFF timer like Tuya) */
#define PULSE_DEFAULT_DURATION_MIN 60
#ifndef PULSE_MIN_MINUTES
#define PULSE_MIN_MINUTES 1
#endif
#ifndef PULSE_MAX_MINUTES
#define PULSE_MAX_MINUTES 1440
#endif

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 256
#endif

/* Repeater MAC override. Se definido, o cliente usa o repeater em vez do gateway direto.
   Formato: "AA:BB:CC:DD:EE:FF". Deixe vazio para modo normal. */
#ifndef REPEATER_MAC
#define REPEATER_MAC ""
#endif

/* Pinos GPIO disponiveis para configuracao do relé */
static const int AVAILABLE_GPIOS[] = {0, 2, 4, 5, 12, 13, 14, 15, 16};
static const int AVAILABLE_GPIOS_COUNT = 9;
