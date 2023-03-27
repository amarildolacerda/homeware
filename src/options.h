

#include <Arduino.h>

#define VERSION "23.03.27.17"

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
//#define SINRICPRO
#define TELNET
#define DRIVERS_EX
#define USE_PIN_NAMES
// #define BUZZER
// #define LEDBAR
#endif

#ifdef ESP32
#define WIFI_ENABLED
#define DRIVERS_ENABLED
//#define SINRICPRO
#define TELNET
#define DRIVERS_EX
//#define TELEGRAM
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
#undef SINRICPRO
#undef ALEXA
#undef LITTLEFs

#endif


#if defined(SONOFF_BASIC)
#undef DRIVERS_ENABLED
#define BOARD_MIN
#undef DRIVERS_EX
#define WIFI_ENABLED
#undef TELNET
#define NO_DRV_ADC
#endif

#ifdef DRIVERS_ENABLED
//#define DHT_SENSOR
//#define GROOVE_ULTRASONIC
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
#define NO_DRV_ADC

#endif
