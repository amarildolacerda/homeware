// tests/unit/test/test_radio_mode.cpp
#include <unity.h>
#include "EEPROM.h"
#include "shared_config.h"
#include "radio_mode.h"

// radio_mode_impl.cpp compila com ESPNOW_ENABLED + TCP_ENABLED + RADIO_SELECT_ENABLED

static void local_setup(void) {
    EEPROMClass::reset();
}

void test_radio_mode_compiled_both(void) {
    TEST_ASSERT_TRUE(radio_mode_compiled(RADIO_MODE_ESPNOW));
    TEST_ASSERT_TRUE(radio_mode_compiled(RADIO_MODE_TCP));
    TEST_ASSERT_FALSE(radio_mode_compiled(7));
}

void test_radio_mode_id_and_label(void) {
    TEST_ASSERT_EQUAL_STRING("espnow", radio_mode_id(RADIO_MODE_ESPNOW));
    TEST_ASSERT_EQUAL_STRING("tcp", radio_mode_id(RADIO_MODE_TCP));
    TEST_ASSERT_EQUAL_STRING("ESP-NOW", radio_mode_label(RADIO_MODE_ESPNOW));
    TEST_ASSERT_EQUAL_STRING("TCP (HTTP)", radio_mode_label(RADIO_MODE_TCP));
}

void test_radio_mode_parse_valid(void) {
    uint8_t mode = 0xFF;
    TEST_ASSERT_TRUE(radio_mode_parse("tcp", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, mode);
    TEST_ASSERT_TRUE(radio_mode_parse("HTTP", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, mode);
    TEST_ASSERT_TRUE(radio_mode_parse("1", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, mode);
    TEST_ASSERT_TRUE(radio_mode_parse("espnow", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, mode);
    TEST_ASSERT_TRUE(radio_mode_parse("ESP-NOW", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, mode);
    TEST_ASSERT_TRUE(radio_mode_parse("0", &mode));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, mode);
}

void test_radio_mode_parse_invalid(void) {
    uint8_t mode = 0xFE;
    TEST_ASSERT_FALSE(radio_mode_parse("lora", &mode));
    TEST_ASSERT_FALSE(radio_mode_parse("", &mode));
    TEST_ASSERT_FALSE(radio_mode_parse("tcp ", &mode));
    TEST_ASSERT_FALSE(radio_mode_parse(nullptr, &mode));
    TEST_ASSERT_EQUAL_UINT8(0xFE, mode);
    TEST_ASSERT_FALSE(radio_mode_parse("tcp", nullptr));
}

void test_radio_mode_save_load_roundtrip(void) {
    local_setup();
    radio_mode_save(RADIO_MODE_TCP);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, EEPROM.read(EEPROM_RADIO_MODE_OFFSET));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, radio_mode_load());
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, radio_mode_active());

    radio_mode_save(RADIO_MODE_ESPNOW);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, EEPROM.read(EEPROM_RADIO_MODE_OFFSET));
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, radio_mode_load());
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, radio_mode_active());
}

void test_radio_mode_load_invalid_falls_back_to_default(void) {
    local_setup();
    EEPROM.write(EEPROM_RADIO_MODE_OFFSET, 0xAA);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, radio_mode_load());
}

void test_radio_mode_save_ignores_unknown_mode(void) {
    local_setup();
    radio_mode_save(RADIO_MODE_TCP);
    radio_mode_save(9);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, EEPROM.read(EEPROM_RADIO_MODE_OFFSET));
}

void test_radio_mode_set_active(void) {
    local_setup();
    radio_mode_set_active(RADIO_MODE_TCP);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_TCP, radio_mode_active());
    radio_mode_set_active(RADIO_MODE_ESPNOW);
    TEST_ASSERT_EQUAL_UINT8(RADIO_MODE_ESPNOW, radio_mode_active());
}
