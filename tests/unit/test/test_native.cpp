#include <unity.h>
#include "test_helper.h"

void test_lora_spi_config_size(void);
void test_lora_spi_radio_defaults(void);
void test_node_protocol_sends_pair_request_on_begin(void);
void test_node_protocol_sends_heartbeat_when_paired(void);
void test_node_protocol_pair_response(void);
void test_node_protocol_handles_command(void);
void test_espnow_init_sends_pair_request(void);
void test_espnow_paired_after_pair_response(void);
void test_espnow_sends_sensor_data(void);
void test_espnow_heartbeat(void);
void test_espnow_command_callback(void);
void test_espnow_forward_callback(void);
void test_espnow_force_repair(void);
void test_espnow_load_gateway_mac(void);
void test_espnow_ack_timeout_retry(void);
void test_pagemanager_no_pages(void);
void test_pagemanager_add_page(void);
void test_pagemanager_switches_page(void);
void test_pagemanager_footer(void);
void test_pagemanager_page_render(void);
void test_pagemanager_cycles_back(void);

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
    RUN_TEST(test_espnow_init_sends_pair_request);
    RUN_TEST(test_espnow_paired_after_pair_response);
    RUN_TEST(test_espnow_sends_sensor_data);
    RUN_TEST(test_espnow_heartbeat);
    RUN_TEST(test_espnow_command_callback);
    RUN_TEST(test_espnow_forward_callback);
    RUN_TEST(test_espnow_force_repair);
    RUN_TEST(test_espnow_load_gateway_mac);
    RUN_TEST(test_espnow_ack_timeout_retry);
    RUN_TEST(test_pagemanager_no_pages);
    RUN_TEST(test_pagemanager_add_page);
    RUN_TEST(test_pagemanager_switches_page);
    RUN_TEST(test_pagemanager_footer);
    RUN_TEST(test_pagemanager_page_render);
    RUN_TEST(test_pagemanager_cycles_back);
    UNITY_END();
    return 0;
}
