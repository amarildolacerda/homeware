#include <unity.h>
#include "mock_lora.h"
#include "lora_spi_radio.h"

void test_lora_spi_config_size() {
    TEST_ASSERT_EQUAL(24, sizeof(LoraSpiConfig));
}

void test_lora_spi_radio_defaults() {
    LoraSpiConfig cfg;
    TEST_ASSERT_EQUAL(18, cfg.ss);
    TEST_ASSERT_EQUAL(14, cfg.rst);
    TEST_ASSERT_EQUAL(868.0f, cfg.freq);
}
