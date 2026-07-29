# RadioInterface Abstraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor hub radio layer so ESP-NOW can be compiled out and future radios can be added via RadioInterface + RadioManager.

**Architecture:** `RadioInterface` (abstract, shared/) gains optional virtual methods for pairing/stats/commands. `EspnowHandler : public RadioInterface` replaces flat functions. `RadioManager` (new) registers radio instances and provides agnostic dispatch for init/loop/command/stats.

**Tech Stack:** C++17, PlatformIO (espressif8266, espressif32), ESP-NOW, sandeepmistry/LoRa

## Global Constraints

- `EspnowHandler` lives in `hub/include/espnow_handler.h` and `hub/src/espnow_handler.cpp`
- `LoraHandler` lives in `hub/src/lora_handler.h` and `hub/src/lora_handler.cpp`
- `radio_interface.h` lives in `shared/src/radio_interface.h` (submodule)
- All ESP-NOW code guarded by `#ifdef HABILITA_ESPNOW`
- Existing `hub_8266` / `hub_32` / `hub_32c3` / `hub_32_ota` builds must continue working with ESP-NOW enabled
- After each task, verify with `pio run -e <env>` for at least `hub_8266` and `hub_32`
- Commit shared submodule separately, then update parent pointer

---

### Task 1: Expand RadioInterface with optional virtual methods

**Files:**
- Modify: `shared/src/radio_interface.h`

**Interfaces:**
- Consumes: current `RadioInterface` (init/send/loop/is_ready/rx_callback)
- Produces: `RadioInterface` with virtual `send_command`, `send_restart`, `get_rx_count`, `get_ack_count`, `get_crc_errors`, `start_pairing`, `stop_pairing`, `is_pairing`, `pairing_remaining_ms`, `get_radio_mac`, `announce`, `broadcast_time_sync`

- [ ] **Step 1: Add virtual methods to radio_interface.h**

```cpp
#ifndef HW_SHARED_RADIO_INTERFACE_H
#define HW_SHARED_RADIO_INTERFACE_H

#include <stdint.h>
#include <stddef.h>

class RadioInterface {
public:
    virtual ~RadioInterface() {}

    virtual int init() = 0;
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual void loop() = 0;
    virtual bool is_ready() const = 0;

    using rx_callback_t = void (*)(const uint8_t* data, size_t len,
                                    int16_t rssi, void* arg);

    void set_rx_callback(rx_callback_t cb, void* arg = nullptr) {
        m_rx_cb = cb;
        m_rx_arg = arg;
    }

    // Optional operations with safe defaults
    virtual bool send_command(const uint8_t* mac, uint8_t state)
        { (void)mac; (void)state; return false; }
    virtual bool send_restart(const uint8_t* mac)
        { (void)mac; return false; }
    virtual unsigned long get_rx_count() const { return 0; }
    virtual unsigned long get_ack_count() const { return 0; }
    virtual unsigned long get_crc_errors() const { return 0; }
    virtual bool start_pairing() { return false; }
    virtual void stop_pairing() {}
    virtual bool is_pairing() const { return false; }
    virtual unsigned long pairing_remaining_ms() const { return 0; }
    virtual uint8_t* get_radio_mac() { return nullptr; }
    virtual void announce() {}
    virtual void broadcast_time_sync(uint32_t epoch) { (void)epoch; }

protected:
    rx_callback_t m_rx_cb = nullptr;
    void* m_rx_arg = nullptr;
};

#endif
```

- [ ] **Step 2: Commit shared submodule**

```bash
cd shared
git add src/radio_interface.h
git commit -m "feat(shared): expand RadioInterface with optional virtual methods"
```

- [ ] **Step 3: Update parent repo pointer**

```bash
cd ..
git add shared
git commit -m "chore(shared): bump for RadioInterface expansion"
```

---

### Task 2: Create RadioManager

**Files:**
- Create: `hub/include/radio_manager.h`
- Create: `hub/src/radio_manager.cpp`
- Requires: Task 1 (expanded RadioInterface)

**Interfaces:**
- Consumes: `RadioInterface` (expanded), `sensor_registry.h` (for `radio_type_t` in `virtual_sensor_t`)
- Produces: `RadioManager` singleton with `add_radio()`, `init_all()`, `loop_all()`, `send_command()`, `send_restart()`, `total_rx_count()`, `total_ack_count()`, `total_crc_errors()`, `any_pairing_active()`, `any_start_pairing()`, `all_stop_pairing()`, `all_announce()`, `all_broadcast_time_sync()`, `get_radio()`

- [ ] **Step 1: Create radio_manager.h**

```cpp
#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include "radio_interface.h"
#include <stdint.h>

#define MAX_RADIOS 4

class RadioManager {
public:
    void add_radio(uint8_t radio_type, RadioInterface* radio);

    void init_all();
    void loop_all();

    bool send_command(uint8_t slot, uint8_t state);
    bool send_restart(uint8_t slot);

    unsigned long total_rx_count() const;
    unsigned long total_ack_count() const;
    unsigned long total_crc_errors() const;

    bool any_pairing_active() const;
    bool any_start_pairing();
    void all_stop_pairing();
    void all_announce();
    void all_broadcast_time_sync(uint32_t epoch);

    RadioInterface* get_radio(uint8_t radio_type) const;

private:
    struct RadioEntry {
        uint8_t type;
        RadioInterface* radio;
    };
    RadioEntry m_entries[MAX_RADIOS];
    int m_count = 0;
};

#endif
```

- [ ] **Step 2: Create radio_manager.cpp**

```cpp
#include "radio_manager.h"
#include "sensor_registry.h"

void RadioManager::add_radio(uint8_t radio_type, RadioInterface* radio) {
    if (m_count >= MAX_RADIOS) return;
    m_entries[m_count].type = radio_type;
    m_entries[m_count].radio = radio;
    m_count++;
}

void RadioManager::init_all() {
    for (int i = 0; i < m_count; i++) {
        m_entries[i].radio->init();
    }
}

void RadioManager::loop_all() {
    for (int i = 0; i < m_count; i++) {
        m_entries[i].radio->loop();
    }
}

bool RadioManager::send_command(uint8_t slot, uint8_t state) {
    virtual_sensor_t* s = sensor_registry_get(slot);
    if (!s || !s->paired) return false;
    RadioInterface* r = get_radio(s->radio_type);
    if (!r) return false;
    return r->send_command(s->mac, state);
}

bool RadioManager::send_restart(uint8_t slot) {
    virtual_sensor_t* s = sensor_registry_get(slot);
    if (!s || !s->paired) return false;
    RadioInterface* r = get_radio(s->radio_type);
    if (!r) return false;
    return r->send_restart(s->mac);
}

unsigned long RadioManager::total_rx_count() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_rx_count();
    return t;
}

unsigned long RadioManager::total_ack_count() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_ack_count();
    return t;
}

unsigned long RadioManager::total_crc_errors() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_crc_errors();
    return t;
}

bool RadioManager::any_pairing_active() const {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].radio->is_pairing()) return true;
    return false;
}

bool RadioManager::any_start_pairing() {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].radio->start_pairing()) return true;
    return false;
}

void RadioManager::all_stop_pairing() {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->stop_pairing();
}

void RadioManager::all_announce() {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->announce();
}

void RadioManager::all_broadcast_time_sync(uint32_t epoch) {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->broadcast_time_sync(epoch);
}

RadioInterface* RadioManager::get_radio(uint8_t radio_type) const {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].type == radio_type)
            return m_entries[i].radio;
    return nullptr;
}
```

- [ ] **Step 3: Verify build for esp8266 and esp32**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

(Expect: compilation succeeds — RadioManager has no dependencies on ESP-NOW or LoRa yet)

- [ ] **Step 4: Commit**

```bash
git add hub/include/radio_manager.h hub/src/radio_manager.cpp
git commit -m "feat(hub): add RadioManager for radio dispatch abstraction"
```

---

### Task 3: Refactor espnow_handler to EspnowHandler class (header)

**Files:**
- Modify: `hub/include/espnow_handler.h`
- Requires: Task 1 (RadioInterface)

**Interfaces:**
- Consumes: `RadioInterface`, `espnow_protocol.h`
- Produces: `EspnowHandler : public RadioInterface` with full ESP-NOW API

- [ ] **Step 1: Rewrite espnow_handler.h**

```cpp
#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include "radio_interface.h"
#include "espnow_protocol.h"
#include "sensor_registry.h"
#include <stdint.h>

#ifdef HABILITA_ESPNOW

class EspnowHandler : public RadioInterface {
public:
    EspnowHandler();
    ~EspnowHandler() {}

    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

    bool start_pairing() override;
    void stop_pairing() override;
    bool is_pairing() const override;
    unsigned long pairing_remaining_ms() const override;

    unsigned long get_rx_count() const override { return m_rx_count; }
    unsigned long get_ack_count() const override { return m_ack_count; }
    unsigned long get_crc_errors() const override { return m_crc_errors; }

    uint8_t* get_radio_mac() override { return m_gateway_mac; }
    void announce() override;
    void broadcast_time_sync(uint32_t epoch_seconds) override;

    bool send_command(const uint8_t* mac, uint8_t state) override;
    bool send_restart(const uint8_t* mac) override;

    // Callback dispatch — called from C-linkage recv_cb
    void handle_rx(const uint8_t* mac, const uint8_t* data, int len);

private:
    static EspnowHandler* s_self;  // for C-linkage callback

    bool m_pairing_mode = false;
    unsigned long m_pairing_start = 0;
    uint8_t m_gateway_mac[6];
    unsigned long m_last_heartbeat = 0;
    unsigned long m_rx_count = 0;
    unsigned long m_ack_count = 0;
    unsigned long m_crc_errors = 0;

    static const uint8_t s_bcast_addr[6];

    struct PendingPair {
        bool active;
        uint8_t mac[6];
        uint8_t sensor_type;
        uint16_t sequence;
        uint8_t client_chip;
        char name[32];
    };
    static const int PENDING_PAIR_MAX = 5;
    PendingPair m_pending_pairs[PENDING_PAIR_MAX];

    static const int PENDING_STATE_MAX = 5;
    uint8_t m_pending_state_slots[PENDING_STATE_MAX];
    int m_pending_state_head = 0;
    int m_pending_state_tail = 0;

    uint16_t m_time_sync_sequence = 0;

    void queue_bridge_state(int slot);
    void process_bridge_queue();
    void send_ack(const uint8_t* mac, uint16_t sequence, uint8_t status, uint8_t slot);
    void send_pair_response(const uint8_t* mac, uint16_t sequence, uint16_t slot);
    void send_gw_announce(const uint8_t* mac);
    const uint8_t* dest_for_chip(const uint8_t* mac, uint8_t client_chip);
};

#else
// Stub: EspnowHandler compiles to nothing when ESP-NOW disabled
class EspnowHandler : public RadioInterface {
public:
    int init() override { return -1; }
    int send(const uint8_t*, size_t) override { return -1; }
    void loop() override {}
    bool is_ready() const override { return false; }
};
#endif

#endif
```

- [ ] **Step 2: Commit**

```bash
git add hub/include/espnow_handler.h
git commit -m "refactor(hub): EspnowHandler class header (RadioInterface)"
```

---

### Task 4: Refactor espnow_handler.cpp to EspnowHandler class (implementation)

**Files:**
- Modify: `hub/src/espnow_handler.cpp`
- Requires: Task 3 (EspnowHandler header)

- [ ] **Step 1: Rewrite espnow_handler.cpp**

Replace entire file. Key changes:
- Wrap in `#ifdef HABILITA_ESPNOW`
- All `static` variables become member variables (prefixed `m_`)
- Functions become methods
- `espnow_recv_cb` remains `extern "C"` static function that delegates via `EspnowHandler::s_self->handle_rx()`
- `espnow_add_peer_wrapper`, `espnow_send_wrapper`, `mac_copy`, `mac_equal`, `mac_to_str` remain as free functions (from platform.h)

Full implementation:

```cpp
#ifdef HABILITA_ESPNOW

#include "espnow_handler.h"
#include "config.h"
#include "sensor_registry.h"
#include "mqtt_client.h"
#include "platform.h"
#include "log_buffer.h"
#include <Arduino.h>
#include <EEPROM.h>
#include "common_console.h"

EspnowHandler* EspnowHandler::s_self = nullptr;
const uint8_t EspnowHandler::s_bcast_addr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

EspnowHandler::EspnowHandler() {
    memset(m_pending_pairs, 0, sizeof(m_pending_pairs));
    memset(m_pending_state_slots, 0, sizeof(m_pending_state_slots));
}

int EspnowHandler::init() {
    s_self = this;
    WiFi.mode(WIFI_STA);
    WiFi.macAddress(m_gateway_mac);

    if (esp_now_init() != 0) {
        console.println("[ESP-NOW] Init failed");
        return -1;
    }

#if !defined(ARDUINO_ARCH_ESP32)
    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
#endif
    esp_now_register_recv_cb(espnow_recv_cb);

    uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espnow_add_peer_wrapper(broadcast_mac, WiFi.channel());

    char mac_str[18];
    mac_to_str(m_gateway_mac, mac_str, sizeof(mac_str));
    console.printf("[ESP-NOW] Initialized, MAC: %s WiFi ch=%d\n",
                  mac_str, WiFi.channel());
    return 0;
}

int EspnowHandler::send(const uint8_t* data, size_t len) {
    return espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)data, len, "ESP-NOW") ? 0 : -1;
}

bool EspnowHandler::is_ready() const {
    return true; // ESP-NOW is always ready after init
}

void EspnowHandler::loop() {
    if (m_pairing_mode && millis() - m_pairing_start > PAIRING_WINDOW_MS) {
        m_pairing_mode = false;
        digitalWrite(STATUS_LED_GPIO, HIGH);
        console.println("[ESP-NOW] Pairing mode timeout");
    }

    for (int i = 0; i < PENDING_PAIR_MAX; i++) {
        if (!m_pending_pairs[i].active) continue;

        m_pending_pairs[i].active = false;
        int free_slot = sensor_registry_find_free_slot();

        if (free_slot < 0) {
            send_ack(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, PAIR_STATUS_FULL, 0xFF);
            continue;
        }

        if (!sensor_registry_add(m_pending_pairs[i].mac, m_pending_pairs[i].sensor_type,
                                 free_slot, m_pending_pairs[i].name, m_pending_pairs[i].client_chip)) {
            int existing = sensor_registry_find_by_mac(m_pending_pairs[i].mac);
            if (existing >= 0)
                send_pair_response(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, existing);
            continue;
        }
        send_pair_response(m_pending_pairs[i].mac, m_pending_pairs[i].sequence, free_slot);
        {
            char mac_str[18];
            mac_to_str(m_pending_pairs[i].mac, mac_str, sizeof(mac_str));
            log_add("info", "Sensor %s pareado slot %d", mac_str, free_slot);
        }
        if (mqtt_client_is_connected())
            mqtt_client_publish_discovery(sensor_registry_get(free_slot));

        char pending_mac_str[18];
        mac_to_str(m_pending_pairs[i].mac, pending_mac_str, sizeof(pending_mac_str));
        console.printf("[ESP-NOW] Paired sensor slot %d: %s type=%d\n",
                      free_slot, pending_mac_str, m_pending_pairs[i].sensor_type);
    }

    process_bridge_queue();

    if (millis() - m_last_heartbeat > HEARTBEAT_INTERVAL_MS) {
        m_last_heartbeat = millis();

        for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
            virtual_sensor_t *s = sensor_registry_get(i);
            if (s && s->paired) {
                unsigned long elapsed = millis() - s->last_seen;
                if (s->online && elapsed > SENSOR_TIMEOUT_MS) {
                    s->online = false;
                    log_add("warn", "Sensor slot %d offline", i);
                    console.printf("[ESP-NOW] Sensor slot %d offline (last seen %lu ms ago)\n", i, elapsed);
                }
            }
        }
    }
}

bool EspnowHandler::start_pairing() {
    if (sensor_registry_count_paired() >= MAX_VIRTUAL_SENSORS) {
        console.println("[ESP-NOW] Max sensors reached");
        return false;
    }
    m_pairing_mode = true;
    m_pairing_start = millis();
    digitalWrite(STATUS_LED_GPIO, LOW);
    console.printf("[ESP-NOW] Pairing mode started (%us)\n", PAIRING_WINDOW_MS / 1000);
    return true;
}

void EspnowHandler::stop_pairing() {
    m_pairing_mode = false;
    digitalWrite(STATUS_LED_GPIO, HIGH);
    console.println("[ESP-NOW] Pairing mode stopped");
}

bool EspnowHandler::is_pairing() const {
    return m_pairing_mode;
}

unsigned long EspnowHandler::pairing_remaining_ms() const {
    if (!m_pairing_mode) return 0;
    unsigned long elapsed = millis() - m_pairing_start;
    if (elapsed >= PAIRING_WINDOW_MS) return 0;
    return PAIRING_WINDOW_MS - elapsed;
}

void EspnowHandler::queue_bridge_state(int slot) {
    int next = (m_pending_state_head + 1) % PENDING_STATE_MAX;
    if (next == m_pending_state_tail) return;
    m_pending_state_slots[m_pending_state_head] = slot;
    m_pending_state_head = next;
}

void EspnowHandler::process_bridge_queue() {
    while (m_pending_state_tail != m_pending_state_head) {
        int slot = m_pending_state_slots[m_pending_state_tail];
        m_pending_state_tail = (m_pending_state_tail + 1) % PENDING_STATE_MAX;
        virtual_sensor_t *s = sensor_registry_get(slot);
        if (s && s->paired && mqtt_client_is_connected())
            mqtt_client_publish_state(s);
    }
}

void EspnowHandler::send_ack(const uint8_t *mac, uint16_t sequence, uint8_t status, uint8_t slot) {
    if (!mac || mac_equal(mac, s_bcast_addr)) return;
    espnow_ack_t ack;
    memset(&ack, 0, sizeof(ack));
    ack.msg_type = ESPNOW_MSG_ACK;
    ack.sequence = sequence;
    ack.status = status;
    ack.assigned_slot = slot;
    mac_copy(ack.sensor_mac, mac);

    espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
    espnow_send_wrapper((uint8_t*)mac, (uint8_t*)&ack, sizeof(ack), "ESP-NOW");
}

void EspnowHandler::send_pair_response(const uint8_t *mac, uint16_t sequence, uint16_t slot) {
    if (!mac || mac_equal(mac, s_bcast_addr)) return;
    espnow_pair_response_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.msg_type = ESPNOW_MSG_PAIR_RESPONSE;
    resp.sequence = sequence;
    resp.status = PAIR_STATUS_OK;
    resp.assigned_slot = slot;
    mac_copy(resp.sensor_mac, mac);
    mac_copy(resp.gateway_mac, m_gateway_mac);

    espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
    espnow_send_wrapper((uint8_t*)mac, (uint8_t*)&resp, sizeof(resp), "ESP-NOW");
}

void EspnowHandler::send_gw_announce(const uint8_t *mac) {
    espnow_gw_announce_t ann;
    memset(&ann, 0, sizeof(ann));
    ann.msg_type = ESPNOW_MSG_GW_ANNOUNCE;
    mac_copy(ann.gateway_mac, m_gateway_mac);
    strncpy((char*)ann.fw_version, FW_VERSION, sizeof(ann.fw_version));

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper(s_bcast_addr, ch);
    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&ann, sizeof(ann), "ESP-NOW");
}

void EspnowHandler::announce() {
    send_gw_announce(s_bcast_addr);
}

const uint8_t* EspnowHandler::dest_for_chip(const uint8_t *mac, uint8_t client_chip) {
    if (client_chip == HW_CHIP_ESP_1) {
        espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
        return mac;
    }
    return s_bcast_addr;
}

bool EspnowHandler::send_command(const uint8_t *mac, uint8_t state) {
    uint8_t chip = HW_CHIP_UNKNOWN;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && mac_equal(s->mac, mac)) {
            chip = s->client_chip;
            break;
        }
    }
    espnow_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = ESPNOW_MSG_COMMAND;
    cmd.sequence = 0;
    mac_copy(cmd.target_mac, mac);
    cmd.command = state;

    const uint8_t *dest = dest_for_chip(mac, chip);
    return espnow_send_wrapper((uint8_t*)dest, (uint8_t*)&cmd, sizeof(cmd), "ESP-NOW");
}

bool EspnowHandler::send_restart(const uint8_t *mac) {
    uint8_t chip = HW_CHIP_UNKNOWN;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->paired && mac_equal(s->mac, mac)) {
            chip = s->client_chip;
            break;
        }
    }
    espnow_restart_t rst;
    memset(&rst, 0, sizeof(rst));
    rst.msg_type = ESPNOW_MSG_RESTART;
    rst.sequence = 0;
    mac_copy(rst.target_mac, mac);

    const uint8_t *dest = dest_for_chip(mac, chip);
    return espnow_send_wrapper((uint8_t*)dest, (uint8_t*)&rst, sizeof(rst), "ESP-NOW");
}

void EspnowHandler::broadcast_time_sync(uint32_t epoch_seconds) {
    espnow_time_sync_t ts;
    memset(&ts, 0, sizeof(ts));
    ts.msg_type = ESPNOW_MSG_TIME_SYNC;
    ts.sequence = m_time_sync_sequence++;
    mac_copy(ts.gateway_mac, m_gateway_mac);
    ts.epoch_seconds = epoch_seconds;

    int ch = WiFi.channel();
    if (ch < 1 || ch > 13) ch = 1;
    espnow_add_peer_wrapper((uint8_t*)s_bcast_addr, ch);
    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&ts, sizeof(ts), "ESP-NOW");
}

void EspnowHandler::handle_rx(const uint8_t *mac, const uint8_t *data, int len) {
    if (!data || len < 1) { m_crc_errors++; return; }
    m_rx_count++;
    uint8_t msg_type = data[0];

    char mac_str[18];
    mac_to_str(mac, mac_str, sizeof(mac_str));

    switch (msg_type) {
        case ESPNOW_MSG_PAIR_REQUEST: {
            if (len < (int)sizeof(espnow_pair_request_t)) { m_crc_errors++; return; }
            const espnow_pair_request_t *req = (const espnow_pair_request_t*)data;
            char sensor_mac_str[18];
            mac_to_str(req->sensor_mac, sensor_mac_str, sizeof(sensor_mac_str));

            int existing_slot = sensor_registry_find_by_mac(req->sensor_mac);
            if (existing_slot >= 0) {
                send_pair_response(mac, req->sequence, existing_slot);
                return;
            }
            {
                EEPROM.begin(EEPROM_SIZE);
                bool pairing_required = EEPROM.read(EEPROM_PAIRING_EN_OFFSET) == 1;
                EEPROM.end();
                if (pairing_required && !m_pairing_mode) {
                    espnow_nak_t nak;
                    memset(&nak, 0, sizeof(nak));
                    nak.msg_type = ESPNOW_MSG_NAK;
                    nak.sequence = req->sequence;
                    mac_copy(nak.target_mac, req->sensor_mac);
                    nak.reason = NAK_REASON_PAIRING_DISABLED;
                    espnow_send_wrapper((uint8_t*)s_bcast_addr, (uint8_t*)&nak, sizeof(nak), "ESP-NOW");
                    return;
                }
            }
            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (m_pending_pairs[i].active && mac_equal(m_pending_pairs[i].mac, req->sensor_mac)) {
                    return;
                }
            }
            for (int i = 0; i < PENDING_PAIR_MAX; i++) {
                if (!m_pending_pairs[i].active) {
                    mac_copy(m_pending_pairs[i].mac, req->sensor_mac);
                    m_pending_pairs[i].sensor_type = req->sensor_type;
                    m_pending_pairs[i].client_chip = req->client_chip;
                    m_pending_pairs[i].sequence = req->sequence;
                    strncpy(m_pending_pairs[i].name, req->device_name, sizeof(m_pending_pairs[i].name) - 1);
                    m_pending_pairs[i].name[sizeof(m_pending_pairs[i].name) - 1] = '\0';
                    m_pending_pairs[i].active = true;
                    break;
                }
            }
            break;
        }

        case ESPNOW_MSG_SENSOR_DATA:
        case ESPNOW_MSG_HEARTBEAT: {
            if (len < (int)ESPNOW_HEADER_FIXED_SIZE) { m_crc_errors++; return; }
            const espnow_header_t *hdr = (const espnow_header_t*)data;
            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { m_crc_errors++; return; }
            if (len < (int)(ESPNOW_HEADER_FIXED_SIZE + hdr->payload_len)) { m_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            if (msg_type == ESPNOW_MSG_SENSOR_DATA) {
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_DENIED, 0xFF);
                    return;
                }
                sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
                send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                log_add("info", "Dados recebidos slot %d seq %d", slot, hdr->sequence);
                m_ack_count++;
                queue_bridge_state(slot);
            } else {
                if (slot >= 0) {
                    sensor_registry_get(slot)->last_seen = millis();
                    sensor_registry_get(slot)->online = true;
                    send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
                }
            }
            break;
        }

        case ESPNOW_MSG_GW_DISCOVER: {
            int slot = sensor_registry_find_by_mac(mac);
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot >= 0) {
                    sensor_registry_add(mac, SENSOR_TYPE_REPEATER, slot, "Repeater", HW_CHIP_ESP_1);
                }
            } else {
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s && s->type != SENSOR_TYPE_REPEATER) {
                    s->type = SENSOR_TYPE_REPEATER;
                    strncpy(s->name, "Repeater", sizeof(s->name) - 1);
                    s->name[sizeof(s->name) - 1] = '\0';
                    sensor_registry_save();
                }
            }
            send_gw_announce(mac);
            break;
        }

        case ESPNOW_MSG_REPEATER_STATUS: {
            if (len < (int)(ESPNOW_HEADER_FIXED_SIZE + sizeof(payload_repeater_status_t))) { m_crc_errors++; return; }
            const espnow_header_t *hdr = (const espnow_header_t*)data;
            if (hdr->version != ESPNOW_PROTOCOL_VERSION) { m_crc_errors++; return; }

            int slot = sensor_registry_find_by_mac(hdr->sensor_mac);
            if (slot < 0) {
                slot = sensor_registry_find_free_slot();
                if (slot < 0) {
                    send_ack(mac, hdr->sequence, PAIR_STATUS_FULL, 0xFF);
                    return;
                }
                sensor_registry_add(hdr->sensor_mac, hdr->sensor_type, slot, "Repeater", HW_CHIP_ESP_1);
            } else {
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (s && s->type != SENSOR_TYPE_REPEATER) {
                    s->type = SENSOR_TYPE_REPEATER;
                    strncpy(s->name, "Repeater", sizeof(s->name) - 1);
                    s->name[sizeof(s->name) - 1] = '\0';
                    sensor_registry_save();
                }
            }
            sensor_registry_update_state(slot, hdr, hdr->payload, hdr->payload_len);
            send_ack(mac, hdr->sequence, PAIR_STATUS_OK, slot);
            log_add("info", "Repeater status slot %d seq %d", slot, hdr->sequence);
            m_ack_count++;
            queue_bridge_state(slot);
            break;
        }

        case ESPNOW_MSG_COMMAND: {
            if (len < 10) { m_crc_errors++; return; }
            const espnow_command_t *cmd = (const espnow_command_t *)data;
            uint8_t target[6];
            mac_copy(target, cmd->target_mac);
            bool mac_is_zero = (target[0] == 0 && target[1] == 0 && target[2] == 0 &&
                                target[3] == 0 && target[4] == 0 && target[5] == 0);
            if (mac_is_zero && len >= (int)sizeof(espnow_command_t) && cmd->target_device_id[0] != '\0') {
                int slot = sensor_registry_find_by_name(cmd->target_device_id);
                if (slot < 0) return;
                virtual_sensor_t *s = sensor_registry_get(slot);
                if (!s || !s->paired) return;
                mac_copy(target, s->mac);
            }
            if (mac_is_zero) return;
            espnow_command_t fwd;
            memset(&fwd, 0, sizeof(fwd));
            fwd.msg_type = ESPNOW_MSG_COMMAND;
            fwd.sequence = cmd->sequence;
            mac_copy(fwd.target_mac, target);
            fwd.command = cmd->command;
            espnow_send_wrapper((uint8_t *)s_bcast_addr, (uint8_t *)&fwd, sizeof(fwd), "ESP-NOW");
            break;
        }

        default:
            m_crc_errors++;
            break;
    }
}

// C-linkage callback registered with ESP-NOW
#ifdef ESP32
extern "C" void espnow_recv_cb(const uint8_t *mac, const uint8_t *data, int len) {
#else
extern "C" void espnow_recv_cb(uint8_t *mac, uint8_t *data, uint8_t len) {
#endif
    if (EspnowHandler::s_self) {
        EspnowHandler::s_self->handle_rx(mac, data, len);
    }
}

#endif // HABILITA_ESPNOW
```

- [ ] **Step 2: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/espnow_handler.cpp
git commit -m "refactor(hub): EspnowHandler class implementation"
```

---

### Task 5: Adapt LoraHandler with send_command

**Files:**
- Modify: `hub/src/lora_handler.h`
- Modify: `hub/src/lora_handler.cpp`
- Requires: Task 1 (RadioInterface)

- [ ] **Step 1: Add send_command to lora_handler.h**

```cpp
class LoraHandler : public RadioInterface {
public:
    // ... existing methods ...
    bool send_command(const uint8_t* mac, uint8_t state) override;
};
```

- [ ] **Step 2: Add implementation to lora_handler.cpp**

```cpp
#ifdef HABILITA_LORA

// ... existing code ...

bool LoraHandler::send_command(const uint8_t* mac, uint8_t state) {
    lora_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.msg_type = LORA_MSG_COMMAND;
    cmd.sequence = 0;
    memcpy(cmd.sensor_id, mac, 6);
    cmd.command = state;
    return send((const uint8_t*)&cmd, sizeof(cmd)) == 0;
}

#endif
```

- [ ] **Step 3: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32 && pio run -e hub_32_lora_heltec
```

- [ ] **Step 4: Commit**

```bash
git add hub/src/lora_handler.h hub/src/lora_handler.cpp
git commit -m "feat(hub): LoraHandler::send_command for RadioInterface"
```

---

### Task 6: Update main.cpp to use RadioManager

**Files:**
- Modify: `hub/src/main.cpp`
- Requires: Task 2 (RadioManager), Task 4 (EspnowHandler class), Task 5 (LoraHandler send_command)

- [ ] **Step 1: Rewrite radio declarations and usage in main.cpp**

Changes:
- Include `radio_manager.h` instead of `espnow_handler.h`
- Declare handlers under `#ifdef` blocks
- Use `s_radio_mgr` for all radio operations
- Remove direct `espnow_*()` calls

```cpp
// At top:
#include "radio_manager.h"

// Remove: #include "espnow_handler.h"

// After console setup, before configTime:
#ifdef HABILITA_ESPNOW
#include "espnow_handler.h"
static EspnowHandler s_espnow;
#endif
#ifdef HABILITA_LORA
#include "lora_handler.h"
#include "lora_protocol.h"
static LoraHandler s_lora;
// ... existing LoraHandler vars and callbacks ...
#endif

RadioManager s_radio_mgr;

// In setup(), replace espnow_handler_init() and lora init with:
#ifdef HABILITA_ESPNOW
    s_espnow.set_rx_callback(nullptr, nullptr);
    s_radio_mgr.add_radio(RADIO_ESPNOW, &s_espnow);
#endif
#ifdef HABILITA_LORA
    s_lora.set_rx_callback(lora_rx_cb, nullptr);
    s_radio_mgr.add_radio(RADIO_LORA, &s_lora);
#endif
    s_radio_mgr.init_all();

// Remove: espnow_announce()
// Replace with:
    s_radio_mgr.all_announce();

// In loop(), replace espnow_handler_loop() and s_lora.loop() with:
    s_radio_mgr.loop_all();

// In handle_console 'p' command, replace espnow_start_pairing() with:
            if (s_radio_mgr.any_start_pairing()) {

// In handle_console 's' status, replace espnow_get_*() with:
            console.printf("Sensores: %d pareados, %d online\n", 
                          sensor_registry_count_paired(), sensor_registry_count_online());
            console.printf("RX: %lu ACK: %lu CRC_ERR: %lu\n",
                          s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count(), s_radio_mgr.total_crc_errors());

// Replace espnow_is_pairing() in status:
            console.printf("Pareamento: %s\n", s_radio_mgr.any_pairing_active() ? "ATIVO" : "inativo");

// Replace button check espnow_is_pairing/espnow_start_pairing with:
        if (digitalRead(PAIR_BUTTON_GPIO) == LOW) {
            if (press_start == 0) press_start = millis();
            else if (millis() - press_start > 3000) {
                if (!s_radio_mgr.any_pairing_active()) {
                    s_radio_mgr.any_start_pairing();
                }
                press_start = 0;
            }
        } else {
            press_start = 0;
        }

// Replace LED pairing blink condition:
        if (s_radio_mgr.any_pairing_active()) {

// Replace telemetry espnow_get_*() with s_radio_mgr:
            console.printf("[%s] Uptime=%lus RX=%lu ACK=%lu Paired=%d Online=%d MQTT=%d\n",
                          TAG, now / 1000, s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count(),
                          sensor_registry_count_paired(), sensor_registry_count_online(),
                          mqtt_client_is_connected());

// Replace espnow_broadcast_time_sync with:
        s_radio_mgr.all_broadcast_time_sync((uint32_t)s_ntp_epoch);
```

- [ ] **Step 2: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/main.cpp
git commit -m "refactor(hub): main.cpp uses RadioManager instead of direct espnow_* calls"
```

---

### Task 7: Update device_router.cpp to use RadioManager

**Files:**
- Modify: `hub/src/device_router.cpp`
- Modify: `hub/include/device_router.h`
- Requires: Task 2 (RadioManager)

- [ ] **Step 1: Rewrite device_router.cpp**

```cpp
#include "device_router.h"
#include "sensor_registry.h"
#include "radio_manager.h"
#include "common_console.h"

extern RadioManager s_radio_mgr;

bool device_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    return s_radio_mgr.send_command(slot, state);
}

bool device_send_restart(const uint8_t *mac, uint8_t slot) {
    return s_radio_mgr.send_restart(slot);
}
```

- [ ] **Step 2: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/device_router.cpp
git commit -m "refactor(hub): device_router dispatches via RadioManager"
```

---

### Task 8: Update web_server.cpp to use RadioManager

**Files:**
- Modify: `hub/src/web_server.cpp`
- Requires: Task 2 (RadioManager)

- [ ] **Step 1: Replace espnow_* calls in web_server.cpp**

Changes:
- Remove `#include "espnow_handler.h"`
- Add `#include "radio_manager.h"`
- Replace all `espnow_get_*()` with `s_radio_mgr.total_*()`
- Replace `espnow_is_pairing()` with `s_radio_mgr.any_pairing_active()`
- Replace `espnow_pairing_remaining_ms()` with `s_radio_mgr.get_radio(RADIO_ESPNOW)->pairing_remaining_ms()` (or 0 if no ESP-NOW radio)
- Replace `espnow_start_pairing()` with `s_radio_mgr.any_start_pairing()`
- Replace `espnow_stop_pairing()` with `s_radio_mgr.all_stop_pairing()`
- Replace `espnow_get_gateway_mac()` with a WiFi MAC fallback

```cpp
// Add include:
#include "radio_manager.h"

// Remove: #include "espnow_handler.h"

// Add extern:
extern RadioManager s_radio_mgr;

// In /api/info handler:
        doc["rx_total"] = s_radio_mgr.total_rx_count();
        doc["ack_total"] = s_radio_mgr.total_ack_count();
        doc["crc_errors"] = s_radio_mgr.total_crc_errors();
        doc["pairing_mode"] = s_radio_mgr.any_pairing_active();
        doc["pairing_remaining_sec"] = 0;
        {
            RadioInterface* r = s_radio_mgr.get_radio(RADIO_ESPNOW);
            if (r) doc["pairing_remaining_sec"] = r->pairing_remaining_ms() / 1000;
        }
        {
            uint8_t mac_buf[6];
            RadioInterface* r = s_radio_mgr.get_radio(RADIO_ESPNOW);
            if (r && r->get_radio_mac()) {
                memcpy(mac_buf, r->get_radio_mac(), 6);
            } else {
                WiFi.macAddress(mac_buf);
            }
            char mac_str[18];
            mac_to_str(mac_buf, mac_str, sizeof(mac_str));
            doc["gateway_mac"] = mac_str;
        }

// In /api/pair/start:
        if (s_radio_mgr.any_pairing_active()) {
            s_server.send(409, "application/json", "{\"error\":\"already pairing\"}");
        } else if (s_radio_mgr.any_start_pairing()) {
            log_add("info", "Pareamento iniciado");
            s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            s_server.send(400, "application/json", "{\"error\":\"max sensors reached\"}");
        }

// In /api/pair/stop:
        s_radio_mgr.all_stop_pairing();
```

- [ ] **Step 2: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

- [ ] **Step 3: Commit**

```bash
git add hub/src/web_server.cpp
git commit -m "refactor(hub): web_server uses RadioManager"
```

---

### Task 9: Update display_handler.cpp and clean up mqtt_client.cpp

**Files:**
- Modify: `hub/src/display_handler.cpp`
- Modify: `hub/src/mqtt_client.cpp`
- Requires: Task 2 (RadioManager)

- [ ] **Step 1: Update display_handler.cpp**

```cpp
// Add include:
#include "radio_manager.h"

// Remove: #include "espnow_handler.h"

// Add extern:
extern RadioManager s_radio_mgr;

// Replace the extern declarations and calls in render_page_1():
    // Remove:
    // extern unsigned long espnow_get_rx_count(void);
    // extern unsigned long espnow_get_ack_count(void);
    // disp_printf("RX: %lu ACK: %lu", espnow_get_rx_count(), espnow_get_ack_count());
    
    // Replace with:
    disp_printf("RX: %lu ACK: %lu", s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count());
```

- [ ] **Step 2: Clean up mqtt_client.cpp**

Remove the unused `#include "espnow_handler.h"` line.

- [ ] **Step 3: Verify build**

```bash
pio run -e hub_8266 && pio run -e hub_32
```

- [ ] **Step 4: Commit**

```bash
git add hub/src/display_handler.cpp hub/src/mqtt_client.cpp
git commit -m "refactor(hub): display_handler uses RadioManager, mqtt_client cleanup"
```

---

### Task 10: Update platformio.ini with HABILITA_ESPNOW

**Files:**
- Modify: `hub/platformio.ini`
- Requires: Tasks 3-9 (code uses the define)

- [ ] **Step 1: Add HABILITA_ESPNOW to all ESP-NOW build envs**

Add `-DHABILITA_ESPNOW` to the build_flags of: `hub_8266`, `hub_8266_ota`, `hub_32`, `hub_32c3`, `hub_32_ota`.

For `hub_32_lora` and `hub_32_lora_heltec` (which extend `hub_32`), they will inherit it. This is correct — LoRa envs currently need both radios.

Platformio.ini diffs:

For `[env:hub_8266]`:
```
build_flags = 
    -DHABILITA_ESPNOW
    -D HEARTBEAT_INTERVAL_MS=30000
    ...
```

For `[env:hub_8266_ota]`:
```
build_flags = 
    -DHABILITA_ESPNOW
    -D HEARTBEAT_INTERVAL_MS=30000
    ...
```

For `[env:hub_32]`:
```
build_flags = 
    -DHABILITA_ESPNOW
    -D HEARTBEAT_INTERVAL_MS=30000
    ...
```

(Similarly for hub_32c3 and hub_32_ota)

- [ ] **Step 2: Full build test**

```bash
pio run -e hub_8266 && pio run -e hub_32 && pio run -e hub_32c3 && pio run -e hub_32_lora_heltec
```

- [ ] **Step 3: Commit**

```bash
git add hub/platformio.ini
git commit -m "build(hub): add HABILITA_ESPNOW to all ESP-NOW build envs"
```

---

### Task 11: Final verification — build all envs

**Files:** None (verification only)

- [ ] **Step 1: Build all hub envs**

```bash
pio run -e hub_8266 -e hub_8266_ota -e hub_32 -e hub_32c3 -e hub_32_ota -e hub_32_lora -e hub_32_lora_heltec
```

Expected: all 7 envs compile without errors.

- [ ] **Step 2: Verify no dangling espnow_* references**

```bash
rg -n 'espnow_(?!add_peer_wrapper|send_wrapper|copy|equal|to_str|init|protocol_version|header_fixed_size|pair_request_t|pair_response_t|command_t|ack_t|nak_t|gw_announce_t|time_sync_t|restart_t|msg_type_t)' hub/src/ --include '*.cpp' --include '*.h'
```

Should return no matches (all free functions have been moved to EspnowHandler methods).
