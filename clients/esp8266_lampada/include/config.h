#pragma once

#ifndef FW_VERSION
#define FW_VERSION "v0.0.30"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP01-Relay"
#endif

#ifdef ESP01S
  #ifndef RELAY_PIN
    #define RELAY_PIN 0
  #endif
  #ifndef BUTTON_PIN
    #define BUTTON_PIN 2
  #endif
  // ESP-01S: no LED (GPIO2 used for button)
  #undef LED_PIN
#else
  #ifndef RELAY_PIN
    #define RELAY_PIN 15
  #endif
  #ifndef BUTTON_PIN
    #define BUTTON_PIN 4
  #endif
  #ifndef LED_PIN
    #define LED_PIN 2
  #endif
#endif

#ifndef RELAY_ON
#define RELAY_ON HIGH
#endif

#ifndef WIFI_CONFIG_PORTAL_SSID
#define WIFI_CONFIG_PORTAL_SSID "Lampada-Config"
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

/* Repeater MAC override. Se definido, o cliente usa o repeater em vez do gateway direto.
   Formato: "AA:BB:CC:DD:EE:FF". Deixe vazio para modo normal. */
#ifndef REPEATER_MAC
#define REPEATER_MAC ""
#endif

#ifndef MAX_TIMERS
#define MAX_TIMERS 6
#endif

#ifndef TIMER_CHECK_INTERVAL_MS
#define TIMER_CHECK_INTERVAL_MS 10000
#endif

#ifndef CYCLIC_CHECK_INTERVAL_MS
#define CYCLIC_CHECK_INTERVAL_MS 1000
#endif

#ifndef EEPROM_TIMEZONE_ADDR
#define EEPROM_TIMEZONE_ADDR 160
#endif

#ifndef EEPROM_TIMER_BASE
#define EEPROM_TIMER_BASE 161
#endif

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 256
#endif

/* Pinos GPIO disponiveis para configuracao do relé */
#ifdef ESP01S
  static const int AVAILABLE_GPIOS[] = {0, 2};
  static const int AVAILABLE_GPIOS_COUNT = 2;
#else
  static const int AVAILABLE_GPIOS[] = {0, 2, 4, 5, 12, 13, 14, 15, 16};
  static const int AVAILABLE_GPIOS_COUNT = 9;
#endif

#ifdef STATIC_WIFI
  #ifndef WIFI_SSID
    #define WIFI_SSID ""
  #endif
  #ifndef WIFI_PASS
    #define WIFI_PASS ""
  #endif
#endif
