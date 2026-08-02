# EspnowNodeProtocol — ESP-NOW Node Abstraction

## Problem

Every ESP-NOW node (`lamp`, `climate-gas`, `presence`, `switch`, `soil-moisture`, `presence-bat`, `rain`) duplicates ~100 lines of ESP-NOW boilerplate: `espnow_send_cb`, `espnow_recv_cb`, `espnow_send_pair_request`, `espnow_send_data`, `espnow_send_heartbeat`, ACK/retry state machine, and ~15 state variables (`s_paired`, `s_gw_mac`, `s_sequence`, etc.).

LoRa nodes already have `LoraNodeProtocol` — a clean abstraction via `NodeProtocol`. ESP-NOW nodes need the same.

## Solution

Create `EspnowNodeProtocol` in `shared/`, implementing `NodeProtocol`. Mirror `LoraNodeProtocol`'s API. Each node replaces ~100 lines of inline ESP-NOW code with:
- `#include "espnow_node_protocol.h"`
- `static EspnowNodeProtocol s_espnow;`
- `s_espnow.begin() / s_espnow.loop() / s_espnow.publish_state()`
- callback struct with `get_sensor_type`, `get_sensor_payload`, `on_command`, `on_paired`, `on_restart`, `on_forward`

## API

```cpp
class EspnowNodeProtocol : public NodeProtocol {
public:
    EspnowNodeProtocol();

    void begin() override;
    void loop() override;
    bool is_paired() const override;
    uint8_t assigned_slot() const override;
    void force_repair() override;

    void publish_state();

    // counters and state (for dashboard)
    int16_t last_rssi() const;
    uint32_t tx_count() const;
    uint32_t rx_count() const;
    uint8_t* my_mac();

    // configuration
    void set_mac(const uint8_t* mac);
    void set_device_name(const char* name);
    void set_gateway_mac(const uint8_t* mac);
    const uint8_t* gateway_mac() const;
    void load_gateway_mac();
    void save_gateway_mac();
    void set_pair_interval(unsigned long ms);
    void set_heartbeat_interval(unsigned long ms);
    void set_state_interval(unsigned long ms);

private:
    // ---- internal state ----
    uint8_t m_mac[6];
    uint8_t m_gateway_mac[6];
    bool m_paired;
    uint8_t m_slot;
    uint16_t m_sequence;
    char m_device_name[32];
    bool m_espnow_ready;
    bool m_ack_received;
    int m_retries_left;

    // timing
    unsigned long m_pair_interval_ms;
    unsigned long m_heartbeat_interval_ms;
    unsigned long m_state_interval_ms;
    unsigned long m_last_pair_ms;
    unsigned long m_last_heartbeat_ms;
    unsigned long m_last_state_ms;
    unsigned long m_send_deadline;
    int m_pair_attempts;

    // counters
    int16_t m_last_rssi;
    uint32_t m_tx_count;
    uint32_t m_rx_count;

    // send state machine
    enum SendState { SEND_IDLE, SEND_WAIT_ACK, SEND_RETRY_DELAY, SEND_RETRY_WAIT_ACK };
    SendState m_send_state;
    uint16_t m_last_send_sequence;

    // internal methods
    void send_pair_request();
    void send_sensor_data();
    void send_heartbeat();
    void handle_frame(const uint8_t* mac, const uint8_t* data, size_t len);
    void on_send_done(const uint8_t* mac, uint8_t status);
};
```

## Callbacks (NodeCallbacks + on_forward)

`NodeCallbacks` is extended with an optional `on_forward`:

```cpp
struct NodeCallbacks {
    uint8_t (*get_sensor_type)();
    uint8_t (*get_sensor_payload)(uint8_t* buf, uint8_t max_len);
    void    (*on_command)(uint8_t command);
    void    (*on_paired)(uint8_t slot);
    void    (*on_restart)();
    void    (*on_forward)(const uint8_t* data, size_t len, const uint8_t* mac); // NEW
};
```

If `on_forward` is set, frames not matching this node's MAC are forwarded. The lamp node uses this for extender mode.

## Singleton Bridge (C-linkage)

ESP-NOW callbacks are C-linkage (`extern "C"`). `EspnowNodeProtocol` uses the same pattern as `EspnowHandler`:

```cpp
static EspnowNodeProtocol* s_self = nullptr;

extern "C" void espnow_recv_cb(uint8_t* mac, uint8_t* data, uint8_t len) {
    if (s_self) s_self->handle_frame(mac, data, len);
}

extern "C" void espnow_send_cb(uint8_t* mac, uint8_t status) {
    if (s_self) s_self->on_send_done(mac, status);
}
```

Only ONE `EspnowNodeProtocol` instance per node (no node uses multiple radios).

## Send State Machine (in loop)

```
SEND_IDLE
  ├─ pair_interval elapsed && !m_paired  → send_pair_request() → stay IDLE
  ├─ state_interval elapsed && m_paired  → send_sensor_data()  → WAIT_ACK
  └─ heartbeat_interval elapsed          → send_heartbeat()    → stay IDLE

SEND_WAIT_ACK
  ├─ ack_received      → IDLE (update counters)
  ├─ timeout && retries left → RETRY_DELAY
  └─ timeout && no retries    → force_repair() → IDLE

SEND_RETRY_DELAY
  └─ delay 50ms → send_sensor_data() → RETRY_WAIT_ACK

SEND_RETRY_WAIT_ACK
  ├─ ack_received  → IDLE
  └─ timeout       → RETRY_DELAY (or force_repair)
```

## handle_frame Dispatch

| msg_type | Action |
|----------|--------|
| `ESPNOW_MSG_PAIR_RESPONSE` | Match MAC → set `m_paired`, `m_slot`, call `on_paired` |
| `ESPNOW_MSG_ACK` | Match sequence → set `m_ack_received`; if `PAIR_STATUS_DENIED` → force_repair |
| `ESPNOW_MSG_NAK` | `NAK_REASON_GATEWAY_LOST` → force_repair |
| `ESPNOW_MSG_RESTART` | Match MAC → call `on_restart` |
| `ESPNOW_MSG_COMMAND` | Match MAC → call `on_command` |
| `ESPNOW_MSG_TIME_SYNC` | (ignored, stored if needed) |
| *any other* | If `on_forward` set → forward callback |

## O que sai de cada node

**Remover do main.cpp (~100 linhas cada):**
- `espnow_send_cb()` — integral
- `espnow_recv_cb()` — integral
- `espnow_send_pair_request()` — integral
- `espnow_send_data()` — integral
- `espnow_send_heartbeat()` — integral
- `s_paired`, `s_gw_mac[6]`, `s_sequence`, `s_assigned_slot`, `s_send_pending`, `s_ack_received`, `s_last_send_sequence`, `s_send_retries_left`, `s_pair_attempts`, `s_last_espnow_send`, `s_last_espnow_pair`, `s_last_heartbeat`, `s_espnow_tx_count`, `s_espnow_rx_count`
- ACK/retry state machine do `loop()` (bloco SEND_IDLE/WAIT_ACK/RETRY)

**Adicionar ao main.cpp:**
```cpp
#include "espnow_node_protocol.h"
static EspnowNodeProtocol s_espnow;
```

`setup()`:
```cpp
s_espnow.set_mac(s_my_mac);
s_espnow.set_device_name(s_device_name);
s_espnow.callbacks = { get_sensor_type, get_sensor_payload, on_command, on_paired, on_restart, on_forward };
s_espnow.load_gateway_mac();
s_espnow.begin();
```

`loop()`:
```cpp
s_espnow.loop();
```

Forçar estado (ex: relay toggle):
```cpp
s_espnow.publish_state();
```

Repair manual (comando `p`):
```cpp
s_espnow.force_repair();
```

## Implementação

Arquivos em `shared/`:
- `shared/src/espnow_node_protocol.h` — classe
- `shared/src/espnow_node_protocol.cpp` — implementação

Testes nativos:
- `tests/unit/test/mock_espnow.h` — mock do ESP-NOW (similar a MockRadio)
- `tests/unit/test/test_espnow_node_protocol.cpp` — testes

## Nodes a refatorar (ordem)

| Node | Esforço | Observação |
|------|---------|------------|
| 1. `climate-gas` | Baixo | Mais simples, sem extender |
| 2. `presence` | Baixo | PIR puro, sem payload extra |
| 3. `switch` | Baixo | Relé ON/OFF sem extender |
| 4. `lamp` | Alto | Extender mode + bridge + Alexa callbacks |
| 5. `soil-moisture` | Médio | Deep sleep, bateria |
| 6. `presence-bat` | Médio | Deep sleep, bateria |
| 7. `rain` | Médio | Previsível |

`extender` fica de fora (lógica própria de forward queue).

## shared/common_espnow.h

Após a refatoração, `common_espnow.h` pode ser reduzido ou incorporado. As funções de persistência (gateway MAC, device name) viram métodos da classe. As funções auxiliares (mac_copy, mac_to_str, etc.) permanecem em `espnow_protocol.h`.

## Riscos

- **lamp extender mode**: o callback `on_forward` é suficiente, mas a lógica de forward queue (ring buffer, processamento em loop) continua no main.cpp do lamp ou vira classe separada futuramente
- **Nodes bateria**: deep sleep interrompe o ciclo — `begin()` após wake deve reenviar pair request. `EspnowNodeProtocol::begin()` já faz isso
- **ACK/retry**: clima-gas usa ACK, lamp usa ACK + retry com timeouts — a máquina de estados precisa ser genérica o suficiente para ambos

## Testes

- Native unit tests com `MockEspnow` (mock dos callbacks C-linkage)
- Testar: pair request, pair response, sensor data, heartbeat, ACK, retry timeout, forward callback, force_repair
- Validar que cada node compila (PlatformIO) após refatoração
