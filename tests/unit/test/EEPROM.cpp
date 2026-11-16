// tests/unit/test/EEPROM.cpp
// Implementacao do mock de EEPROM.
#include "EEPROM.h"

uint8_t EEPROMClass::s_data[EEPROM_MOCK_SIZE] = {0};
EEPROMClass EEPROM;
