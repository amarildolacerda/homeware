#include <unity.h>
#include "test_helper.h"
#include "lora_protocol.h"
#include "lora_node_protocol.h"

const uint8_t s_test_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
uint8_t test_get_sensor_type() { return 0xFF; }
uint8_t test_get_payload(uint8_t* buf, uint8_t max_len) { (void)buf; (void)max_len; return 0; }

static uint8_t s_last_command = 0xFF;
static uint8_t s_paired_slot = 0xFF;
static bool s_restart_called = false;
void test_on_command(uint8_t cmd) { s_last_command = cmd; }
void test_on_paired(uint8_t slot) { s_paired_slot = slot; }
void test_on_restart() { s_restart_called = true; }

static void setup_proto(LoraNodeProtocol& proto) {
    proto.callbacks.get_sensor_type = test_get_sensor_type;
    proto.callbacks.get_sensor_payload = test_get_payload;
    proto.callbacks.on_command = test_on_command;
    proto.callbacks.on_paired = test_on_paired;
    proto.callbacks.on_restart = test_on_restart;
    proto.set_mac(s_test_mac);
}

void test_node_protocol_sends_pair_request_on_begin() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.begin();
    fake_millis_advance(10);

    TEST_ASSERT(mock.m_last_sent_len > 0);
    lora_frame_t* frame = (lora_frame_t*)mock.m_last_sent;
    TEST_ASSERT_EQUAL(MSG_PAIR_REQUEST, frame->msg_type);
}

void test_node_protocol_sends_heartbeat_when_paired() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.set_heartbeat_interval(30000);
    proto.set_state_interval(120000);
    proto.begin();
    mock.m_last_sent_len = 0;

    lora_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    resp.assigned_slot = 1;
    memcpy(resp.sensor_id, s_test_mac, 6);
    mock.inject_rx((const uint8_t*)&resp, sizeof(resp), -50);

    mock.m_last_sent_len = 0;
    fake_millis_advance(30000);
    proto.loop();

    TEST_ASSERT(mock.m_last_sent_len > 0);
    lora_frame_t* frame = (lora_frame_t*)mock.m_last_sent;
    TEST_ASSERT_EQUAL(MSG_HEARTBEAT, frame->msg_type);
}

void test_node_protocol_pair_response() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.begin();

    lora_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = MSG_PAIR_RESPONSE;
    resp.assigned_slot = 5;
    memcpy(resp.sensor_id, s_test_mac, 6);
    mock.inject_rx((const uint8_t*)&resp, sizeof(resp), -50);

    TEST_ASSERT_TRUE(proto.is_paired());
    TEST_ASSERT_EQUAL(5, proto.assigned_slot());
    TEST_ASSERT_EQUAL(5, s_paired_slot);
}

void test_node_protocol_handles_command() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.begin();

    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = MSG_COMMAND;
    cmd.command = 1;
    memcpy(cmd.sensor_id, s_test_mac, 6);
    mock.inject_rx((const uint8_t*)&cmd, sizeof(cmd), -50);

    TEST_ASSERT_EQUAL(1, s_last_command);
}


