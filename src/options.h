

#include <Arduino.h>

#define VERSION "23.09.06.23"
//---- opcoes de boards  -- configurado no platformio.ini
// #define WEMO_D1
// #define BOARD_ESP12S // LCTECHRELAY
// #define ESP8285
// #define SONOFF_BASIC
// #define ARDUINO_AVR

//-------------------------------
#ifndef SPIFFs
#define LITTLEFs
#endif

#if defined(WEMO_D1)
//----------------------
#define WIFI_ENABLED
#define WEBSOCKET

#ifndef NO_DRIVERS_ENABLED
#define DRIVERS_ENABLED
#define DRIVERS_EX
#endif

// #define SINRICPRO
#define TELNET
#define USE_PIN_NAMES
// #define BUZZER
// #define LEDBAR
// #define MQTTBroker
#endif

#ifdef ESP32
#define WIFI_ENABLED
#define DRIVERS_ENABLED
// #define SINRICPRO
#define TELNET
#define DRIVERS_EX
// #define TELEGRAM
#undef LITTLEFs
#undef USE_PIN_NAMES
#define NO_MDNS
#endif

#ifdef SPIFFs
#undef LITTLEFs
#endif

#if NO_WIFI
#undef WIFI_ENABLED
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



#if defined(NO_DRIVERS_ENABLED) || defined(NO_DRIVERS)
#undef DRIVERS_ENABLED
#undef DRIVERS_EX
#endif

#ifdef DRIVERS_ENABLED
// #define DHT_SENSOR
// #define GROOVE_ULTRASONIC
#endif

#ifdef BOARD_MIN
#undef SINRICPRO
#undef USE_PIN_NAMES
#endif

#if defined(BOARD_ESP12S) || defined(ESP8285) || defined(BOARD_MIN) || defined(SONOFF_BASIC) || defined(ESP01)
#define WIFI_ENABLED
#define DRIVERS_ENABLED
#undef SINRICPRO
#undef DHT_SENSOR
#undef GROOVE_ULTRASONIC
#undef LCTECHRELAY
#undef SINRICPRO
#undef GROOVE_ULTRASONIC
#undef MQTTClient
#endif

//*****************************************
#ifdef WIFI_ENABLED
#define PORTAL
#define ALEXA
#define OTA
#define WEBSOCKET
#endif

#if defined(SONOFF_BASIC)
#undef DRIVERS_ENABLED
#undef DRIVERS_EX
#define WIFI_ENABLED
#define NO_DRV_ADC
#define TELNET
#define MQTTClient
#define LED_INVERT
#endif

#if defined(BOARD_ESP12S)
#define LCTECHRELAY
#define LITTLEFs
#define TELNET
#define WEBSOCKET
#define NO_DRV_ADC
#define NO_DRV_PWM
#define DRIVERS_SIZE 8
#define API_SIZE 4
#endif

#ifdef ESP01
#define TELNET
#undef WEBSOCKET
#undef OTA
#undef ALEXA
#define NO_DRV_ADC
#define NO_DRV_PWM
#undef DRIVERS_ENABLED
#define DRIVERS_EX
#define LITTLEFs
#undef SINRICPRO
#undef USE_PIN_NAMES
#define BASIC
#define NO_MDNS
// #define NO_API
#define NO_WM
#define DRIVERS_SIZE 8
#define API_SIZE 4
#define MQTTClient
#endif

#ifdef MINIMAL
#undef DRIVERS_EX
#endif

#ifdef NO_WIFI
#define NO_MQTT
#define NO_PORTAL
#define NO_WEBSOCKET
#define NO_TELNET
#define NO_OTA
#define NO_ALEXA
#endif

#ifdef NO_MQTT
#undef MQTTClient
#undef MQTTBroker
#endif

#ifdef ESP32CAM
#undef PORTAL
#undef WEBSOCKET
#endif

#ifdef NO_PORTAL
#undef PORTAL
#endif
#ifdef NO_WEBSOCKET
#undef WEBSOCKET
#endif

#ifdef NO_MQTTClient
#undef MQTTClient
#endif

#ifdef NO_TELNET
#undef TELNET
#endif

#ifdef NO_ALEXA
#undef ALEXA
#endif

#ifdef NO_OTA
#undef OTA
#endif

#ifndef API_SIZE
#define DRIVERS_SIZE 24
#define API_SIZE 12
#endif

#if defined(MQTTClient) || defined(MYSENSORS)
#define MQTTClientEnabled
#endif