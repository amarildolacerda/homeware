

#include <Arduino.h>
#define LABEL String(getChipId(), HEX)
#define VERSION "23.03.08.8"
#define WIFI_ENABLED

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
#ifdef BOARD_ESP12S
#undef WEMO_D1
#undef ESP8285
#endif

//*****************************************
#define DRIVERS_ENABLED

#ifdef WIFI_ENABLED
#define PORTAL
#define ALEXA
#define SINRIC
#define OTA
#define WEBSOCKET
// #define TELNET
#endif

#ifdef DRIVERS_ENABLED
#define DHT_SENSOR
#define GROOVE_ULTRASONIC
#endif
// #define MQTT

#if defined(ESP8285) || defined(BOARD_MIN)
#undef SINRIC
#undef GROOVE_ULTRASONIC
#undef MQTT
#endif

#if defined(BOARD_ESP12S) || defined(ESP8285)
#undef SINRIC
#undef TELNET
#undef DHT_SENSOR
#undef GROOVE_ULTRASONIC
#endif

#ifdef BOARD_ESP12S
#define LCTECHRELAY
#endif
