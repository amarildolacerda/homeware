

#include <Arduino.h>
#define LABEL String(getChipId(), HEX)
#define VERSION "23.03.03.7"
#define WIFI_ENABLED
#define DRIVERS_ENABLED

#ifdef WIFI_ENABLED
#define PORTAL
#define ALEXA
#define SINRIC
#define TELNET
#define OTA
#endif

#ifdef DRIVERS_ENABLED
#define DHT_SENSOR
#define GROOVE_ULTRASONIC
#endif
// #define MQTT

#if defined(ESP8285)
#undef SINRIC
#undef GROOVE_ULTRASONIC
#undef MQTT
#endif
