# RF95 LoRa Radio Support Design

Date: 2026-07-29
Status: Draft
FW_VERSION: v0.0.31+

## 1. Objective

Adicionar suporte a rádio LoRa RF95 no ecossistema Homeware, seguindo o padrão `RadioInterface` + protocolo compartilhado, separando transporte da lógica de protocolo.

## 2. Arquitetura

```
shared/src/
├── radio_interface.h          ← abstract transport (inalterado)
├── lora_protocol.h            ← frame structs (inalterado)
├── lora_spi_radio.h / .cpp    ← sandeepmistry/LoRa via SPI
├── lora_serial_radio.h / .cpp ← RadioHead RH_RF95 via Serial
└── lora_node_protocol.h / .cpp ← protocolo compartilhado (pairing, sensor_data, heartbeat)

nodes/onoff-lora/              ← refatorado para usar LoraSpiRadio + LoraNodeProtocol
nodes/onoff-rf95/              ← novo node que usa LoraSerialRadio + LoraNodeProtocol (opcional futuro)

hub/src/
├── lora_handler.h / .cpp      ← substituído por LoraSpiRadio (shared)
└── main.cpp                   ← usa LoraSpiRadio via RadioInterface
```

### 2.1 Dependências

```
LoraSpiRadio ──→ RadioInterface (shared)
             ──→ sandeepmistry/LoRa (library, SPI)
             ──→ lora_protocol (shared, via LoraNodeProtocol)

LoraSerialRadio ──→ RadioInterface (shared)
                ──→ RadioHead (library, Serial)
                ──→ lora_protocol (shared, via LoraNodeProtocol)

LoraNodeProtocol ──→ RadioInterface (shared)
                 ──→ lora_protocol (shared)

Hub main.cpp ──→ LoraSpiRadio (como RadioInterface*)
             ──→ sensor_registry (hub)
             ──→ mqtt_client (hub)
```

## 3. Transport Layer

### 3.1 LoraSpiRadio — SPI LoRa (sandeepmistry/LoRa)

Implementa `RadioInterface` para módulos SX1276/SX1278/RF95 conectados via SPI.

```cpp
struct LoraSpiConfig {
    int8_t ss    = 18;
    int8_t rst   = 14;
    int8_t dio0  = -1;   // -1 = polling mode
    int8_t sck   = 5;
    int8_t miso  = 19;
    int8_t mosi  = 27;
    float  freq  = 868.0;  // MHz
    uint8_t sf   = 10;
    float  bw    = 125E3;  // Hz
    uint8_t cr   = 7;      // coding rate 4/7
    uint8_t preamble = 8;
    int8_t  tx_power  = 17;  // dBm
};

class LoraSpiRadio : public RadioInterface {
public:
    LoraSpiRadio(const LoraSpiConfig& config);
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
private:
    LoraSpiConfig m_cfg;
    bool m_ok;
    uint8_t m_rx_buf[256];
    void handle_rx();
};
```

- `init()`: SPI.begin() + LoRa.setPins() + LoRa.begin(freq) + configura SF/BW/CR/TxPower/Preamble + LoRa.receive()
- `send()`: LoRa.beginPacket() + LoRa.write() + LoRa.endPacket() + LoRa.receive()
- `loop()`: polling LoRa.parsePacket() → available() → read() → callback
- Compilado com `-DLORA_DEVICE` (não inclui esp_now.h)

### 3.2 LoraSerialRadio — Serial LoRa (RadioHead RH_RF95)

Implementa `RadioInterface` para módulos RF95 conectados via UART Serial ou SoftwareSerial.

```cpp
enum SerialRadioType { HARDWARE_SERIAL, SOFTWARE_SERIAL };

struct LoraSerialConfig {
    int8_t rx_pin    = -1;  // -1 = hardware serial
    int8_t tx_pin    = -1;
    uint8_t terminal_id = 1;
    float  freq      = 868.0;
    int8_t tx_power  = 14;   // dBm, RF95 max 20
    uint8_t preamble = 8;
    bool   promiscuous = true;
};

class LoraSerialRadio : public RadioInterface {
public:
    LoraSerialRadio(const LoraSerialConfig& config);
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
    int16_t last_rssi() const;
private:
    LoraSerialConfig m_cfg;
    bool m_ok;
    uint8_t m_rx_buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t m_rx_len;
    int16_t m_last_rssi;
    void handle_rx();
};
```

- `init()`: rf95.init() + rf95.setFrequency() + rf95.setTxPower() + rf95.setPreambleLength() + rf95.setModeRx()
- `send()`: rf95.setHeaderFrom/To/Id/Flags() + rf95.send() + rf95.waitPacketSent(1000)
- `loop()`: rf95.waitAvailableTimeout(50) → rf95.recv() → extrai headerFrom/rssi → callback
- `headerFrom` do quadro recebido identifica o sender (útil para filtrar loopback)

**Detalhes do frame no LoraSerialRadio:**
- O payload é enviado raw (bytes do `lora_frame_t`), sem envelope adicional
- `headerFrom` = `terminal_id` (config), `headerTo` = `0xFF` (broadcast)
- No receive: filtra frames de si mesmo (`headerFrom == terminal_id`)

### 3.3 RadioInterface (inalterado)

```cpp
class RadioInterface {
public:
    virtual ~RadioInterface() {}
    virtual int init() = 0;
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual void loop() = 0;
    virtual bool is_ready() const = 0;

    using rx_callback_t = void (*)(const uint8_t* data, size_t len, int16_t rssi, void* arg);
    void set_rx_callback(rx_callback_t cb, void* arg = nullptr);

    // Optional com default false — não são mais override obrigatório
    virtual bool send_command(const uint8_t* mac, uint8_t state);
    virtual bool send_restart(const uint8_t* mac);
    virtual unsigned long get_rx_count() const;
    virtual unsigned long get_ack_count() const;
    virtual unsigned long get_crc_errors() const;
    virtual uint8_t* get_radio_mac();
    virtual void broadcast_time_sync(uint32_t epoch);

protected:
    rx_callback_t m_rx_cb = nullptr;
    void* m_rx_arg = nullptr;
};
```

## 4. Protocol Layer — LoraNodeProtocol

Classe compartilhada que implementa a máquina de estados do node LoRa sobre qualquer `RadioInterface`.

### 4.1 Callbacks

```cpp
class LoraNodeProtocol {
public:
    LoraNodeProtocol(RadioInterface* radio);

    // Callbacks que o node deve implementar
    uint8_t (*get_sensor_type)();                   // retorna SENSOR_TYPE_*
    uint8_t (*get_sensor_payload)(uint8_t* buf, uint8_t max_len); // preenche buf, retorna tamanho
    void    (*on_command)(uint8_t command);          // recebe comando do hub
    void    (*on_paired)(uint8_t slot);    // pairing concluído
    void    (*on_restart)();               // restart command
    const uint8_t* (*get_mac)();           // MAC do dispositivo

    // Lifecycle
    void begin();                          // inicia pareamento
    void loop();                           // deve ser chamado no loop() principal
    void force_repair();                   // força re-pareamento

    // Estado
    bool is_paired() const;
    uint8_t assigned_slot() const;

private:
    RadioInterface* m_radio;
    uint8_t m_mac[6];
    bool m_paired;
    uint8_t m_slot;
    uint32_t m_pair_interval_ms;
    uint32_t m_heartbeat_interval_ms;
    uint32_t m_state_interval_ms;
    uint32_t m_last_pair_ms;
    uint32_t m_last_heartbeat_ms;
    uint32_t m_last_state_ms;
    uint8_t m_pair_attempts;
    uint16_t m_sequence;

    void send_pair_request();
    void send_sensor_data();
    void send_heartbeat();
    void handle_frame(const uint8_t* data, size_t len, int16_t rssi);
};
```

### 4.2 Protocol State Machine

```
    ┌─────────┐
    │  IDLE   │ ← begin() / force_repair()
    └────┬────┘
         │ send_pair_request() a cada PAIR_INTERVAL_MS
         │ até LORA_MAX_PAIR_ATTEMPTS ou receber PAIR_RESPONSE
         ▼
    ┌─────────┐
    │ PAIRING │ ← recebeu PAIR_RESPONSE → on_paired(slot)
    └────┬────┘
         │ paired = true
         ▼
    ┌─────────┐
    │ PAIRED  │ → send_sensor_data() a cada STATE_INTERVAL
    │         │ → send_heartbeat() a cada HEARTBEAT_INTERVAL
    │         │ → recebe COMMAND → on_command()
    │         │ → recebe RESTART → on_restart()
    └─────────┘
```

### 4.3 Message Types (lora_protocol.h — inalterado)

- `LORA_MSG_SENSOR_DATA` (0x01) — node → hub
- `LORA_MSG_PAIR_REQUEST` (0x02) — node → hub
- `LORA_MSG_PAIR_RESPONSE` (0x03) — hub → node
- `LORA_MSG_HEARTBEAT` (0x04) — node → hub
- `LORA_MSG_NAK` (0x05) — hub → node
- `LORA_MSG_GW_ANNOUNCE` (0x06) — hub → node
- `LORA_MSG_COMMAND` (0x07) — hub → node

## 5. Node refatorado (ex: onoff-lora)

### `main.cpp` após refatoração:

```cpp
#include "lora_spi_radio.h"
#include "lora_node_protocol.h"

static LoraSpiRadio s_radio(LoraSpiConfig{ /* .ss=18, .rst=14, ... */ });
static LoraNodeProtocol s_proto(&s_radio);

void setup() {
    s_radio.init();
    s_radio.set_rx_callback(rx_cb);
    s_proto.get_sensor_type = []() { return SENSOR_TYPE_ONOFF; };
    s_proto.get_sensor_payload = [](uint8_t* buf, uint8_t max_len) {
        if (max_len < 1) return (uint8_t)0;
        buf[0] = digitalRead(RELAY_PIN);
        return (uint8_t)1;
    };
    s_proto.on_command = [](uint8_t cmd) { set_relay(cmd); };
    s_proto.on_paired = [](uint8_t slot) { /* salva slot */ };
    s_proto.on_restart = []() { ESP.restart(); };
    s_proto.begin();
}

void loop() {
    s_radio.loop();
    s_proto.loop();
    // WiFi / HTTP server / display / botão / etc.
}
```

## 6. Hub — refatoração

O `LoraHandler` atual (`hub/src/lora_handler.h/.cpp`) deve ser refatorado para usar `LoraSpiRadio` como transporte subjacente ou ser substituído por ele.

Opção recomendada: **substituir** `LoraHandler` por `LoraSpiRadio`, movendo a lógica de protocolo (dispatch de pair_request, sensor_data, etc.) para o callback registrado pelo `main.cpp` (já é assim hoje — o `lora_rx_cb()` em `main.cpp` faz o dispatch).

### Mudanças no hub:

| Arquivo | Ação |
|---------|------|
| `hub/src/lora_handler.h` | Remover — substituído por `shared/src/lora_spi_radio.h` |
| `hub/src/lora_handler.cpp` | Remover — substituído por `shared/src/lora_spi_radio.cpp` |
| `hub/include/lora_config.h` | Manter para config da placa, ou migrar para struct `LoraSpiConfig` |
| `hub/src/main.cpp` | Trocar `LoraHandler` por `LoraSpiRadio`, callback segue igual |
| `hub/platformio.ini` | `lib_extra_dirs` já inclui `../shared`, nenhuma mudança |

## 7. library.json (shared)

Adicionar dependências condicionais:

```json
{
    "name": "homeware-shared",
    "version": "0.0.1",
    "dependencies": {
        "arduinojson/ArduinoJson": "^7",
        "tzapu/WiFiManager": "^2.0"
    }
}
```

As libs de rádio (sandeepmistry/LoRa, RadioHead) ficam no `platformio.ini` do projeto que as consome, não no shared.

## 8. Configuração RF95

### LoraSerialRadio pinos típicos:

| Placa | RX | TX | Tipo Serial |
|-------|----|----|-------------|
| ESP32 (RF95 UART) | 16 | 17 | HardwareSerial |
| ESP8266 (RF95 UART) | GPIO4(D2) | GPIO5(D1) | SoftwareSerial |
| TTGO LoRa32 V1 | N/A | N/A | SPI (LoraSpiRadio) |

### RadioHead dependencies:

Para `LoraSerialRadio`, o `platformio.ini` do node que usar RF95 precisa adicionar:
```ini
lib_deps =
    ${env:base.lib_deps}
    arduinolibraries/RadioHead @ ^1.122
build_flags =
    -DLORA_DEVICE
    -DRF95
```

## 9. Migration Path

| Step | File Change | Scope |
|------|------------|-------|
| 1 | Criar `shared/src/lora_spi_radio.h` | New |
| 2 | Criar `shared/src/lora_spi_radio.cpp` | New |
| 3 | Criar `shared/src/lora_serial_radio.h` | New |
| 4 | Criar `shared/src/lora_serial_radio.cpp` | New |
| 5 | Criar `shared/src/lora_node_protocol.h` | New |
| 6 | Criar `shared/src/lora_node_protocol.cpp` | New |
| 7 | Refatorar `hub/src/lora_handler.h/.cpp` para usar LoraSpiRadio | Modified |
| 8 | Refatorar `hub/src/main.cpp` (remoção do LoraHandler, uso do LoraSpiRadio) | Modified |
| 9 | Refatorar `nodes/onoff-lora/main.cpp` para usar LoraSpiRadio + LoraNodeProtocol | Modified |
| 10 | Build test: hub_32_lora | Verify |
| 11 | Build test: onoff-lora | Verify |
| 12 | Teste funcional: hub + node LoRa (comunicação existente não quebra) | Verify |
| 13 | Criar node de teste `nodes/onoff-rf95` (opcional, RF95 Serial) | New |

## 10. Non-Goals

- Não criar dashboard novo para RF95 (mesmo padrão dos nodes existentes)
- Não modificar `lora_protocol.h` (frames existentes são compatíveis)
- Não modificar `radio_interface.h` (interface já atende)
- Não refatorar nodes ESP-NOW (lamp, climate-gas, presence)
- Não implementar `LoraSerialRadio` no hub (hub continua usando SPI)
- Não criar o node `onoff-rf95` — criação será decidida após validação
- Não alterar `library.json` do shared (dependências de rádio ficam no platformio.ini de cada projeto)

## 11. Riscos

- RadioHead via SoftwareSerial em ESP8266 pode perder pacotes em altas taxas — usar baud rate baixo (9600) ou HardwareSerial
- Polling no `LoraSerialRadio::loop()` com `waitAvailableTimeout(50)` adiciona 50ms de latência — frame pequeno, aceitável para sensoriamento
- Coexistência LoraSpiRadio + LoraSerialRadio no mesmo dispositivo (ex: hub com SPI e node com Serial) — sem conflito, são instâncias separadas

## 12. Extensibilidade Futura: Zigbee / Thread Custom

Caso no futuro implementemos nodes Zigbee ou Thread custom (firmware nosso), a infraestrutura atual (`RadioInterface` + `NodeProtocol`) já os acomoda sem refactors.

### 12.1 Zigbee Custom (ZNP via UART + CC2530/ESP32-HZ)

```
shared/src/
├── radio_interface.h           ← unchanged (init, send, loop, is_ready)
├── zigbee_znp_radio.h/.cpp    ← NOVO: transport via ZNP serial protocol
│     init()   → ZNP init + network formation
│     send()   → ZNP AF_DATA_REQUEST (APS frame addressing)
│     loop()   → ZNP AF_INCOMING_MSG → parse → callback
│     is_ready() → coordinator online
├── zigbee_protocol.h           ← NOVO: IEEE 64-bit ↔ mac[6] mapping
└── zigbee_node_protocol.h/.cpp ← NOVO: NodeProtocol impl sobre Zigbee APS
```

- `ZigbeeZnpRadio` implementa `RadioInterface` via serial UART com o chip Zigbee
- `ZigbeeNodeProtocol` implementa `NodeProtocol` — pairing/heartbeat/sensor_data sobre APS frames
- `main.cpp` do node: `RadioInterface*` + `NodeProtocol*` — mesma abstração, escolhe via `#ifdef RADIO_ZIGBEE`

Fluxo: `NodeProtocol.loop()` → `ZigbeeNodeProtocol::send_sensor_data()` → `ZigbeeZnpRadio::send(data, len)` → ZNP AF_DATA_REQUEST → rádio Zigbee.

No hub: `ZigbeeZnpRadio` com callback → `sensor_registry`. Mesma integração de LoraSpiRadio/NRF24SpiRadio.

### 12.2 Thread Custom (OpenThread + UDP/CoAP)

```
shared/src/
├── radio_interface.h           ← unchanged
├── thread_radio.h/.cpp         ← NOVO: transport via OpenThread UDP socket
│     init()   → otInit() + attach UDP socket
│     send()   → otUdpSend() (IPv6 unicast)
│     loop()   → otTasklets() + poll UDP recv → callback
│     is_ready() → thread attached
├── thread_protocol.h           ← NOVO: IPv6 ↔ mac[6] mapping
└── thread_node_protocol.h/.cpp ← NOVO: NodeProtocol sobre UDP
```

- `ThreadRadio` implementa `RadioInterface` sobre OpenThread UDP
- `ThreadNodeProtocol` implementa `NodeProtocol` com mensagens sobre UDP
- No ESP32, OpenThread roda no mesmo chip (ESP32-H2 ou ESP32-C6)
- `main.cpp`: mesma abstração com `#ifdef RADIO_THREAD`

### 12.3 Principais diferenças

| Camada | LoRa / nRF24 | Zigbee (custom) | Thread (custom) |
|--------|-------------|----------------|-----------------|
| Transport API | SPI / Serial | ZNP UART | OpenThread UDP |
| Addressing | Pipe/MAC | IEEE 64-bit | IPv6 |
| `RadioInterface::send()` | write bytes | `AF_DATA_REQUEST` | `otUdpSend()` |
| `RadioInterface::loop()` | poll radio | poll ZNP serial | `otTasklets()` |
| Mesh | Não (estrela) | Sim (ZCL mesh) | Sim (Thread mesh) |
| Node protocol | `LoraNodeProtocol` | `ZigbeeNodeProtocol` | `ThreadNodeProtocol` |

O `hub/main.cpp` não muda — só adiciona o `#ifdef` e instancia o handler específico:
