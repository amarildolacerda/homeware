

#include <Arduino.h>

#define VERSION "23.03.17.15"

//---- opcoes de boards  -- configurado no platformio.ini
// #define WEMO_D1
// #define BOARD_ESP12S // LCTECHRELAY
// #define ESP8285
// #define SONOFF_BASIC
// #define ARDUINO_AVR

//-------------------------------
#define LITTLEFs

#if defined(WEMO_D1)
//----------------------
#define WIFI_ENABLED
#define DRIVERS_ENABLED
#define SINRICPRO
#define TELNET
#define USE_PIN_NAMES
#define DRIVERS_EX
#endif

#ifdef ESP32
// -------------------------------------
#define TELEGRAM
// #undef LITTLEFs
#undef USE_PIN_NAMES
// #define SPIFFs
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
#undef SINRICPRO
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
#undef SINRICPRO
#undef DHT_SENSOR
#undef GROOVE_ULTRASONIC
#undef LCTECHRELAY
#undef SINRICPRO
#undef GROOVE_ULTRASONIC
#undef MQTT
#endif

#if defined(BOARD_ESP12S)
#define LCTECHRELAY
#define LITTLEFs
#define TELNET
#define WEBSOCKET

#endif
