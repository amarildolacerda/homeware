

#include <Arduino.h>

#define VERSION "23.03.11.11"

//---- opcoes de boards
#define WEMO_D1
// #define BOARD_ESP12S
// #define ESP8285
// #define SONOFF_BASIC
// #define ARDUINO_AVR

//-------------------------------


#if defined(WEMO_D1)
//----------------------
#define WIFI_ENABLED
#define DRIVERS_ENABLED
#define SINRIC
#define TELNET
#define USE_PIN_NAMES
#define LITTLEFS
#define DRIVERS_EX
#endif

#ifdef ESP32
#define TELEGRAM
#endif

#if defined(BOARD_ESP12S)
//-------------------------
#define BOARD_MIN
#define WIFI_ENABLED
#define DRIVERS_ENABLED

#endif



#ifndef ARDUINO_AVR
//-----------------------------------------------
#define LABEL String(getChipId(), HEX)
#else
#define LABEL "ARDUINO_AVR"
#endif

//********************************** boards

#if defined(ARDUINO_ESP8266_NODEMCU)
//-------------------------------------
#define BOARD_MIN
#endif

#if defined(ARDUINO_AVR)
#undef WIFI_ENABLED
#undef DRIVERS_ENABLED
#undef BOARD_MIN
#undef TELNET
#undef LITTLEFS
#undef USE_PIN_NAMES
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
#endif

#ifdef DRIVERS_ENABLED
#define DHT_SENSOR
#define GROOVE_ULTRASONIC
#endif

#if defined(SONOFF_BASIC)
#undef USE_PIN_NAMES
#undef DRIVERS_EX
#undef DHT_SENSOR
#undef GROOVE_ULTRASONIC
#endif

#if defined(BOARD_ESP12S) || defined(ESP8285) || defined(BOARD_MIN) || defined(SONOFF_BASIC)
#define WIFI_ENABLED
#define DRIVERS_ENABLED
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
