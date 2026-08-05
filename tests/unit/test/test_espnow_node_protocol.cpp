#include <unity.h>
#include "mock_espnow.h"
#include "espnow_node_protocol.h"
#include "espnow_protocol.h"
#include <cstring>

MockEspnowSend g_mock_last_send;
int g_mock_send_count = 0;
bool g_mock_espnow_init_ret = true;
bool g_mock_load_gateway_ret = false;

static EspnowNodeProtocol s_proto;
static uint8_t s_test_mac[6] = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC };
static int s_on_command_called = 0;
static uint8_t s_on_command_state = 0;
static int s_on_paired_called = 0;
static int s_on_restart_called = 0;
static int s_on_forward_called = 0;
static uint8_t s_get_sensor_type_val = 8;

static void reset_globals() {
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    g_mock_send_count = 0;
    g_mock_espnow_init_ret = true;
    g_mock_load_gateway_ret = false;
    s_on_command_called = 0;
    s_on_paired_called = 0;
    s_on_restart_called = 0;
    s_on_forward_called = 0;
    s_get_sensor_type_val = 8;
    s_proto.callbacks.get_sensor_type = nullptr;
    s_proto.callbacks.get_sensor_payload = nullptr;
    s_proto.callbacks.on_command = nullptr;
    s_proto.callbacks.on_paired = nullptr;
    s_proto.callbacks.on_restart = nullptr;
    s_proto.callbacks.on_forward = nullptr;
}

static uint8_t test_get_type() { return s_get_sensor_type_val; }

static uint8_t test_get_payload(uint8_t* buf, uint8_t max_len) {
    if (max_len < 2) return 0;
    buf[0] = 0xAA;
    buf[1] = 0xBB;
    return 2;
}

static void test_on_command(uint8_t state) {
    s_on_command_called++;
    s_on_command_state = state;
}

static void test_on_paired(uint8_t slot) {
    s_on_paired_called++;
    (void)slot;
}

static void test_on_restart() {
    s_on_restart_called++;
}

static void test_on_forward(const uint8_t* data, size_t len, const uint8_t* mac) {
    s_on_forward_called++;
    (void)data; (void)len; (void)mac;
}

void test_espnow_init_sends_pair_request() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.begin();
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    TEST_ASSERT_EQUAL(MSG_PAIR_REQUEST, g_mock_last_send.data[0]);
}

void test_espnow_paired_after_pair_response() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_paired = test_on_paired;
    s_proto.begin();

    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    resp.sequence = 0;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 3;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    TEST_ASSERT_TRUE(s_proto.is_paired());
    TEST_ASSERT_EQUAL(3, s_proto.assigned_slot());
    TEST_ASSERT_EQUAL(1, s_on_paired_called);
}

void test_espnow_sends_sensor_data() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.get_sensor_type = test_get_type;
    s_proto.callbacks.get_sensor_payload = test_get_payload;
    s_proto.set_state_interval(0);
    s_proto.begin();

    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));

    g_mock_send_count = 0;
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    s_proto.publish_state();
    s_proto.loop();
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    TEST_ASSERT_EQUAL(MSG_SENSOR_DATA, g_mock_last_send.data[1]);
}

void test_espnow_heartbeat() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.set_heartbeat_interval(0);
    s_proto.begin();

    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));

    g_mock_send_count = 0;
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    s_proto.loop();
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    TEST_ASSERT_EQUAL(MSG_HEARTBEAT, g_mock_last_send.data[1]);
}

void test_espnow_command_callback() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_command = test_on_command;
    s_proto.begin();

    espnow_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    memcpy(cmd.target_mac, s_test_mac, 6);
    cmd.command = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(1, s_on_command_called);
    TEST_ASSERT_EQUAL(1, s_on_command_state);
}

void test_espnow_forward_callback() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_forward = test_on_forward;
    s_proto.begin();

    uint8_t buf[15] = {0};
    buf[0] = ESPNOW_PROTOCOL_VERSION;
    buf[1] = 0xFF;
    s_proto.handle_frame(s_test_mac, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(1, s_on_forward_called);
}

void test_espnow_force_repair() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.begin();

    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    TEST_ASSERT_TRUE(s_proto.is_paired());

    s_proto.force_repair();
    TEST_ASSERT_FALSE(s_proto.is_paired());
}

void test_espnow_load_gateway_mac() {
    reset_globals();
    g_mock_load_gateway_ret = true;
    s_proto.set_mac(s_test_mac);
    s_proto.load_gateway_mac();
    TEST_ASSERT_TRUE(s_proto.is_paired());
    const uint8_t* gw = s_proto.gateway_mac();
    TEST_ASSERT_EQUAL(0xAA, gw[0]);
}

void test_espnow_ack_timeout_retry() {
    reset_globals();
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.get_sensor_type = test_get_type;
    s_proto.set_state_interval(0);
    s_proto.set_heartbeat_interval(60000);  // keep heartbeat from firing
    s_proto.begin();

    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));

    s_proto.publish_state();
    g_mock_send_count = 0;
    s_proto.loop();
    TEST_ASSERT_EQUAL(1, g_mock_send_count);
}
