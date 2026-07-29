#include <unity.h>
#include "test_helper.h"

void test_lora_spi_config_size(void);
void test_lora_spi_radio_defaults(void);

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lora_spi_config_size);
    RUN_TEST(test_lora_spi_radio_defaults);
    UNITY_END();
    return 0;
}
