

#include <Arduino.h>
#define LABEL String(getChipId(), HEX)
#define VERSION "23.03.03.7"
#define WIFI_ENABLED

#ifdef WIFI_ENABLED
#define PORTAL
#define ALEXA
#define SINRIC
#define TELNET
#define OTA
#endif

#define DHT_SENSOR

#define GROOVE_ULTRASONIC
// #define MQTT

#if defined(ESP8285)
#undef SINRIC
#undef GROOVE_ULTRASONIC
#undef MQTT
#endif
