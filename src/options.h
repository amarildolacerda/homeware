

#include <Arduino.h>

#define VERSION "25.04.23.1"
//---- opcoes de boards  -- configurado no platformio.ini
//  #define NO_WM
//  #define WEMO_D1
//  #define BOARD_ESP12S // LCTECHRELAY
//  #define ESP8285
//  #define SONOFF_BASIC
//  #define ARDUINO_AVR

//-----------------------------------------------
#ifndef ARDUINO_AVR
#define LABEL String(getChipId(), HEX)
#else
#define LABEL "ARDUINO_AVR"
#endif
//-----------------------------------------------

#ifndef SPIFFs
#define LITTLEFs
#endif
//-----------------------------------------------
#ifndef API_SIZE
#define DRIVERS_SIZE 6
#define API_SIZE 2
#endif
//-----------------------------------------------
#ifdef DEBUG
#define DEBUGP(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#define DEBUGF(fmt) Serial.printf(fmt)
#else
#define DEBUGF(fmt)      // Faz nada quando DEBUG não está definido
#define DEBUGP(fmt, ...) // Faz nada quando DEBUG não está definido
#endif
//-----------------------------------------------
