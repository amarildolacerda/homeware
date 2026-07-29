#include <unity.h>
#include "test_helper.h"

void test_lora_spi_config_size(void);
void test_lora_spi_radio_defaults(void);
void test_node_protocol_sends_pair_request_on_begin(void);
void test_node_protocol_sends_heartbeat_when_paired(void);
void test_node_protocol_pair_response(void);
void test_node_protocol_handles_command(void);

void setUp(void) {
    fake_millis_set(0);
}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lora_spi_config_size);
    RUN_TEST(test_lora_spi_radio_defaults);
    RUN_TEST(test_node_protocol_sends_pair_request_on_begin);
    RUN_TEST(test_node_protocol_sends_heartbeat_when_paired);
    RUN_TEST(test_node_protocol_pair_response);
    RUN_TEST(test_node_protocol_handles_command);
    UNITY_END();
    return 0;
}
