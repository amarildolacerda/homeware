// tests/unit/test/radio_mode_impl.cpp
// Compila o radio_mode.cpp do shared com os dois protocolos habilitados.
#define ESPNOW_ENABLED 1
#define TCP_ENABLED 1
#define RADIO_SELECT_ENABLED 1
#define RADIO_MODE_DEFAULT RADIO_MODE_ESPNOW

#include <Arduino.h>
#include "EEPROM.h"
#include "../../../shared/src/radio_mode.cpp"
