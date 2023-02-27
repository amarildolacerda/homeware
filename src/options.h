
//#ifdef ESP8266
#define WIFI_NEW
//#endif

#include <Arduino.h>
#define LABEL String(getChipId(), HEX)
#define VERSION "23.02.25.6"
#define ALEXA
#define SINRIC
#define TELNET
#define OTA
//#define GROOVE_ULTRASONIC
//#define MQTT


#if defined(ESP8285)
  #undef SINRIC
  #undef GROOVE_ULTRASONIC
  #undef MQTT
#endif
