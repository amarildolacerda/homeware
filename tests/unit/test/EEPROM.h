// tests/unit/test/EEPROM.h
// Mock de EEPROM para testes nativos (buffer estatico em EEPROM.cpp).
#pragma once
#include <stdint.h>
#include <string.h>

#ifndef EEPROM_MOCK_SIZE
#define EEPROM_MOCK_SIZE 512
#endif

class EEPROMClass {
public:
    void begin(size_t size) { (void)size; }
    void end() {}
    uint8_t read(int addr) {
        if (addr < 0 || addr >= EEPROM_MOCK_SIZE) return 0xFF;
        return s_data[addr];
    }
    void write(int addr, uint8_t val) {
        if (addr < 0 || addr >= EEPROM_MOCK_SIZE) return;
        s_data[addr] = val;
    }
    bool commit() { return true; }

    static uint8_t s_data[EEPROM_MOCK_SIZE];
    static void reset() { memset(s_data, 0, EEPROM_MOCK_SIZE); }
    static void fill(uint8_t val) { memset(s_data, val, EEPROM_MOCK_SIZE); }
};

extern EEPROMClass EEPROM;
