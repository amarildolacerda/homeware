# Refactor LoRa Radio: Shared Abstractions

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extrair `LoraSpiRadio` e `LoraNodeProtocol` para `shared/`, refatorar hub e onoff-lora para usá-los sem mudar comportamento.

**Architecture:** O código LoRa existe em 2 lugares (hub LoraHandler, node onoff-lora main.cpp). Ambos fazem SPI begin + LoRa.begin + polling RX. O node ainda tem protocolo (pairing/heartbeat) inline. Vamos extrair: (1) `LoraSpiRadio` — transporte SPI genérico, (2) `NodeProtocol` — abstrato, (3) `LoraNodeProtocol` — protocolo node. `main.cpp` de ambos fala só com abstrações.

**Tech Stack:** C++17, PlatformIO, sandeepmistry/LoRa, Unity test framework

## Global Constraints

- Zero comportamento alterado — refatoração pura
- `shared/src/` é submodule — commits no branch dev do shared
- `nodes/onoff-lora` é "em desenvolvimento" (pode modificar)
- `hub/` é estável — refatorar com cuidado, build test obrigatório
- `-DLORA_DEVICE` desativa include de `<esp_now.h>` — LoraSpiRadio precisa
- `lora_protocol.h` já existe em shared/ — não modificar
- `radio_interface.h` já existe em shared/ — não modificar
- Nomes: `LoraSpiRadio` (transporte), `NodeProtocol` (abstrato), `LoraNodeProtocol` (protocolo node)

---

### Task 1: test infrastructure + mock

**Files:**
- Create: `tests/unit/platformio.ini`
- Create: `tests/unit/test_helper.h`
- Create: `tests/unit/test_native.cpp`

**Interfaces:**
- Produces: millis() stub, MockRadio (implements RadioInterface), native test env

- [ ] **Step 1: Criar test helper com mock RadioInterface e millis()**

```cpp
// tests/unit/test_helper.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>   // memset, memcpy

// Stub millis() para testes
static unsigned long s_fake_millis = 0;
unsigned long millis() { return s_fake_millis; }
void fake_millis_set(unsigned long t) { s_fake_millis = t; }
void fake_millis_advance(unsigned long delta) { s_fake_millis += delta; }

// RadioInterface (mesmo caminho que shared/ usa)
#include "radio_interface.h"

// MockRadio: implementa RadioInterface, grava sends p/ inspeção
class MockRadio : public RadioInterface {
public:
    bool m_ready = true;
    int m_init_ret = 0;
    uint8_t m_last_sent[256];
    size_t m_last_sent_len = 0;
    int m_send_ret = 0;
    bool m_init_called = false;

    int init() override { m_init_called = true; return m_init_ret; }
    int send(const uint8_t* data, size_t len) override {
        m_last_sent_len = len < 256 ? len : 256;
        memcpy(m_last_sent, data, m_last_sent_len);
        return m_send_ret;
    }
    void loop() override {}
    bool is_ready() const override { return m_ready; }

    // Helper para injetar pacotes RX como se o rádio tivesse recebido
    void inject_rx(const uint8_t* data, size_t len, int16_t rssi) {
        if (m_rx_cb) m_rx_cb(data, len, rssi, m_rx_arg);
    }
};
```

- [ ] **Step 2: Criar platformio.ini para native testing**

```ini
; tests/unit/platformio.ini
[platformio]
default_envs = native

[env:native]
platform = native
build_flags =
    -I../../shared/src
    -DLORA_DEVICE
test_framework = unity
; exclui arquivos que dependem de Arduino.h
; só compila o que for explicitamente incluído no teste
lib_ldf_mode = off
```

- [ ] **Step 3: Criar test_native.cpp vazio que compila**

```cpp
// tests/unit/test_native.cpp
#include <unity.h>
#include "test_helper.h"

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    UNITY_END();
    return 0;
}
```

- [ ] **Step 4: Build test — compila native OK**

```bash
cd tests/unit && pio test -e native
```
Expected: builds and runs, no tests yet.

- [ ] **Step 5: Commit**
```
git add tests/unit/
git commit -m "test: add native test infrastructure with MockRadio"
```

---

### Task 2: Create `node_protocol.h` (abstract base)

**Files:**
- Create: `shared/src/node_protocol.h`

**Interfaces:**
- Produces: `class NodeProtocol` with pure virtual `begin()`, `loop()`, `is_paired()`, `assigned_slot()`, `force_repair()` + `NodeCallbacks` struct

- [ ] **Step 1: Write the file**

```cpp
// shared/src/node_protocol.h
#ifndef HW_SHARED_NODE_PROTOCOL_H
#define HW_SHARED_NODE_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

struct NodeCallbacks {
    uint8_t (*get_sensor_type)();
    uint8_t (*get_sensor_payload)(uint8_t* buf, uint8_t max_len);
    void    (*on_command)(uint8_t command);
    void    (*on_paired)(uint8_t slot);
    void    (*on_restart)();
};

class NodeProtocol {
public:
    virtual ~NodeProtocol() {}
    virtual void begin() = 0;
    virtual void loop() = 0;
    virtual bool is_paired() const = 0;
    virtual uint8_t assigned_slot() const = 0;
    virtual void force_repair() = 0;
    NodeCallbacks callbacks;
};

#endif
```

- [ ] **Step 2: Build test — compila native**

```bash
cd tests/unit && pio test -e native
```
Expected: builds OK.

- [ ] **Step 3: Commit**
```
git add shared/src/node_protocol.h
git commit -m "feat(shared): add NodeProtocol abstract base class"
```

---

### Task 3: Create `LoraSpiRadio` (transport, extracted from hub LoraHandler)

**Files:**
- Create: `shared/src/lora_spi_radio.h`
- Create: `shared/src/lora_spi_radio.cpp`
- Test: `tests/unit/test_lora_spi_radio.cpp` (compile test + static_assert)

**Interfaces:**
- Consumes: `RadioInterface` (radio_interface.h), `LoraSpiConfig` struct
- Produces: `class LoraSpiRadio : public RadioInterface`

- [ ] **Step 1: Write lora_spi_radio.h**

```cpp
// shared/src/lora_spi_radio.h
#ifndef HW_SHARED_LORA_SPI_RADIO_H
#define HW_SHARED_LORA_SPI_RADIO_H

#include "radio_interface.h"
#include <stdint.h>
#include <stddef.h>

struct LoraSpiConfig {
    int8_t ss    = 18;
    int8_t rst   = 14;
    int8_t dio0  = -1;
    int8_t sck   = 5;
    int8_t miso  = 19;
    int8_t mosi  = 27;
    float  freq  = 868.0;
    uint8_t sf   = 10;
    float  bw    = 125E3;
    uint8_t cr   = 7;
    uint8_t preamble = 8;
    int8_t  tx_power = 17;
};

class LoraSpiRadio : public RadioInterface {
public:
    LoraSpiRadio(const LoraSpiConfig& cfg);
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
private:
    LoraSpiConfig m_cfg;
    bool m_ok = false;
    uint8_t m_rx_buf[256];
    int m_rx_len = 0;
    void handle_rx();
};

#endif
```

- [ ] **Step 2: Write lora_spi_radio.cpp**

```cpp
// shared/src/lora_spi_radio.cpp
#ifdef LORA_DEVICE

#include "lora_spi_radio.h"
#include <LoRa.h>
#include <SPI.h>

LoraSpiRadio::LoraSpiRadio(const LoraSpiConfig& cfg) : m_cfg(cfg) {}

int LoraSpiRadio::init() {
    SPI.begin(m_cfg.sck, m_cfg.miso, m_cfg.mosi, m_cfg.ss);
    LoRa.setPins(m_cfg.ss, m_cfg.rst, m_cfg.dio0);
    if (!LoRa.begin(m_cfg.freq * 1E6)) return -1;
    LoRa.setSpreadingFactor(m_cfg.sf);
    LoRa.setSignalBandwidth(m_cfg.bw);
    LoRa.setCodingRate4(m_cfg.cr);
    LoRa.setTxPower(m_cfg.tx_power);
    LoRa.setPreambleLength(m_cfg.preamble);
    LoRa.receive();
    m_ok = true;
    return 0;
}

int LoraSpiRadio::send(const uint8_t* data, size_t len) {
    if (!m_ok) return -1;
    LoRa.beginPacket();
    LoRa.write(data, len);
    int ret = LoRa.endPacket() ? 0 : -1;
    LoRa.receive();
    return ret;
}

void LoraSpiRadio::loop() {
    if (!m_ok) return;
    handle_rx();
}

bool LoraSpiRadio::is_ready() const {
    return m_ok;
}

void LoraSpiRadio::handle_rx() {
    int len = LoRa.parsePacket();
    if (len <= 0 || len > (int)sizeof(m_rx_buf)) return;
    int i = 0;
    while (LoRa.available() && i < (int)sizeof(m_rx_buf)) {
        m_rx_buf[i++] = LoRa.read();
    }
    m_rx_len = i;
    int16_t rssi = LoRa.packetRssi();
    if (m_rx_len >= 11 && m_rx_cb) {
        m_rx_cb(m_rx_buf, m_rx_len, rssi, m_rx_arg);
    }
}

#endif // LORA_DEVICE
```

Note: `11` é `LORA_HEADER_SIZE` de `lora_protocol.h`. Para evitar dependência circular, usar valor literal ou incluir o header. Vou incluir `lora_protocol.h`:

```cpp
#include "lora_protocol.h"
// ...
if (m_rx_len >= LORA_HEADER_SIZE && m_rx_cb) {
```

- [ ] **Step 3: Compile test for lora_spi_radio (mock LoRa.h)**

Criar mock minimal para native test:
```cpp
// tests/unit/mock_lora.h
#pragma once
#include <stdint.h>
#include <stddef.h>

class LoRaClass {
public:
    void setPins(int8_t ss, int8_t rst, int8_t dio0) {}
    bool begin(long freq) { return true; }
    void setSpreadingFactor(int sf) {}
    void setSignalBandwidth(long bw) {}
    void setCodingRate4(int cr) {}
    void setTxPower(int txPower) {}
    void setPreambleLength(long len) {}
    void receive() {}
    int beginPacket() { return 1; }
    size_t write(const uint8_t* buf, size_t len) { return len; }
    int endPacket(bool async = false) { return 1; }
    int parsePacket(int size = 0) { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int packetRssi() { return 0; }
};
extern LoRaClass LoRa;

class SPIClass {
public:
    void begin(int sck, int miso, int mosi, int ss) {}
};
extern SPIClass SPI;
```

- [ ] **Step 4: Write test that verifies LoraSpiRadio compiles and struct sizes**

```cpp
// tests/unit/test_lora_spi_radio.cpp
#include <unity.h>
#include "mock_lora.h"
#include "lora_spi_radio.h"

void test_lora_spi_config_size() {
    TEST_ASSERT_EQUAL(18, sizeof(LoraSpiConfig));
}

void test_lora_spi_radio_defaults() {
    LoraSpiConfig cfg;
    TEST_ASSERT_EQUAL(18, cfg.ss);
    TEST_ASSERT_EQUAL(14, cfg.rst);
    TEST_ASSERT_EQUAL(868.0f, cfg.freq);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lora_spi_config_size);
    RUN_TEST(test_lora_spi_radio_defaults);
    UNITY_END();
    return 0;
}
```

- [ ] **Step 5: Run test**

```bash
cd tests/unit && pio test -e native
```
Expected: PASS.

- [ ] **Step 6: Commit**
```
git add shared/src/lora_spi_radio.h shared/src/lora_spi_radio.cpp tests/unit/mock_lora.h tests/unit/test_lora_spi_radio.cpp
git commit -m "feat(shared): add LoraSpiRadio transport class"
```

---

### Task 4: Create `LoraNodeProtocol` (node state machine, extracted from onoff-lora main.cpp)

**Files:**
- Create: `shared/src/lora_node_protocol.h`
- Create: `shared/src/lora_node_protocol.cpp`
- Test: `tests/unit/test_lora_node_protocol.cpp`

**Interfaces:**
- Consumes: `RadioInterface*`, `NodeCallbacks`, `lora_protocol.h`
- Produces: `class LoraNodeProtocol : public NodeProtocol`

- [ ] **Step 1: Write the failing test (TDD: test protocol behavior first)**

```cpp
// tests/unit/test_lora_node_protocol.cpp
#include <unity.h>
#include "test_helper.h"
#include "lora_protocol.h"
#include "lora_node_protocol.h"

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
    TEST_ASSERT_EQUAL(LORA_MSG_PAIR_REQUEST, frame->msg_type);
}

void test_node_protocol_sends_heartbeat_when_paired() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.begin();
    fake_millis_advance(10);
    mock.m_last_sent_len = 0;

    // Simula PAIR_RESPONSE para ficar paired
    lora_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = LORA_MSG_PAIR_RESPONSE;
    resp.assigned_slot = 1;
    memcpy(resp.sensor_id, s_test_mac, 6);
    mock.inject_rx((const uint8_t*)&resp, sizeof(resp), -50);

    mock.m_last_sent_len = 0;
    fake_millis_advance(60000);
    proto.loop();

    TEST_ASSERT(mock.m_last_sent_len > 0);
    lora_frame_t* frame = (lora_frame_t*)mock.m_last_sent;
    TEST_ASSERT_EQUAL(LORA_MSG_HEARTBEAT, frame->msg_type);
}

void test_node_protocol_pair_response() {
    MockRadio mock;
    LoraNodeProtocol proto(&mock);
    setup_proto(proto);

    proto.begin();
    fake_millis_advance(10);

    lora_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = LORA_MSG_PAIR_RESPONSE;
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
    fake_millis_advance(10);

    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.command = 1;
    memcpy(cmd.sensor_id, s_test_mac, 6);
    mock.inject_rx((const uint8_t*)&cmd, sizeof(cmd), -50);

    TEST_ASSERT_EQUAL(1, s_last_command);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_node_protocol_sends_pair_request_on_begin);
    RUN_TEST(test_node_protocol_sends_heartbeat_when_paired);
    RUN_TEST(test_node_protocol_pair_response);
    RUN_TEST(test_node_protocol_handles_command);
    UNITY_END();
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd tests/unit && pio test -e native
```
Expected: build error (lora_node_protocol.h not found).

- [ ] **Step 3: Write lora_node_protocol.h**

```cpp
#ifndef HW_SHARED_LORA_NODE_PROTOCOL_H
#define HW_SHARED_LORA_NODE_PROTOCOL_H

#include "node_protocol.h"
#include "radio_interface.h"
#include <stdint.h>

class LoraNodeProtocol : public NodeProtocol {
public:
    LoraNodeProtocol(RadioInterface* radio);

    void begin() override;
    void loop() override;
    bool is_paired() const override { return m_paired; }
    uint8_t assigned_slot() const override { return m_slot; }
    void force_repair() override;

    void set_mac(const uint8_t* mac);
    void set_pair_interval(unsigned long ms) { m_pair_interval_ms = ms; }
    void set_heartbeat_interval(unsigned long ms) { m_heartbeat_interval_ms = ms; }
    void set_state_interval(unsigned long ms) { m_state_interval_ms = ms; }

    void set_device_name(const char* name);

private:
    RadioInterface* m_radio;
    uint8_t m_mac[6];
    bool m_paired;
    uint8_t m_slot;
    char m_device_name[32];
    uint16_t m_sequence;
    unsigned long m_pair_interval_ms;
    unsigned long m_heartbeat_interval_ms;
    unsigned long m_state_interval_ms;
    unsigned long m_last_pair_ms;
    unsigned long m_last_heartbeat_ms;
    unsigned long m_last_state_ms;
    uint8_t m_pair_attempts;

    void send_pair_request();
    void send_sensor_data();
    void send_heartbeat();
    void handle_frame(const uint8_t* data, size_t len, int16_t rssi);
    static void rx_cb_wrapper(const uint8_t* data, size_t len, int16_t rssi, void* arg);
};

#endif
```

- [ ] **Step 4: Write lora_node_protocol.cpp**

```cpp
#include "lora_node_protocol.h"
#include "lora_protocol.h"
#include <string.h>

static unsigned long s_default_pair_ms = 5000;
static unsigned long s_default_heartbeat_ms = 60000;
static unsigned long s_default_state_ms = 60000;
static uint8_t s_max_pair_attempts = 20;

LoraNodeProtocol::LoraNodeProtocol(RadioInterface* radio)
    : m_radio(radio)
    , m_paired(false)
    , m_slot(0)
    , m_sequence(0)
    , m_pair_interval_ms(s_default_pair_ms)
    , m_heartbeat_interval_ms(s_default_heartbeat_ms)
    , m_state_interval_ms(s_default_state_ms)
    , m_last_pair_ms(0)
    , m_last_heartbeat_ms(0)
    , m_last_state_ms(0)
    , m_pair_attempts(0)
{
    memset(m_mac, 0, sizeof(m_mac));
    memset(m_device_name, 0, sizeof(m_device_name));
}

void LoraNodeProtocol::set_mac(const uint8_t* mac) {
    memcpy(m_mac, mac, 6);
}

void LoraNodeProtocol::set_device_name(const char* name) {
    strncpy(m_device_name, name, sizeof(m_device_name) - 1);
    m_device_name[sizeof(m_device_name) - 1] = '\0';
}

void LoraNodeProtocol::begin() {
    m_paired = false;
    m_pair_attempts = 0;
    m_last_pair_ms = 0;
    m_last_heartbeat_ms = 0;
    m_last_state_ms = 0;
    m_sequence = 0;
    m_radio->set_rx_callback(rx_cb_wrapper, this);
}

void LoraNodeProtocol::loop() {
    unsigned long now = millis();

    if (!m_paired) {
        if (now - m_last_pair_ms >= m_pair_interval_ms &&
            m_pair_attempts < s_max_pair_attempts) {
            m_last_pair_ms = now;
            m_pair_attempts++;
            send_pair_request();
        }
    } else {
        if (now - m_last_heartbeat_ms >= m_heartbeat_interval_ms) {
            m_last_heartbeat_ms = now;
            send_heartbeat();
        }
        if (now - m_last_state_ms >= m_state_interval_ms) {
            m_last_state_ms = now;
            send_sensor_data();
        }
    }
}

void LoraNodeProtocol::force_repair() {
    m_paired = false;
    m_pair_attempts = 0;
    m_last_pair_ms = 0;
}

void LoraNodeProtocol::send_pair_request() {
    lora_pair_request_t req;
    memset(&req, 0, sizeof(req));
    req.msg_type = LORA_MSG_PAIR_REQUEST;
    req.sequence = m_sequence++;
    memcpy(req.sensor_id, m_mac, 6);
    req.payload_len = 1;
    req.sensor_type = callbacks.get_sensor_type ? callbacks.get_sensor_type() : 0;
    strncpy(req.device_name, m_device_name, sizeof(req.device_name) - 1);
    req.device_name[sizeof(req.device_name) - 1] = '\0';
    m_radio->send((const uint8_t*)&req, sizeof(req));
}

void LoraNodeProtocol::send_sensor_data() {
    uint8_t payload[LORA_MAX_PAYLOAD];
    uint8_t payload_len = 0;
    if (callbacks.get_sensor_payload) {
        payload_len = callbacks.get_sensor_payload(payload, LORA_MAX_PAYLOAD);
    }
    uint8_t buf[LORA_HEADER_SIZE + payload_len];
    lora_frame_t* frame = (lora_frame_t*)buf;
    frame->msg_type = LORA_MSG_SENSOR_DATA;
    frame->sequence = m_sequence++;
    memcpy(frame->sensor_id, m_mac, 6);
    frame->rssi = 0;
    frame->payload_len = payload_len;
    if (payload_len > 0) memcpy(frame->payload, payload, payload_len);
    m_radio->send(buf, LORA_HEADER_SIZE + payload_len);
}

void LoraNodeProtocol::send_heartbeat() {
    uint8_t buf[LORA_HEADER_SIZE];
    lora_frame_t* frame = (lora_frame_t*)buf;
    frame->msg_type = LORA_MSG_HEARTBEAT;
    frame->sequence = m_sequence++;
    memcpy(frame->sensor_id, m_mac, 6);
    frame->rssi = 0;
    frame->payload_len = 0;
    m_radio->send(buf, LORA_HEADER_SIZE);
}

void LoraNodeProtocol::handle_frame(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    (void)rssi;
    if (len < LORA_HEADER_SIZE) return;
    const lora_frame_t* frame = (const lora_frame_t*)data;

    if (frame->msg_type == LORA_MSG_PAIR_RESPONSE) {
        if (len >= sizeof(lora_pair_response_t)) {
            const lora_pair_response_t* resp = (const lora_pair_response_t*)data;
            if (memcmp(resp->sensor_id, m_mac, 6) == 0) {
                m_paired = true;
                m_slot = resp->assigned_slot;
                m_pair_attempts = 0;
                if (callbacks.on_paired) callbacks.on_paired(m_slot);
                send_sensor_data();
            }
        }
    } else if (frame->msg_type == LORA_MSG_COMMAND) {
        if (len >= sizeof(lora_command_t)) {
            const lora_command_t* cmd = (const lora_command_t*)data;
            if (memcmp(cmd->sensor_id, m_mac, 6) == 0) {
                if (cmd->command == 0xFF) {
                    if (callbacks.on_restart) callbacks.on_restart();
                } else {
                    if (callbacks.on_command) callbacks.on_command(cmd->command);
                }
            }
        }
    }
}

void LoraNodeProtocol::rx_cb_wrapper(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    LoraNodeProtocol* self = (LoraNodeProtocol*)arg;
    self->handle_frame(data, len, rssi, arg);
}
```

- [ ] **Step 5: Add mac setter to test and run tests**

Preciso setar o mac no node protocol para os testes de COMMAND e PAIR_RESPONSE funcionarem:
Adicionar no teste:

```cpp
void setUp() {
    // ...
    uint8_t test_mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    proto.set_mac(test_mac);
}
```

- [ ] **Step 6: Run test to verify passes**

```bash
cd tests/unit && pio test -e native
```
Expected: all 4 tests PASS.

- [ ] **Step 7: Commit**
```
git add shared/src/lora_node_protocol.h shared/src/lora_node_protocol.cpp tests/unit/test_lora_node_protocol.cpp
git commit -m "feat(shared): add LoraNodeProtocol with pairing/heartbeat/sensor_data"
```

---

### Task 5: Refactor hub `lora_handler.*` to use LoraSpiRadio

**Files:**
- Modify: `hub/src/lora_handler.h`
- Modify: `hub/src/lora_handler.cpp`
- Modify: `hub/src/main.cpp`

**Interfaces:**
- Consumes: `LoraSpiRadio` from shared/

- [ ] **Step 1: Rewrite lora_handler.h as thin wrapper around LoraSpiRadio**

```cpp
#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include "radio_interface.h"
#include "lora_spi_radio.h"
#include "lora_protocol.h"

class LoraHandler : public RadioInterface {
public:
    LoraHandler();
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;
private:
    LoraSpiRadio m_radio;
};

#endif
```

- [ ] **Step 2: Rewrite lora_handler.cpp**

```cpp
#ifdef HABILITA_LORA

#include "lora_handler.h"
#include "lora_config.h"

LoraHandler::LoraHandler()
    : m_radio(LoraSpiConfig{
        .ss = LORA_SS,
        .rst = LORA_RST,
        .dio0 = LORA_DIO0,
        .sck = LORA_SCK,
        .miso = LORA_MISO,
        .mosi = LORA_MOSI,
        .freq = LORA_FREQ,
        .sf = LORA_SF,
        .bw = LORA_BW * 1E3f,
        .cr = LORA_CR,
        .preamble = LORA_PREAMBLE,
        .tx_power = LORA_TX_POWER,
    })
{}

int LoraHandler::init() { return m_radio.init(); }
int LoraHandler::send(const uint8_t* data, size_t len) { return m_radio.send(data, len); }
void LoraHandler::loop() { m_radio.loop(); }
bool LoraHandler::is_ready() const { return m_radio.is_ready(); }

bool LoraHandler::send_command(const uint8_t* mac, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

bool LoraHandler::send_restart(const uint8_t* mac) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = 0xFF;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

#endif
```

- [ ] **Step 3: Build test hub_32_lora**

```bash
cd hub && pio run -e hub_32_lora
```
Expected: compiles sem erros.

- [ ] **Step 4: Commit**
```
git add hub/src/lora_handler.h hub/src/lora_handler.cpp
git commit -m "refactor(hub): LoraHandler wraps LoraSpiRadio from shared/"
```

---

### Task 6: Refactor `onoff-lora` to use LoraSpiRadio + LoraNodeProtocol

**Files:**
- Modify: `nodes/onoff-lora/src/main.cpp`

**Interfaces:**
- Consumes: `LoraSpiRadio`, `LoraNodeProtocol` from shared/

- [ ] **Step 1: Replace inline LoRa functions with LoraSpiRadio + LoraNodeProtocol**

Substituir as linhas 30-37 (LORA_ defines) e 129-215 (lora functions) no main.cpp:

Remover:
```cpp
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  -1
#define LORA_SCK    5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_FREQ  868.0
```

Remover funções: `lora_send()`, `lora_send_pair_request()`, `lora_send_state()`, `lora_send_heartbeat()`, `handle_lora_rx()`, `lora_init()`.

Adicionar includes:
```cpp
#include "lora_spi_radio.h"
#include "lora_node_protocol.h"
```

Adicionar globals:
```cpp
static LoraSpiConfig s_lora_cfg = {
    .ss = 18, .rst = 14, .dio0 = -1,
    .sck = 5, .miso = 19, .mosi = 27,
    .freq = 868.0, .sf = 10, .bw = 125E3,
    .cr = 7, .preamble = 8, .tx_power = 17,
};
static LoraSpiRadio s_radio(s_lora_cfg);
static LoraNodeProtocol s_proto(&s_radio);
```

Remover variáveis LoRa que agora estão no protocolo:
```cpp
// REMOVER:
// static bool s_paired = false;
// static uint16_t s_seq = 0;
// static unsigned long s_last_heartbeat = 0;
// static unsigned long s_last_state_send = 0;
// static unsigned long s_last_pair_attempt = 0;
// static int s_pair_attempts = 0;
```

Substituir `lora_init()` por `s_radio.init()` no setup.

Substituir lógica LoRa no loop() por `s_radio.loop()` + `s_proto.loop()`.

O `set_relay()` chama `lora_send_state()` → substituir por `s_proto.force_repair()`? Não, `set_relay` deve notificar o hub imediatamente. Como `LoraNodeProtocol` não tem "send state now", precisamos de um método `send_state_now()` ou chamar o callback manualmente.

Na verdade, o `set_relay` chama `lora_send_state()` para enviar o estado atualizado imediatamente. A `LoraNodeProtocol` só envia periodicamente. Precisamos de um método `publish_state()`.

Adicionar em `LoraNodeProtocol`:
```cpp
void publish_state(); // envia estado imediatamente
```

E implementar:
```cpp
void LoraNodeProtocol::publish_state() {
    m_last_state_ms = millis();
    send_sensor_data();
}
```

O set_relay vira:
```cpp
static void set_relay(bool state) {
    s_relay = state;
    digitalWrite(RELAY_PIN, state ? RELAY_ON : !RELAY_ON);
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_RELAY_STATE, state);
    EEPROM.commit();
    EEPROM.end();
    s_proto.publish_state();
    console.printf("Relay set to %s\n", state ? "ON" : "OFF");
}
```

- [ ] **Step 2: Build test**

```bash
cd nodes/onoff-lora && pio run -e lora_esp32
```
Expected: compiles sem erros.

- [ ] **Step 3: Verify on device (flash + test basic functions)**

Opcional neste momento — o build test já garante que a compilação funciona. O teste funcional vira validação adicional.

- [ ] **Step 4: Commit**
```
git add nodes/onoff-lora/src/main.cpp
git commit -m "refactor(onoff-lora): use LoraSpiRadio + LoraNodeProtocol from shared"
```

---

### Task 7: Full integration test

- [ ] **Step 1: Build all affected envs**

```bash
cd hub && pio run -e hub_32_lora && pio run -e hub_32_lora_heltec
cd nodes/onoff-lora && pio run -e lora_esp32
```
Expected: all build successfully.

- [ ] **Step 2: Run native unit tests**

```bash
cd tests/unit && pio test -e native
```
Expected: all tests pass.

- [ ] **Step 3: Final commit with summary**
```
git add -A
git commit -m "refactor: unify LoRa radio into shared abstractions

- node_protocol.h: abstract node protocol base
- lora_spi_radio.h/.cpp: SPI LoRa transport (shared)
- lora_node_protocol.h/.cpp: node state machine (shared)
- hub/lora_handler.*: thin wrapper around LoraSpiRadio
- onoff-lora: uses LoraSpiRadio + LoraNodeProtocol

Tests: native unit tests for LoraNodeProtocol (pairing, heartbeat,
sensor_data, commands) and LoraSpiRadio (config, struct sizes).
All build targets verified."
```
