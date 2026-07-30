# RF433MHz Radio Support Design

Date: 2026-07-29
Status: Draft
FW_VERSION: v0.0.31+

## 1. Objective

Adicionar suporte a rádio 433MHz OOK/ASK no ecossistema Homeware, cobrindo:
- **Recepção** de sensores comerciais (PT2262, EV1527) e customizados
- **Transmissão** de comandos para atuadores 433MHz (outlets, relays)
- Hub com `Radio433Handler` (implementa `RadioInterface`) para RX + TX half-duplex
- Nodes sensores TX-only (sem WiFi, ATmega328P ideais)

## 2. Arquitetura

```
shared/src/
├── radio_interface.h           ← abstract (existente)
└── radio433_handler.h / .cpp   ← NOVO: hub-side 433MHz RX+TX via rc-switch + RH_ASK

hub/src/
├── main.cpp                    ← #ifdef HABILITA_RADIO433
├── sensor_registry.h/.cpp      ← RADIO_433 = 3
└── platformio.ini               ← env hub_radio433 (existe)

nodes/ (exemplos futuros)
├── temp-433/                   ← ATmega328P + DHT22 + FS1000A (TX-only)
│   └── src/main.cpp            ← RCSwitch.send() na função principal
└── switch-433/                 ← ATmega328P + RX module + relay
    └── src/main.cpp            ← RCSwitch.enableReceive() escuta comando
```

### 2.1 Hub com Radio433Handler

O hub usa `Radio433Handler` como mais uma implementação de `RadioInterface`:

```cpp
#ifdef HABILITA_RADIO433
static Radio433Handler s_radio433;
s_radio433.set_rx_callback(rf433_rx_cb);
s_radio_mgr.add_radio(RADIO_433, &s_radio433);
#endif
```

### 2.2 Nodes 433MHz

Diferente de LoRa e nRF24, sensores 433MHz são **simples demais** para precisar de `RadioInterface` ou `NodeProtocol`:
- TX-only: setup → loop { read sensor → rc-switch/ASK send → deepsleep }
- RX-only: setup → loop { rc-switch receive → check address → toggle }
- Sem WiFi, sem OTA, sem pairing, sem heartbeat
- Código direto no `main.cpp` com rc-switch ou RH_ASK chamadas diretas

## 3. Radio433Handler — Hub Implementation

### 3.1 Hardware Setup

O hub precisa de dois módulos 433MHz conectados ao mesmo ESP:

| Módulo | Função | GPIO típico ESP32 |
|--------|--------|-------------------|
| XY-MK-5V (RX) | Receptor dados | GPIO 4 (interrupt) |
| FS1000A (TX)  | Transmissor comandos | GPIO 5 |

Não é possível RX+TX simultâneo — o handler alterna:
- **Modo RX**: `enableReceive()` ativo, escuta pacotes
- **Modo TX**: desativa RX, `send()`, reativa RX
- Transição gerenciada internamente pelo handler

### 3.2 Config

```cpp
struct Radio433Config {
    int8_t rx_pin   = 4;   // interrupt-capable pin for receiver
    int8_t tx_pin   = 5;   // data pin for transmitter
    uint16_t tx_length = 24; // bit length for tx (default PT2262)
    uint8_t protocol = 0;   // 0=auto (comercial+custom), 1=PT2262, 2=EV1527
    int16_t repeat  = 3;   // repeats for reliability
};
```

### 3.3 Class Outline

```cpp
class Radio433Handler : public RadioInterface {
public:
    Radio433Handler(const Radio433Config& cfg);
    int init() override;
    int send(const uint8_t* data, size_t len) override;
    void loop() override;
    bool is_ready() const override;
    
    // Método auxiliar para enviar valor decodificado a um dispositivo 433MHz
    void send_switch(uint32_t address, uint32_t value, uint8_t protocol = 0);

private:
    Radio433Config m_cfg;
    bool m_ok;
    int m_rx_status; // 0=recebendo, 1=transmitindo
    
    void handle_rx();
};
```

- `init()`: `RCSwitch::enableReceive(m_cfg.rx_pin)` + configura TX pin via `RCSwitch::enableTransmit(m_cfg.tx_pin)`. Se usar RH_ASK, inicializa driver.
- `send()`: Se `data` contém frame bruto → envia via RH_ASK; se contém packed `{address:4, value:4}` → envia via rc-switch `send(address << 8 | value, 24)`. Após TX, reativa RX mode.
- `loop()`: `RCSwitch::available()` → `RCSwitch::getReceivedValue()` + `getReceivedProtocol()` + `getReceivedBitlength()`. Se custom RH_ASK: `driver.recv()`.
- No recebimento: extrai `address` (bits altos) e `value` (bits baixos) do valor recebido + protocol id + bit length.
- Chama `m_rx_cb()` com frame padronizado contendo: `{address:4, value:1, protocol:1, bit_length:1}` = 7 bytes + RSSI (0, não disponível em OOK).

### 3.4 Frame de RX (callback)

O callback recebe dados no formato padronizado:

```cpp
typedef struct {
    uint32_t address;    // 24-bit address from rc-switch (ou 16-bit custom)
    uint8_t  value;      // valor do sensor (0-255)
    uint8_t  protocol;   // ID do protocolo (0=raw ASK, 1=PT2262, 2=EV1527)
    uint8_t  bit_length; // bits do pacote
} rf433_data_t;
```

### 3.5 Send Command

Para enviar comando a um atuador 433MHz:

```cpp
void Radio433Handler::send_command(const uint8_t* mac, uint8_t state) {
    // mac[0..3] contém o endereço 433MHz (32 bits)
    // mac[4] contém protocolo
    uint32_t addr;
    memcpy(&addr, mac, 4);
    uint8_t proto = mac[4];
    uint32_t code = (addr << 8) | (state & 0xFF);
    send_switch(addr, state, proto);
}
```

## 4. Protocolos Suportados

### 4.1 Comerciais (rc-switch)

| Protocolo | Bit Length | Address Bits | Data Bits | Exemplo |
|-----------|-----------|-------------|-----------|---------|
| PT2262 | 24 | 20 | 4 | Sensores temperatura, PIR |
| EV1527 | 24 | 20 | 4 | Sensores abertura, presença |
| SC5262 | 24 | 20 | 4 | Outlets, relés |
| Raw OOK | 12-32 | variável | variável | Aprendizado automático |

A decodificação é feita pelo rc-switch `available()` → `getReceivedValue()`.

### 4.2 Custom (RH_ASK)

Para sensores próprios, RH_ASK com Manchester + CRC:

```
┌───────────┬──────────┬──────────────┬───────┐
│ address   │ sensor   │  payload     │ CRC   │
│  2 bytes  │ type     │  1-4 bytes   │2 bytes│
│           │  1 byte  │              │       │
└───────────┴──────────┴──────────────┴───────┘
Total: 6-9 bytes (com RH_ASK cabe em 60 bytes máx)
```

- `address`: ID único de 16 bits configurado no sensor (DIP ou EEPROM)
- `sensor_type`: SENSOR_TYPE do espnow_protocol (para compatibilidade)
- `payload`: dados do sensor (temperatura, umidade, etc.)
- RCSwitch raw mode para enviar/receber ou usar RH_ASK com pinos alternativos (conflict com rc-switch no mesmo pino)

## 5. Node Exemplo: temp-433 (ATmega328P + DHT22 + FS1000A)

```cpp
#include <RCSwitch.h>

#define SENSOR_ADDR  0x123456  // ID fixo (configurável via EEPROM)
#define TX_PIN       10
#define DHT_PIN      4

RCSwitch rf = RCSwitch();
DHT dht(DHT_PIN, DHT22);

void setup() {
    rf.enableTransmit(TX_PIN);
    rf.setRepeatTransmit(3);
    dht.begin();
}

void loop() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        uint8_t temp_c = (uint8_t)(t + 0.5);
        uint8_t hum_c  = (uint8_t)(h + 0.5);
        uint32_t code = (SENSOR_ADDR << 8) | (temp_c << 4) | hum_c;
        rf.send(code, 32);
    }
    delay(60000); // deep sleep opcional
}
```

**Sem WiFi, sem RadioInterface, sem NodeProtocol** — só chama rc-switch.

## 6. Node Exemplo: switch-433 (ATmega328P + RX + relay)

```cpp
#include <RCSwitch.h>

#define MY_ADDR      0x123456  // mesmo address do hub envia
#define RELAY_PIN    3
#define RX_PIN       2

RCSwitch rf = RCSwitch();

void setup() {
    rf.enableReceive(RX_PIN);
    pinMode(RELAY_PIN, OUTPUT);
}

void loop() {
    if (rf.available()) {
        uint32_t code = rf.getReceivedValue();
        if ((code >> 8) == MY_ADDR) {
            digitalWrite(RELAY_PIN, code & 0x01);
        }
        rf.resetAvailable();
    }
}
```

## 7. sensor_registry

```cpp
enum { RADIO_ESPNOW = 0, RADIO_LORA = 1, RADIO_NRF24 = 2, RADIO_433 = 3 };
```

Sensores 433MHz são **auto-registrados** no hub quando recebidos pela primeira vez:
- Endereço de 32 bits (rc-switch) é mapeado para `mac[6]` com prefixo `0x00, 0x52, 0x46` (RF)
- O protocolo usado é armazenado no quinto byte do mac para permitir TX correto
- Timestamp de `last_seen` atualizado a cada pacote
- Se `SENSOR_TIMEOUT_MS` sem dados, sensor marcado offline

## 8. Callback de RX no Hub

```cpp
void rf433_rx_cb(const uint8_t* data, size_t len, int16_t rssi, void* arg) {
    rf433_data_t* d = (rf433_data_t*)data;

    // Monta mac virtual (prefixo RF + address 32 bits + protocolo)
    uint8_t mac[6] = {0x00, 0x52, 0x46};
    mac[3] = (d->address >> 24) & 0xFF;
    mac[4] = (d->address >> 16) & 0xFF;
    mac[5] = d->protocol;  // para o hub saber como re-enviar

    // Auto-registra se novo
    int slot = sensor_registry_find_by_mac(mac);
    if (slot < 0) {
        slot = sensor_registry_add(mac, SENSOR_TYPE_DIGITAL, "RF433", RADIO_433);
    }

    sensor_registry_update_state(mac, d->value);
    sensor_registry_update_meta(mac, "protocol", d->protocol);
    sensor_registry_update_meta(mac, "bit_length", d->bit_length);
    queue_bridge_state(mac);
}
```

## 9. platformio.ini

### Hub

```ini
[env:hub_radio433]
extends = env:hub_32
build_flags =
    ${env:hub_32.build_flags}
    -D HABILITA_RADIO433
    -D HEARTBEAT_INTERVAL_MS=30000
    -D SENSOR_TIMEOUT_MS=300000
    -D PAIR_BUTTON_GPIO=0
    -D STATUS_LED_GPIO=2
lib_deps =
    ${env:hub_32.lib_deps}
    sui77/rc-switch @ ^2.6.2
```

### Node temp-433 (ATmega328P)

```ini
[env:nano_433]
platform = atmelavr
board = nano
framework = arduino
lib_deps =
    sui77/rc-switch @ ^2.6.2
    adafruit/DHT sensor library @ ^1.4.4
```

## 10. Migration Path

| Step | File Change | Scope |
|------|-----------|-------|
| 1 | Criar `shared/src/radio433_handler.h` | New |
| 2 | Criar `shared/src/radio433_handler.cpp` | New |
| 3 | Atualizar `hub/src/sensor_registry.h` — `RADIO_433 = 3` | Modified |
| 4 | Atualizar `hub/src/main.cpp` — `#ifdef HABILITA_RADIO433` | Modified |
| 5 | Build test: hub_radio433 | Verify |
| 6 | Teste funcional: hub recebe de sensor comercial PT2262 | Verify |
| 7 | Criar `nodes/temp-433/` — exemplo TX-only | New |
| 8 | Teste: hub recebe de temp-433 | Verify |
| 9 | Teste: hub envia comando para switch-433 | Verify |

## 11. Non-Goals

- Sem OTA via 433MHz (muito lento, não confiável)
- Sem dashboard/node para 433MHz (nodes são ATmega328P, sem WiFi)
- Sem NodeProtocol para nodes 433MHz (TX-only é simples demais)
- Sem suporte a rolling code (KeeLoq, etc.)
- Sem mesh 433MHz
- Sem `Radio433Handler` no lado do node (só hub)
- O `send()` implementa apenas envio de códigos rc-switch / RH_ASK, sem lógica de protocolo

## 12. Riscos

- **Interferência**: Muitos dispositivos 433MHz no mesmo ambiente podem colidir — sensores intervalos randômicos (jitter) ajudam
- **Range**: FS1000A a 3.3V tem alcance limitado (~20m). Para maior alcance, usar módulo com amplificador (ex: STX-433) ou alimentar TX a 5V/12V
- **Esp8266 + rc-switch**: rc-switch usa timer0 no ESP8266 pode conflitar com WiFi — usar ESP32 é mais seguro
- **RX+TX half-duplex**: durante TX, pacotes RX são perdidos — com repeat=3 (~200ms) o gap é aceitável para sensores de baixa frequência (60s)
- **rc-switch sem CRC**: payload de sensores comerciais sem checksum. O hub aceita o valor como recebido — falso positivo possível mas improvável com address matching

## 13. Extensibilidade Futura

Os rádios bidirecionais (LoRa, nRF24, Zigbee, Thread) seguem o padrão `RadioInterface` + `NodeProtocol` descrito em `2026-07-29-rf95-lora-radio-design.md §12`. O RF433 é exceção por ser predominantemente TX-only — nodes simples demais pra precisar da abstração. Caso surja necessidade de RF433 bidirecional custom, o `Radio433Handler` seria refatorado para implementar `RadioInterface` completo e `NodeProtocol` seria opcional.
