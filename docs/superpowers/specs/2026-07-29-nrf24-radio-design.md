# nRF24L01 Radio Support Design

Date: 2026-07-29
Status: Draft
FW_VERSION: v0.0.31+

## 1. Objective

Adicionar suporte ao rádio nRF24L01 no ecossistema Homeware, seguindo o padrão `RadioInterface` + `NodeProtocol`, permitindo uso em nodes ATmega328P (Nano/Pro Mini) e ESP8266/ESP32 com o mesmo código de protocolo compartilhado.

## 2. Arquitetura

```
shared/src/
├── radio_interface.h          ← Transport abstract (existente)
├── node_protocol.h            ← NOVO: Node protocol abstract
├── lora_protocol.h            ← LoRa frames (existente)
├── nrf24_protocol.h           ← NOVO: nRF24 frames otimizados
├── lora_spi_radio.h / .cpp    ← LoRa SPI transport
├── nrf24_spi_radio.h / .cpp   ← NOVO: nRF24 SPI transport (AVR-safe)
├── lora_node_protocol.h/.cpp  ← LoRa node protocol
└── nrf24_node_protocol.h/.cpp ← NOVO: nRF24 node protocol (AVR-safe)

hub/src/
├── main.cpp                   ← usa NRF24SpiRadio via RadioInterface
├── sensor_registry.h / .cpp   ← RADIO_NRF24 = 2
└── platformio.ini              ← env hub_NRF24 (já existe)
```

### 2.1 main.cpp (genérico, sem dependência de API de rádio)

```cpp
#include "radio_interface.h"
#include "node_protocol.h"

static RadioInterface* s_radio = nullptr;
static NodeProtocol* s_proto = nullptr;

// Callbacks do node (implementação específica)
static uint8_t get_sensor_type() { return SENSOR_TYPE_ONOFF; }
static uint8_t get_sensor_payload(uint8_t* buf, uint8_t max_len) {
    if (max_len < 1) return 0;
    buf[0] = digitalRead(RELAY_PIN);
    return 1;
}
static void on_command(uint8_t cmd) { digitalWrite(RELAY_PIN, cmd); }
static void on_paired(uint8_t slot) { /* salva slot */ }
static void on_restart() { ESP.restart(); }

void setup() {
#ifdef RADIO_NRF24
    static NRF24SpiRadio radio({/* config */});
    static NRF24NodeProtocol proto(&radio);
#elif defined(RADIO_LORA_SPI)
    static LoraSpiRadio radio({/* config */});
    static LoraNodeProtocol proto(&radio);
#endif
    s_radio = &radio;
    s_proto = &proto;

    s_proto->callbacks.get_sensor_type = get_sensor_type;
    s_proto->callbacks.get_sensor_payload = get_sensor_payload;
    s_proto->callbacks.on_command = on_command;
    s_proto->callbacks.on_paired = on_paired;
    s_proto->callbacks.on_restart = on_restart;

    s_radio->init();
    s_proto->begin();
}

void loop() {
    s_radio->loop();
    s_proto->loop();
    // WiFi / HTTP / display (ESP only)
}
```

### 2.2 Dependências

- `NRF24SpiRadio` → `RadioInterface` + `RF24` library (tmrh20/RF24)
- `NRF24NodeProtocol` → `RadioInterface` + `nrf24_protocol`
- `RF24` library: compatível AVR + ESP (SPI)
- `NodeProtocol` → puro virtual, sem dependências

## 3. NodeProtocol — Abstract Node Protocol

Classe base abstrata que todo protocolo de node implementa. Permite que `main.cpp` seja independente do tipo de rádio.

```cpp
// node_protocol.h
#ifndef HW_SHARED_NODE_PROTOCOL_H
#define HW_SHARED_NODE_PROTOCOL_H

#include <stdint.h>

struct NodeCallbacks {
    uint8_t (*get_sensor_type)();                     // retorna SENSOR_TYPE_*
    uint8_t (*get_sensor_payload)(uint8_t*, uint8_t); // preenche buf, retorna tamanho
    void    (*on_command)(uint8_t command);            // recebe comando do hub
    void    (*on_paired)(uint8_t slot);               // pairing concluído
    void    (*on_restart)();                           // restart command
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

## 4. nRF24 Protocol — nrf24_protocol.h

Frame otimizado para payload máximo de 32 bytes do nRF24L01.

### 4.1 Frame Structure

```
┌──────────┬──────────┬──────────────────────┐
│ msg_type │  flags   │  payload (até 30B)   │
│  1 byte  │  1 byte  │                      │
└──────────┴──────────┴──────────────────────┘
Total header: 2 bytes. Max payload: 30 bytes.
```

O pipe address (5 bytes) identifica o sender — dispensa `sensor_id[6]` no frame.

### 4.2 Message Types

Reusa o mesmo enum de `lora_protocol.h` para compatibilidade no hub:

```cpp
enum nrf24_msg_type_t {
    NRF24_MSG_SENSOR_DATA   = 0x01,
    NRF24_MSG_PAIR_REQUEST  = 0x02,
    NRF24_MSG_PAIR_RESPONSE = 0x03,
    NRF24_MSG_HEARTBEAT     = 0x04,
    NRF24_MSG_COMMAND       = 0x07,
};
```

Sem NAK (auto-ACK em hardware cobre), sem GW_ANNOUNCE (não necessário).

### 4.3 Flags

```cpp
#define NRF24_FLAG_ACK_REQUESTED  0x01
#define NRF24_FLAG_MORE_FRAGMENTS 0x80  // para payload >30B futuro
```

### 4.4 Pipe Addressing

O hub usa pipe 0 como RX principal com endereço fixo conhecido pelos nodes.
Cada node tem um pipe address único (5 bytes, derivado do chip_id ou configurado).

```
Hub:  pipe 0 = 0xE7E7E7E7E7 (RX, broadcast de todos os nodes)
      pipe 1 = reservado para unicast futuros
      pipes 2-5 = livres

Node: pipe TX = 0xE7E7E7E7E7 (hub address)
      pipe RX = <node_address> (para receber comandos do hub)
```

### 4.5 Enums e Structs

```cpp
#pragma pack(push, 1)

typedef struct {
    uint8_t msg_type;
    uint8_t flags;
    uint8_t payload[];  // até 30 bytes
} nrf24_frame_t;

// Payload de sensor (msg_type = NRF24_MSG_SENSOR_DATA)
// payload[0] = sensor_type
// payload[1..n] = dados do sensor

// Payload de comando (msg_type = NRF24_MSG_COMMAND)
typedef struct {
    uint8_t msg_type;  // NRF24_MSG_COMMAND
    uint8_t flags;
    uint8_t command;    // 0=OFF, 1=ON, 0xFF=RESTART
} nrf24_command_t;

#pragma pack(pop)

#define NRF24_HEADER_SIZE   2
#define NRF24_MAX_PAYLOAD   30
#define NRF24_FRAME_MAX     (NRF24_HEADER_SIZE + NRF24_MAX_PAYLOAD)  // 32
```

O pipe address do sender do pacote recebido é usado como identificador. O hub mapeia pipe address → `sensor_registry` (converte 5 bytes pipe → 6 bytes mac com prefixo `0xNR`).

## 5. NRF24SpiRadio — Transport Implementation

Implementa `RadioInterface` para nRF24L01 via SPI (RF24 library).

### 5.1 Config

```cpp
struct NRF24Config {
    int8_t ce_pin  = 9;   // CE (Chip Enable)
    int8_t csn_pin = 10;  // CSN (SPI SS)
    uint8_t channel = 76; // 2.4GHz channel (0-125)
    uint8_t data_rate = 0; // 0=1Mbps, 1=2Mbps, 2=250kbps
    uint8_t pa_level = 0;  // 0=RF24_PA_MIN, 1=RF24_PA_LOW, 2=RF24_PA_HIGH, 3=RF24_PA_MAX
    uint8_t hub_pipe[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7}; // hub address
    uint8_t node_pipe[5];   // node own address (preenchido por init)
};
```

Para compatibilidade AVR, todo o código usa `uint8_t*` e tamanhos fixos — sem `String`, sem alocação dinâmica.

### 5.2 Class Outline

```cpp
class NRF24SpiRadio : public RadioInterface {
public:
    NRF24SpiRadio(const NRF24Config& config);
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;

private:
    NRF24Config m_cfg;
    bool m_ok;
    RF24 m_radio;  // instância RF24 library
    uint8_t m_rx_buf[NRF24_FRAME_MAX];
    uint8_t m_rx_len;
    uint8_t m_pipe_num;

    void handle_rx();
};
```

- `init()`: `m_radio.begin()` + `setChannel()` + `setDataRate()` + `setPALevel()` + `openReadingPipe(0, m_cfg.hub_pipe)` + `startListening()`
- `send()`: `stopListening()` + `openWritingPipe(m_cfg.hub_pipe)` + `m_radio.write(data, len)` + `startListening()` (ou usar `write()` com auto-ACK)
- `loop()`: `m_radio.available(&m_pipe_num)` → `m_radio.read(m_rx_buf, &m_rx_len)` → extrair RSSI via `m_radio.testRPD()` → callback

## 6. NRF24NodeProtocol — Node Protocol Implementation

Implementa `NodeProtocol` para nRF24, com dois modos:

### 6.1 Modo AVR (send-only, implicit pairing)

Para ATmega328P:

```cpp
class NRF24NodeProtocol : public NodeProtocol {
public:
    NRF24NodeProtocol(RadioInterface* radio);

    void begin() override;
    void loop() override;
    bool is_paired() const override { return true; } // sempre paired via pipe
    uint8_t assigned_slot() const override { return 0; }
    void force_repair() override {} // não faz pairing explícito

private:
    RadioInterface* m_radio;
    uint32_t m_last_send_ms;
    uint32_t m_send_interval_ms; // configurável, default 60s
    uint16_t m_sequence;

    void send_sensor_data();
    void handle_command(const uint8_t* data, uint8_t len);
};
```

- **begin()**: configura `m_send_interval_ms`, inicializa contadores
- **loop()**: se `millis() - m_last_send_ms >= m_send_interval_ms`, monta frame e chama `m_radio->send()`
- **RX**: quando callback é chamado, verifica `NRF24_MSG_COMMAND` → chama `callbacks.on_command(data[0])`
- Sem heartbeat (AVR economy) — sensor data periódico já serve como alive

### 6.2 Modo ESP (full state machine)

Para ESP8266/ESP32, `NRF24NodeProtocol` pode ativar funcionalidades adicionais via `#ifdef ESP8266 || defined(ESP32)`:
- Heartbeat explícito
- Máquina de pareamento (garante slot allocation)
- Suporte a múltiplos payloads

Ou usar uma subclasse separada `NRF24ESPNodeProtocol` que estende `NRF24NodeProtocol`.

## 7. Hub — nRF24 Integration

### 7.1 NRF24SpiRadio no hub

O hub usa `NRF24SpiRadio` como mais uma implementação de `RadioInterface`, exatamente como usa `LoraHandler` para LoRa.

```cpp
#ifdef HABILITA_NRF24
static NRF24SpiRadio s_nrf24(NRF24Config{/* ce=9, csn=10 */});
s_nrf24.set_rx_callback(nrf24_rx_cb);
s_nrf24.init();
s_radio_mgr.add_radio(RADIO_NRF24, &s_nrf24);
#endif
```

### 7.2 Callback de RX (nrf24_rx_cb)

```cpp
void nrf24_rx_cb(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    nrf24_frame_t* frame = (nrf24_frame_t*)data;
    uint8_t pipe = /* from NRF24SpiRadio::last_pipe() */;

    // Converte pipe address (do payload RX ou do pipe_num) para mac[6]
    // Prefixo: 0x00NR (NR = nRF24 magic)
    uint8_t mac[6] = {0x00, 0x4E, 0x52, pipe, 0x00, 0x00};

    switch (frame->msg_type) {
        case NRF24_MSG_SENSOR_DATA:
            sensor_registry_update_state(mac, frame->payload[0]);
            queue_bridge_state(mac);
            break;
        case NRF24_MSG_HEARTBEAT:
            sensor_registry_mark_online(mac);
            break;
        case NRF24_MSG_COMMAND:
            // Hub envia comando para node (via send_command)
            break;
    }
}
```

### 7.3 sensor_registry

```cpp
enum { RADIO_ESPNOW = 0, RADIO_LORA = 1, RADIO_NRF24 = 2 };
```

### 7.4 Envio de comandos

Para enviar comando do hub para um node nRF24:

```cpp
bool NRF24SpiRadio::send_command(const uint8_t* mac, uint8_t state) {
    // Extrai pipe do mac[3]
    uint8_t node_pipe = mac[3];
    // Troca pipe de escrita para o pipe do node
    // Envia nrf24_command_t
}
```

Isso requer que o hub saiba o pipe address de cada node. O pipe pode ser:
- Fixo (derivado do slot ou config)
- Enviado pelo node no PAIR_REQUEST (payload)

## 8. platformio.ini (hub)

```ini
[env:hub_NRF24]
build_flags =
    -D HABILITA_NRF24
    -D HEARTBEAT_INTERVAL_MS=30000
    -D SENSOR_TIMEOUT_MS=300000
    -D PAIR_BUTTON_GPIO=0
    -D STATUS_LED_GPIO=2
lib_deps =
    ${env:hub_32.lib_deps}
    tmrh20/RF24 @ ^1.4.8
```

## 9. Memory Optimization (AVR)

Para ATmega328P (2KB RAM, 32KB flash):

| Medida | Impacto |
|--------|---------|
| Sem `String` class | Economia ~1KB flash |
| Sem `ArduinoJson` | Economia ~10KB flash, ~200B RAM |
| Frame fixo 32 bytes | Buffer RX = 32 bytes |
| Sem pairing state machine | Economia ~500B flash |
| `send_sensor_data()` é função estática, não classe | Economia overhead virtual |
| `NRF24NodeProtocol` sem virtual calls (opção) | Economia vtable |

Para AVR minimal, `NRF24NodeProtocol` pode ser substituído por funções avulsas no `main.cpp` que usam `NRF24SpiRadio` diretamente, ou uma variante sem classes virtuais.

## 10. Migration Path

| Step | File Change | Scope |
|------|-----------|-------|
| 1 | Criar `shared/src/node_protocol.h` | New |
| 2 | Criar `shared/src/nrf24_protocol.h` | New |
| 3 | Criar `shared/src/nrf24_spi_radio.h` | New |
| 4 | Criar `shared/src/nrf24_spi_radio.cpp` | New |
| 5 | Criar `shared/src/nrf24_node_protocol.h` | New |
| 6 | Criar `shared/src/nrf24_node_protocol.cpp` | New |
| 7 | Atualizar `shared/library.json` — add RF24 dep opcional | Modified |
| 8 | Criar `hub/src/nrf24_handler.h/.cpp` (ou integrar no main.cpp) | New |
| 9 | Atualizar `hub/src/main.cpp` — `#ifdef HABILITA_NRF24` | Modified |
| 10 | Atualizar `hub/src/sensor_registry.h` — `RADIO_NRF24 = 2` | Modified |
| 11 | Build test: hub_NRF24 | Verify |
| 12 | Criar node exemplo AVR + nRF24 | New |
| 13 | Teste funcional: hub + node nRF24 | Verify |

## 11. Non-Goals

- Não implementar nRF24 no dashboard do hub (endpoint /api/state genérico já cobre)
- Não modificar RF95 spec — mantida separada
- Não implementar nRF24 OTA via nRF24 (OTA continua via WiFi em nodes ESP)
- Não implementar mesh nRF24 (só estrela hub → nodes)
- Não fazer binding automático de pipe (config fixa ou via EEPROM)
- `NodeProtocol` é comum para LoRa e nRF24 — `nrf24_node_protocol` e `lora_node_protocol` implementam

## 12. Riscos

- AVR RAM limitada (2KB): buffer RX de 32 bytes + RF24 internals (~200B) + EEPROM ~512B deixa ~1.2KB para app — viável
- RF24 library para AVR: `printf`-based debug ocupa flash — desabilitar com `RF24_MINIMAL`
- Pipe de 5 bytes como ID: mapear para mac[6] no hub precisa ser consistente — usar prefixo fixo `0x00, 0x4E, 0x52` (NR)
- nRF24 em ESP coexiste com WiFi: canal do nRF24 não pode conflitar com canal WiFi — configurar canal NRF24 diferente do WiFi
- ESP32 + nRF24 no mesmo SPI: compartilha barramento com outros dispositivos — verificar CS pins

## 13. Extensibilidade Futura

Ver `2026-07-29-rf95-lora-radio-design.md §12` — o mesmo padrão `RadioInterface` + `NodeProtocol` se aplica a Zigbee e Thread custom, trocando apenas o transporte (ZNP UART ou OpenThread UDP). A abstração não muda.
