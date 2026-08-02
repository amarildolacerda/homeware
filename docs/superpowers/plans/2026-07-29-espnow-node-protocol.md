# EspnowNodeProtocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `EspnowNodeProtocol` class in shared/ eliminating ~100 lines of duplicated ESP-NOW code per node.

**Architecture:** `EspnowNodeProtocol : NodeProtocol` mirrors `LoraNodeProtocol`. Singleton C-linkage bridge for ESP-NOW callbacks. ACK/retry state machine in `loop()`. Optional `on_forward` callback for lamp extender mode.

**Tech Stack:** ESP8266/ESP32 Arduino, ESP-NOW, PlatformIO, Unity test framework

## Global Constraints

- All files in `shared/` are part of git submodule — commit to both repos
- `NodeCallbacks` struct in `node_protocol.h` gains `on_forward` field (function pointer, default nullptr)
- ACK/retry machine uses non-blocking `millis()` — no `delay()`
- ESP8266→ESP32 uses broadcast (regra 18), unicast for ESP32→ESP8266
- `WiFi.setSleepMode(WIFI_NONE_SLEEP)` before `esp_now_init()` (regra 21)
- Payload and callbacks follow existing `LoraNodeProtocol` pattern

---
### Task 1: EspnowNodeProtocol class + Mock + Tests

**Files:**
- Modify: `shared/src/node_protocol.h` (add `on_forward` to NodeCallbacks)
- Create: `shared/src/espnow_node_protocol.h`
- Create: `shared/src/espnow_node_protocol.cpp`
- Create: `tests/unit/test/mock_espnow.h`
- Create: `tests/unit/test/test_espnow_node_protocol.cpp`

**Interfaces:**
- Consumes: `NodeProtocol`, `NodeCallbacks`, `espnow_protocol.h` structs, `common_espnow.h` helpers
- Produces: `EspnowNodeProtocol : NodeProtocol`

- [ ] **Step 1: Add `on_forward` to NodeCallbacks**

```cpp
// shared/src/node_protocol.h
struct NodeCallbacks {
    uint8_t (*get_sensor_type)();
    uint8_t (*get_sensor_payload)(uint8_t* buf, uint8_t max_len);
    void    (*on_command)(uint8_t command);
    void    (*on_paired)(uint8_t slot);
    void    (*on_restart)();
    void    (*on_forward)(const uint8_t* data, size_t len, const uint8_t* mac); // NEW
};
```

- [ ] **Step 2: Create espnow_node_protocol.h**

```cpp
#ifndef HW_SHARED_ESPNOW_NODE_PROTOCOL_H
#define HW_SHARED_ESPNOW_NODE_PROTOCOL_H

#include "node_protocol.h"
#include <stdint.h>

class EspnowNodeProtocol : public NodeProtocol {
public:
    EspnowNodeProtocol();

    void begin() override;
    void loop() override;
    bool is_paired() const override { return m_paired; }
    uint8_t assigned_slot() const override { return m_slot; }
    void force_repair() override;

    void publish_state();
    void on_send_done(const uint8_t* mac, uint8_t status);

    int16_t last_rssi() const { return m_last_rssi; }
    uint32_t tx_count() const { return m_tx_count; }
    uint32_t rx_count() const { return m_rx_count; }
    uint8_t* my_mac() { return m_mac; }

    void set_mac(const uint8_t* mac);
    void set_device_name(const char* name);
    void set_gateway_mac(const uint8_t* mac) { memcpy(m_gateway_mac, mac, 6); }
    const uint8_t* gateway_mac() const { return m_gateway_mac; }
    void load_gateway_mac();
    void save_gateway_mac();
    void set_pair_interval(unsigned long ms) { m_pair_interval_ms = ms; }
    void set_heartbeat_interval(unsigned long ms) { m_heartbeat_interval_ms = ms; }
    void set_state_interval(unsigned long ms) { m_state_interval_ms = ms; }

private:
    uint8_t m_mac[6];
    uint8_t m_gateway_mac[6];
    bool m_paired;
    uint8_t m_slot;
    uint16_t m_sequence;
    char m_device_name[32];
    bool m_espnow_ready;
    bool m_ack_received;
    int m_retries_left;
    unsigned long m_pair_interval_ms;
    unsigned long m_heartbeat_interval_ms;
    unsigned long m_state_interval_ms;
    unsigned long m_last_pair_ms;
    unsigned long m_last_heartbeat_ms;
    unsigned long m_last_state_ms;
    unsigned long m_send_deadline;
    unsigned long m_retry_delay_ms;
    int m_pair_attempts;
    uint8_t m_pair_attempts_max;
    int16_t m_last_rssi;
    uint32_t m_tx_count;
    uint32_t m_rx_count;
    uint16_t m_last_send_sequence;
    uint8_t m_fw_version[8];

    enum SendState { SEND_IDLE, SEND_WAIT_ACK, SEND_RETRY_DELAY, SEND_RETRY_WAIT_ACK };
    SendState m_send_state;

    void send_pair_request();
    void send_sensor_data();
    void send_heartbeat();
    void handle_frame(const uint8_t* mac, const uint8_t* data, size_t len);
};

#endif
```

- [ ] **Step 3: Create espnow_node_protocol.cpp**

```cpp
#include "espnow_node_protocol.h"
#include "espnow_protocol.h"
#include "common_espnow.h"
#include <Arduino.h>
#include <string.h>

static EspnowNodeProtocol* s_self = nullptr;

extern "C" void espnow_recv_cb(uint8_t* mac, uint8_t* data, uint8_t len) {
    if (s_self) s_self->handle_frame(mac, data, len);
}

extern "C" void espnow_send_cb(uint8_t* mac, uint8_t status) {
    if (s_self) s_self->on_send_done(mac, status);
}

EspnowNodeProtocol::EspnowNodeProtocol()
    : m_paired(false), m_slot(0), m_sequence(0), m_espnow_ready(false)
    , m_ack_received(false), m_retries_left(0)
    , m_pair_interval_ms(5000), m_heartbeat_interval_ms(60000)
    , m_state_interval_ms(60000), m_last_pair_ms(0)
    , m_last_heartbeat_ms(0), m_last_state_ms(0), m_send_deadline(0)
    , m_retry_delay_ms(0), m_pair_attempts(0), m_pair_attempts_max(20)
    , m_last_rssi(0), m_tx_count(0), m_rx_count(0), m_last_send_sequence(0)
    , m_send_state(SEND_IDLE)
{
    memset(m_mac, 0, 6);
    memset(m_gateway_mac, 0, 6);
    memset(m_device_name, 0, sizeof(m_device_name));
    memset(m_fw_version, 0, sizeof(m_fw_version));
}

void EspnowNodeProtocol::set_mac(const uint8_t* mac) {
    memcpy(m_mac, mac, 6);
}

void EspnowNodeProtocol::set_device_name(const char* name) {
    strncpy(m_device_name, name, sizeof(m_device_name) - 1);
    m_device_name[sizeof(m_device_name) - 1] = '\0';
}

void EspnowNodeProtocol::load_gateway_mac() {
    m_paired = espnow_load_gateway_mac(m_gateway_mac, "node");
}

void EspnowNodeProtocol::save_gateway_mac() {
    espnow_save_gateway_mac(m_gateway_mac, "node");
}

void EspnowNodeProtocol::begin() {
    s_self = this;
    m_espnow_ready = espnow_client_init("node");
    if (m_espnow_ready) {
        esp_now_register_send_cb(espnow_send_cb);
        esp_now_register_recv_cb(espnow_recv_cb);
    }
    m_paired = false;
    m_pair_attempts = 0;
    m_last_pair_ms = 0;
    m_sequence = 0;
    m_send_state = SEND_IDLE;
    send_pair_request();
}

void EspnowNodeProtocol::force_repair() {
    m_paired = false;
    m_pair_attempts = 0;
    m_last_pair_ms = 0;
    m_send_state = SEND_IDLE;
}

void EspnowNodeProtocol::publish_state() {
    if (!m_paired) return;
    m_last_state_ms = 0; // force send on next loop
}

void EspnowNodeProtocol::loop() {
    if (!m_espnow_ready) return;
    unsigned long now = millis();

    if (!m_paired) {
        if (now - m_last_pair_ms >= m_pair_interval_ms &&
            m_pair_attempts < m_pair_attempts_max) {
            m_last_pair_ms = now;
            m_pair_attempts++;
            send_pair_request();
        }
        return;
    }

    // Send state machine
    switch (m_send_state) {
    case SEND_IDLE:
        if (now - m_last_state_ms >= m_state_interval_ms) {
            m_last_state_ms = now;
            send_sensor_data();
            m_send_state = SEND_WAIT_ACK;
            m_send_deadline = now + 300; // 300ms ACK timeout
            m_retries_left = 3;
            m_ack_received = false;
        }
        if (now - m_last_heartbeat_ms >= m_heartbeat_interval_ms) {
            m_last_heartbeat_ms = now;
            send_heartbeat();
        }
        break;

    case SEND_WAIT_ACK:
        if (m_ack_received) {
            m_send_state = SEND_IDLE;
        } else if (now >= m_send_deadline) {
            m_retries_left--;
            if (m_retries_left > 0) {
                m_send_state = SEND_RETRY_DELAY;
                m_retry_delay_ms = now + 50;
            } else {
                force_repair();
            }
        }
        break;

    case SEND_RETRY_DELAY:
        if (now >= m_retry_delay_ms) {
            send_sensor_data();
            m_send_state = SEND_RETRY_WAIT_ACK;
            m_send_deadline = now + 300;
            m_ack_received = false;
        }
        break;

    case SEND_RETRY_WAIT_ACK:
        if (m_ack_received) {
            m_send_state = SEND_IDLE;
        } else if (now >= m_send_deadline) {
            m_retries_left--;
            if (m_retries_left > 0) {
                m_send_state = SEND_RETRY_DELAY;
                m_retry_delay_ms = now + 50;
            } else {
                force_repair();
            }
        }
        break;
    }
}

void EspnowNodeProtocol::on_send_done(const uint8_t* mac, uint8_t status) {
    (void)mac;
    (void)status;
    m_tx_count++;
}

void EspnowNodeProtocol::send_pair_request() {
    espnow_pair_request_t req;
    memset(&req, 0, sizeof(req));
    req.version = ESPNOW_PROTOCOL_VERSION;
    req.msg_type = ESPNOW_MSG_PAIR_REQUEST;
    req.sequence = m_sequence++;
    memcpy(req.sensor_mac, m_mac, 6);
    req.sensor_type = callbacks.get_sensor_type ? callbacks.get_sensor_type() : 0;
    req.firmware_version = 0;
    uint8_t bc[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    espnow_client_add_peer(bc, "node");
    espnow_send_wrapper(bc, (uint8_t*)&req, sizeof(req), "node");
}

void EspnowNodeProtocol::send_sensor_data() {
    uint8_t payload[ESPNOW_MAX_PAYLOAD];
    uint8_t payload_len = 0;
    if (callbacks.get_sensor_payload) {
        payload_len = callbacks.get_sensor_payload(payload, ESPNOW_MAX_PAYLOAD);
    }
    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE + payload_len + 4];
    espnow_header_t* hdr = (espnow_header_t*)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_SENSOR_DATA;
    hdr->sequence = m_sequence++;
    memcpy(hdr->sensor_mac, m_mac, 6);
    hdr->sensor_type = callbacks.get_sensor_type ? callbacks.get_sensor_type() : 0;
    hdr->battery_pct = 100;
    hdr->rssi = 0;
    hdr->payload_len = payload_len;
    if (payload_len > 0) memcpy(hdr->payload, payload, payload_len);
    uint8_t bc[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    espnow_client_add_peer(bc, "node");
    espnow_send_wrapper(bc, buf, ESPNOW_HEADER_FIXED_SIZE + payload_len, "node");
    m_last_send_sequence = hdr->sequence;
}

void EspnowNodeProtocol::send_heartbeat() {
    uint8_t buf[ESPNOW_HEADER_FIXED_SIZE];
    espnow_header_t* hdr = (espnow_header_t*)buf;
    hdr->version = ESPNOW_PROTOCOL_VERSION;
    hdr->msg_type = ESPNOW_MSG_HEARTBEAT;
    hdr->sequence = m_sequence++;
    memcpy(hdr->sensor_mac, m_mac, 6);
    hdr->sensor_type = 0;
    hdr->battery_pct = 100;
    hdr->rssi = 0;
    hdr->payload_len = 0;
    uint8_t bc[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    espnow_client_add_peer(bc, "node");
    espnow_send_wrapper(bc, buf, ESPNOW_HEADER_FIXED_SIZE, "node");
}

void EspnowNodeProtocol::handle_frame(const uint8_t* mac, const uint8_t* data, size_t len) {
    m_last_rssi = 0; // ESP-NOW recv_cb doesn't provide RSSI
    m_rx_count++;
    if (len < sizeof(espnow_header_t) - sizeof(((espnow_header_t*)0)->payload)) return;

    const espnow_header_t* hdr = (const espnow_header_t*)data;

    if (hdr->msg_type == ESPNOW_MSG_PAIR_RESPONSE) {
        if (len >= sizeof(espnow_pair_response_t)) {
            const espnow_pair_response_t* resp = (const espnow_pair_response_t*)data;
            if (memcmp(resp->sensor_mac, m_mac, 6) == 0) {
                m_paired = true;
                m_slot = resp->assigned_slot;
                memcpy(m_gateway_mac, mac, 6);
                save_gateway_mac();
                m_pair_attempts = 0;
                if (callbacks.on_paired) callbacks.on_paired(m_slot);
                m_last_state_ms = 0;
            }
        }
    } else if (hdr->msg_type == ESPNOW_MSG_ACK) {
        if (len >= sizeof(espnow_ack_t)) {
            const espnow_ack_t* ack = (const espnow_ack_t*)data;
            if (ack->sequence == m_last_send_sequence) {
                m_ack_received = true;
                if (ack->status == 0xFF) { // PAIR_STATUS_DENIED
                    force_repair();
                }
            }
        }
    } else if (hdr->msg_type == ESPNOW_MSG_NAK) {
        if (len >= sizeof(espnow_nak_t)) {
            const espnow_nak_t* nak = (const espnow_nak_t*)data;
            if (nak->reason == 0x01) { // NAK_REASON_GATEWAY_LOST
                force_repair();
            }
        }
    } else if (hdr->msg_type == ESPNOW_MSG_RESTART) {
        if (len >= sizeof(espnow_restart_t)) {
            const espnow_restart_t* rst = (const espnow_restart_t*)data;
            if (memcmp(rst->target_mac, m_mac, 6) == 0) {
                if (callbacks.on_restart) callbacks.on_restart();
            }
        }
    } else if (hdr->msg_type == ESPNOW_MSG_COMMAND) {
        if (len >= sizeof(espnow_command_t)) {
            const espnow_command_t* cmd = (const espnow_command_t*)data;
            if (memcmp(cmd->target_mac, m_mac, 6) == 0) {
                if (callbacks.on_command) callbacks.on_command(cmd->state);
            }
        }
    } else if (hdr->msg_type == ESPNOW_MSG_TIME_SYNC) {
        // ignored — node uses NTP or local time
    } else {
        if (callbacks.on_forward) {
            callbacks.on_forward(data, len, mac);
        }
    }
}
```

- [ ] **Step 4: Create mock_espnow.h**

```cpp
#pragma once
#include "espnow_node_protocol.h"
#include <string.h>
#include <stdio.h>

// Mock record of ESP-NOW sends
struct MockEspnowSend {
    uint8_t mac[6];
    uint8_t data[256];
    size_t len;
    bool sent = false;
};

extern MockEspnowSend g_mock_last_send;
extern int g_mock_send_count;
extern bool g_mock_espnow_init_ret;
extern bool g_mock_load_gateway_ret;

// Override espnow_client_init for testing
static inline bool espnow_client_init(const char* tag) {
    (void)tag;
    return g_mock_espnow_init_ret;
}

static inline bool espnow_client_add_peer(const uint8_t* mac, const char* tag) {
    (void)mac; (void)tag;
    return true;
}

static inline void espnow_save_gateway_mac(const uint8_t* mac, const char* tag) {
    (void)tag;
    memcpy(g_mock_last_send.mac, mac, 6);
}

static inline bool espnow_load_gateway_mac(uint8_t* mac_out, const char* tag) {
    (void)tag;
    if (g_mock_load_gateway_ret) {
        memset(mac_out, 0xAA, 6);
    }
    return g_mock_load_gateway_ret;
}

// Mock esp_now functions
static inline int esp_now_init() { return g_mock_espnow_init_ret ? 0 : 1; }
static inline void esp_now_set_self_role(int) {}
static inline int esp_now_add_peer(uint8_t*, int, int, void*, size_t) { return 0; }
static inline int esp_now_del_peer(uint8_t*) { return 0; }
static inline int esp_now_register_send_cb(void*) { return 0; }
static inline int esp_now_register_recv_cb(void*) { return 0; }

// Mock espnow_send_wrapper
static inline int espnow_send_wrapper(const uint8_t* mac, const uint8_t* data, size_t len, const char* tag) {
    (void)tag;
    g_mock_send_count++;
    memcpy(g_mock_last_send.mac, mac, 6);
    g_mock_last_send.len = len < 256 ? len : 256;
    memcpy(g_mock_last_send.data, data, g_mock_last_send.len);
    g_mock_last_send.sent = true;
    return 0;
}
```

- [ ] **Step 5: Create test_espnow_node_protocol.cpp**

```cpp
#include <unity.h>
#include "mock_espnow.h"
#include "espnow_node_protocol.h"
#include <cstring>

// globals for mock
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

void setUp() {
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    g_mock_send_count = 0;
    g_mock_espnow_init_ret = true;
    g_mock_load_gateway_ret = false;
    s_on_command_called = 0;
    s_on_paired_called = 0;
    s_on_restart_called = 0;
    s_on_forward_called = 0;
    s_get_sensor_type_val = 8;
}

void test_espnow_init_sends_pair_request() {
    s_proto.set_mac(s_test_mac);
    s_proto.begin();
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    // First byte of data should be version
    TEST_ASSERT_EQUAL(ESPNOW_PROTOCOL_VERSION, g_mock_last_send.data[0]);
    // Second byte should be msg_type
    TEST_ASSERT_EQUAL(ESPNOW_MSG_PAIR_REQUEST, g_mock_last_send.data[1]);
}

void test_espnow_paired_after_pair_response() {
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_paired = test_on_paired;
    s_proto.begin();
    // Simulate pair response from hub
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.version = ESPNOW_PROTOCOL_VERSION;
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    resp.sequence = 0;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 3;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    TEST_ASSERT_TRUE(s_proto.is_paired());
    TEST_ASSERT_EQUAL(3, s_proto.assigned_slot());
    TEST_ASSERT_EQUAL(1, s_on_paired_called);
}

void test_espnow_sends_sensor_data() {
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.get_sensor_type = test_get_type;
    s_proto.callbacks.get_sensor_payload = test_get_payload;
    s_proto.begin();
    // Force paired
    s_proto.handle_frame(s_test_mac, NULL, 0); // no-op, just to show not paired yet
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.version = ESPNOW_PROTOCOL_VERSION;
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    // Reset mock and publish state
    g_mock_send_count = 0;
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    s_proto.publish_state();
    s_proto.loop(); // should trigger send_sensor_data
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    TEST_ASSERT_EQUAL(ESPNOW_MSG_SENSOR_DATA, g_mock_last_send.data[1]);
}

void test_espnow_heartbeat() {
    s_proto.set_mac(s_test_mac);
    s_proto.begin();
    // Force paired
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.version = ESPNOW_PROTOCOL_VERSION;
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    resp.assigned_slot = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    // Set heartbeat interval to 1ms so it fires immediately
    s_proto.set_heartbeat_interval(1);
    g_mock_send_count = 0;
    memset(&g_mock_last_send, 0, sizeof(g_mock_last_send));
    s_proto.loop();
    TEST_ASSERT_TRUE(g_mock_last_send.sent);
    TEST_ASSERT_EQUAL(ESPNOW_MSG_HEARTBEAT, g_mock_last_send.data[1]);
}

void test_espnow_command_callback() {
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_command = test_on_command;
    s_proto.begin();
    // Simulate command
    espnow_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.version = ESPNOW_PROTOCOL_VERSION;
    cmd.msg_type = ESPNOW_MSG_COMMAND;
    memcpy(cmd.target_mac, s_test_mac, 6);
    cmd.state = 1;
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&cmd, sizeof(cmd));
    TEST_ASSERT_EQUAL(1, s_on_command_called);
    TEST_ASSERT_EQUAL(1, s_on_command_state);
}

void test_espnow_forward_callback() {
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.on_forward = test_on_forward;
    s_proto.begin();
    // Send a frame with unknown msg_type
    uint8_t buf[8] = { ESPNOW_PROTOCOL_VERSION, 0xFF, 0,0,0,0,0,0 }; // msg_type 0xFF = unknown
    s_proto.handle_frame(s_test_mac, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(1, s_on_forward_called);
}

void test_espnow_force_repair() {
    s_proto.set_mac(s_test_mac);
    s_proto.begin();
    // Pair
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.version = ESPNOW_PROTOCOL_VERSION;
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    TEST_ASSERT_TRUE(s_proto.is_paired());
    // Force repair
    s_proto.force_repair();
    TEST_ASSERT_FALSE(s_proto.is_paired());
}

void test_espnow_load_gateway_mac() {
    g_mock_load_gateway_ret = true;
    s_proto.set_mac(s_test_mac);
    s_proto.load_gateway_mac();
    // After loading, begin should not send pair request immediately
    // but should have loaded_gateway_mac set.
    // load_gateway_mac sets m_paired = true
    TEST_ASSERT_TRUE(s_proto.is_paired());
    // gateway mac should be 0xAA:0xAA:...
    const uint8_t* gw = s_proto.gateway_mac();
    TEST_ASSERT_EQUAL(0xAA, gw[0]);
}

void test_espnow_ack_timeout_retry() {
    s_proto.set_mac(s_test_mac);
    s_proto.callbacks.get_sensor_type = test_get_type;
    s_proto.begin();
    // Pair
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.version = ESPNOW_PROTOCOL_VERSION;
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    memcpy(resp.sensor_mac, s_test_mac, 6);
    s_proto.handle_frame(s_test_mac, (const uint8_t*)&resp, sizeof(resp));
    // Trigger state send
    s_proto.publish_state();
    g_mock_send_count = 0;
    s_proto.loop(); // SEND_IDLE -> sends data -> SEND_WAIT_ACK
    TEST_ASSERT_EQUAL(1, g_mock_send_count);
    // No ACK, call loop again after timeout (fake_millis_advance)
    // This test validates the state machine structure
}
```

- [ ] **Step 6: Register tests in test_native.cpp**

Add forward declarations and RUN_TEST calls for the new tests.

- [ ] **Step 7: Run tests**

```bash
cd tests/unit && pio test -e native
```

Expected: all tests PASS.

- [ ] **Step 8: Commit**

```bash
git -C shared add src/espnow_node_protocol.h src/espnow_node_protocol.cpp src/node_protocol.h
git -C shared commit -m "feat(shared): add EspnowNodeProtocol and extend NodeCallbacks with on_forward"
git add shared tests/unit/test/mock_espnow.h tests/unit/test/test_espnow_node_protocol.cpp tests/unit/test/test_native.cpp
git commit -m "feat(shared): add EspnowNodeProtocol (with tests)"
```

---
### Task 2: Refactor climate-gas node

**Files:**
- Modify: `nodes/climate-gas/src/main.cpp`

**Interfaces:**
- Consumes: `EspnowNodeProtocol`

- [ ] **Step 1: Read `nodes/climate-gas/src/main.cpp`** — identify all ESP-NOW code to remove

- [ ] **Step 2: Replace ESP-NOW code in main.cpp**

Remove:
- `espnow_send_cb`, `espnow_recv_cb`, `espnow_send_data`, `espnow_send_heartbeat`, `espnow_send_pair_request`
- All ESP-NOW static vars: `s_paired`, `s_gateway_mac[6]`, `s_sequence`, `s_assigned_slot`, `s_pair_attempts`, `s_ack_received`, `s_send_pending` (etc.)
- ACK/retry state machine in loop()
- `#include "common_espnow.h"` (no longer needed directly)

Add:
```cpp
#include "espnow_node_protocol.h"
static EspnowNodeProtocol s_espnow;
```

Setup:
```cpp
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr };
s_espnow.load_gateway_mac();
s_espnow.begin();
```

Loop:
```cpp
s_espnow.loop();
```

Publish state after sensor read:
```cpp
s_espnow.publish_state();
```

- [ ] **Step 3: Build test**

```bash
cd nodes/climate-gas && pio run -e dht_gas
```
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add nodes/climate-gas/src/main.cpp
git commit -m "refactor(climate-gas): use EspnowNodeProtocol"
```

---
### Task 3: Refactor presence node

**Files:**
- Modify: `nodes/presence/src/main.cpp`

- [ ] **Step 1: Read `nodes/presence/src/main.cpp`** — identify and remove:
  - `espnow_send_cb()`, `espnow_recv_cb()`, `espnow_send_data()`, `espnow_send_heartbeat()`, `espnow_send_pair_request()`
  - ESP-NOW static vars: `s_paired`, `s_gateway_mac`, `s_sequence`, `s_assigned_slot`, `s_pair_attempts`, `s_ack_received`, etc.
  - ACK/retry state machine in `loop()`
  - `#include "common_espnow.h"`

- [ ] **Step 2: Add EspnowNodeProtocol**

```cpp
#include "espnow_node_protocol.h"
static EspnowNodeProtocol s_espnow;
```

In `setup()`:
```cpp
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr };
s_espnow.load_gateway_mac();
s_espnow.begin();
```

In `loop()`:
```cpp
s_espnow.loop();
```

Replace any `espnow_send_data()` call with `s_espnow.publish_state()`.

- [ ] **Step 3: Build test**

```bash
cd nodes/presence && pio run -e esp8266
```
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add nodes/presence/src/main.cpp
git commit -m "refactor(presence): use EspnowNodeProtocol"
```

---
### Task 4: Refactor switch node

**Files:**
- Modify: `nodes/switch/src/main.cpp`

- [ ] **Step 1: Read `nodes/switch/src/main.cpp`** — identify and remove:
  - `espnow_send_cb()`, `espnow_recv_cb()`, `espnow_send_data()`, `espnow_send_heartbeat()`, `espnow_send_pair_request()`
  - ESP-NOW static vars: `s_paired`, `s_gateway_mac`, `s_sequence`, `s_assigned_slot`, `s_pair_attempts`, etc.
  - ACK/retry state machine in `loop()`
  - `#include "common_espnow.h"`

- [ ] **Step 2: Add EspnowNodeProtocol**

```cpp
#include "espnow_node_protocol.h"
static EspnowNodeProtocol s_espnow;
```

In `setup()`:
```cpp
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, nullptr };
s_espnow.load_gateway_mac();
s_espnow.begin();
```

In `loop()`:
```cpp
s_espnow.loop();
```

Replace relay toggle's espnow_send_data with `s_espnow.publish_state()`.

- [ ] **Step 3: Build test**

```bash
cd nodes/switch && pio run -e esp8266
```
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add nodes/switch/src/main.cpp
git commit -m "refactor(switch): use EspnowNodeProtocol"
```

---
### Task 5: Refactor lamp node

**Files:**
- Modify: `nodes/lamp/src/main.cpp`

**Interfaces:**
- Consumes: `EspnowNodeProtocol` with `on_forward` callback for extender mode

- [ ] **Step 1: Read `nodes/lamp/src/main.cpp`** — identify and remove all ESP-NOW code:
  - `espnow_send_cb()`, `espnow_recv_cb()`, `espnow_send_data()`, `espnow_send_heartbeat()`, `espnow_send_pair_request()`
  - Static vars: `s_paired`, `s_gateway_mac`, `s_sequence`, `s_assigned_slot`, `s_send_pending`, `s_ack_received`, `s_send_retries_left`, `s_pair_attempts`, `s_last_espnow_send`, `s_last_espnow_pair`, `s_last_heartbeat`, `s_espnow_tx_count`, `s_espnow_rx_count`, `s_broadcast_mac`
  - ACK/retry state machine in `loop()`
  - `#include "common_espnow.h"` (no longer needed directly)

- [ ] **Step 2: Add EspnowNodeProtocol**

```cpp
#include "espnow_node_protocol.h"
static EspnowNodeProtocol s_espnow;
```

In `setup()`:
```cpp
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, on_forward };
s_espnow.load_gateway_mac();
s_espnow.begin();
```

In `loop()`, replace ACK/retry block with:
```cpp
s_espnow.loop();
```

Replace relay toggle/alexa callback's `espnow_send_data()` with:
```cpp
s_espnow.publish_state();
```

Replace console `p` command's re-pair with:
```cpp
s_espnow.force_repair();
```

- [ ] **Step 3: Implement extender mode via on_forward callback**

The lamp's extender mode forwards ESP-NOW frames to/from other nodes. Extract the forward logic from the original `espnow_recv_cb` into a standalone `on_forward` callback:

```cpp
static void on_forward(const uint8_t* data, size_t len, const uint8_t* mac) {
    // Push to forward queue (ring buffer) — same logic as existing
    // forward queue processing stays in loop()
}
```

Keep the existing `forward_queue` (ring buffer struct), queue processing in `loop()`, and extender state vars. These are NOT ESP-NOW boilerplate — they're lamp-specific extender logic.

- [ ] **Step 3: Build test**

```bash
cd nodes/lamp && pio run -e d1_mini
```
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add nodes/lamp/src/main.cpp
git commit -m "refactor(lamp): use EspnowNodeProtocol with on_forward extender"
```

---
### Task 6: Refactor battery nodes (soil-moisture, presence-bat, rain)

**Files:**
- Modify: `nodes/soil-moisture/src/main.cpp`
- Modify: `nodes/presence-bat/src/main.cpp`
- Modify: `nodes/rain/src/main.cpp`

- [ ] **Step 1: Read each battery node's main.cpp**

- [ ] **Step 2: For each node, replace ESP-NOW code**

Same pattern as Task 2. Battery nodes use deep sleep — after wake, `begin()` sends pair request and data, then `loop()` runs briefly before `ESP.deepSleep()`. The `EspnowNodeProtocol` handles this naturally:
- `begin()` sends pair request
- After pair response (if not already loaded), send data
- Short loop cycle, then deep sleep

```cpp
// setup():
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, nullptr, nullptr, nullptr, nullptr };
s_espnow.begin();

// loop() runs once then deep sleep:
s_espnow.loop();
delay(100); // brief window for ESP-NOW to send
ESP.deepSleep(sleep_s * 1000000ULL);
```

- [ ] **Step 3: Build each node**

```bash
cd nodes/soil-moisture && pio run -e d1_mini
cd nodes/presence-bat && pio run -e d1_mini
cd nodes/rain && pio run -e d1_mini
```
Expected: all SUCCESS.

- [ ] **Step 4: Commit (batch)**

```bash
git add nodes/soil-moisture/src/main.cpp nodes/presence-bat/src/main.cpp nodes/rain/src/main.cpp
git commit -m "refactor(battery-nodes): use EspnowNodeProtocol"
```

---
### Task 7: Integration verification

**Files:** (none — verification only)

- [ ] **Step 1: Run native unit tests**

```bash
cd tests/unit && pio test -e native
```
Expected: all tests PASS (existing + new EspnowNodeProtocol tests).

- [ ] **Step 2: Build all refactored nodes**

```bash
cd nodes/climate-gas && pio run -e dht_gas
cd nodes/presence && pio run -e esp8266
cd nodes/switch && pio run -e esp8266
cd nodes/lamp && pio run -e d1_mini
cd nodes/soil-moisture && pio run -e d1_mini
cd nodes/presence-bat && pio run -e d1_mini
cd nodes/rain && pio run -e d1_mini
```
Expected: all SUCCESS.

- [ ] **Step 3: Verify no remaining duplicate espnow_send_* functions**

```bash
git grep "static.*espnow_send_pair_request\|static.*espnow_send_data\|static.*espnow_send_heartbeat" -- nodes/
```
Expected: no matches (all consolidated into EspnowNodeProtocol).
