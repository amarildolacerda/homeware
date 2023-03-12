

#include <Arduino.h>

#define VERSION "23.03.11.11"

#if defined(ESP8266) || defined(ESP32)
#define WIFI_ENABLED
#define BOARD_MIN
#define SONOFF_BASIC
#define DRIVERS_ENABLED
#define SINRIC
#define TELNET
#define USE_PIN_NAMES
#define LITTLEFS
#define DRIVERS_EX
#else
#define ARDUINO_AVR
#endif

#ifndef ARDUINO_AVR
#define LABEL String(getChipId(), HEX)
#else
#define LABEL "ARDUINO_AVR"
#endif

//********************************** boards

#if defined(ARDUINO_ESP8266_NODEMCU)
#define BOARD_MIN
#elif defined(ESP8266)
// ESP8266
#define WEMO_D1
#define BOARD_ESP12S
#define ESP8285

#elif defined(ESP32)
// ESP32
// noop ainda
#endif

// compatibilizar os defines
#ifdef WEMO_D1
#undef BOARD_ESP12S
#undef ESP8285
#endif

#if defined(ARDUINO_AVR)
#undef WIFI_ENABLED
#undef DRIVERS_ENABLED
#undef BOARD_MIN
#undef TELNET
#undef LITTLEFS
#undef USE_PIN_NAMES
#endif

#ifdef BOARD_ESP12S
#undef WEMO_D1
#undef ESP8285
#endif

//*****************************************

#ifdef WIFI_ENABLED
#define PORTAL
#define ALEXA
#define OTA
#define WEBSOCKET
#endif

#ifdef BOARD_MIN
#undef SINRIC
#undef USE_PIN_NAMES
// #undef WEBSOCKET
#endif

#ifdef DRIVERS_ENABLED
#define DHT_SENSOR
#define GROOVE_ULTRASONIC
#endif

#if defined(SONOFF_BASIC)
#undef USE_PIN_NAMES
#undef DRIVERS_EX
#endif

#if defined(BOARD_ESP12S) || defined(ESP8285) || defined(BOARD_MIN) || defined(SONOFF_BASIC)
#undef SINRIC
#undef TELNET
#undef DHT_SENSOR
#undef GROOVE_ULTRASONIC
#undef LCTECHRELAY
#undef SINRIC
#undef GROOVE_ULTRASONIC
#undef MQTT
#endif

#if defined(BOARD_ESP12S)
#define LCTECHRELAY
#endif
