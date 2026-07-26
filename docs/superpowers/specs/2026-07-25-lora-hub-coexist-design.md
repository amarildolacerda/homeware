# Hub LoRa + ESP-NOW Coexistindo — Design

## Objetivo

Adicionar suporte a LoRa (RadioLib/SX1278) ao hub TTGO LORA32 como transporte coexistente com ESP-NOW. Ambos operam no mesmo firmware, alimentam o mesmo `sensor_registry` e pipeline de encaminhamento para HA via MQTT. Nodes LoRa podem ser MCUs simples (sem WiFi, apenas LoRa) ou ESP32 com transceiver LoRa.

## Contexto

- Projeto AgriSense IoT
- Hub atual: ESP-NOW only (ESP8266 D1 Mini ou ESP32), recebe sensores via ESP-NOW, encaminha para server Python via HTTP/MQTT
- Novo hub: TTGO LORA32 (ESP32 + SX1278 868/915 MHz) recebe dados de:
  - **Nodes ESP-NOW** — curto alcance, mesmo AP WiFi
  - **Nodes LoRa** — 300m+ com obstáculos densos (mata), RadioLib no rádio
- Nodes LoRa podem ser bateria (deep sleep) ou sempre ligados
- Nodes LoRa podem ser MCUs simples (sem WiFi) ou ESP32
- RadioLib já usada nos outros radios LoRa do projeto

## Requisitos

1. LoRa e ESP-NOW coexistem no mesmo firmware — compilação condicional via flag `HABILITA_LORA`
2. Nodes LoRa se registram/pareiam via broadcast LoRa (sem necessidade de WiFi)
3. Todos os sensores (ESP-NOW ou LoRa) aparecem no HA como entidades uniformes
4. Hub mantém loop non-blocking (regra 15) — `delay()` nunca bloqueante
5. LoRa usa RadioLib (SX1278), pinos padrão TTGO LORA32
6. `espnow_handler` permanece intacto — zero alterações
7. Env `hub_32` original continua inalterado

## Compilação Condicional (PlatformIO)

Novo ambiente em `hub/platformio.ini`:

```ini
[env:hub_32_lora]
extends = env:hub_32
build_flags =
    -D HABILITA_LORA
lib_deps =
    bblanchon/ArduinoJson@^7.2.1
    knolleary/PubSubClient@^2.8
    jgromes/RadioLib@^7.0
```

O env `hub_32` original continua sem nenhuma alteração — sem `HABILITA_LORA` = apenas ESP-NOW. O lib_deps de RadioLib só entra no env `hub_32_lora`.

## Arquivos

### Novos
- `hub/include/lora_config.h` — pinos LoRa, frequência, SF, BW
- `hub/src/lora_handler.cpp` — init RadioLib, ISR rx, parser LoRa, dispatch
- `hub/src/lora_handler.h` — declarações

### Modificados
- `hub/src/main.cpp` — adiciona `lora_handler_init()` e `lora_handler_loop()` condicionalmente com `#ifdef HABILITA_LORA`
- `hub/platformio.ini` — adiciona env `hub_32_lora`

### Inalterados
- `hub/src/espnow_handler.cpp` — zero alterações
- `hub/src/sensor_registry.cpp` — transport-agnostic, nenhuma mudança
- `hub/src/mqtt_client.cpp` — nenhuma mudança
- `hub/include/config.h` — nenhuma mudança
- Env `hub_32` — completamente inalterado

## Pinos LoRa (TTGO LORA32)

```cpp
// hub/include/lora_config.h
#define LORA_SS_PIN     5
#define LORA_RST_PIN    14
#define LORA_DIO0_PIN   2
#define LORA_DIO1_PIN   15

#define LORA_FREQ       868.0   // MHz (ajustar: 915 para US/AU)
#define LORA_SF         10      // Spreading Factor (8–12)
#define LORA_BW         125     // kHz
#define LORA_CR         7       // Coding Rate 4/7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17
```

SF10/BW125 é o ponto de partida para ~300m com obstáculos densos (mata). Se necessário, pode subir para SF12 (mais alcance, menos throughput) via `LORA_SF`.

## Protocolo LoRa (binário, compacto)

Similar ao ESP-NOW mas sem o overhead de `espnow_header_t` — minimiza airtime em SF alta.

```
┌──────────┬──────┬──────────┬─────┬───────────┬──────────┐
│ msg_type │ seq  │ sensor_id│ rssi│ payload_len│ payload  │
│  1 byte  │ 2    │  6 bytes │ 1   │   1 byte   │ N bytes  │
└──────────┴──────┴──────────┴─────┴───────────┴──────────┘
Overhead fixo = 11 bytes + payload (máx ~200 bytes útil)
```

**msg_types:**
| Valor | Constante | Direção | Descrição |
|-------|-----------|---------|-----------|
| 0x01 | `LORA_MSG_SENSOR_DATA` | Node → Hub | Telemetria do sensor |
| 0x02 | `LORA_MSG_PAIR_REQUEST` | Node → Hub | Primeira conexão / reapareamento |
| 0x03 | `LORA_MSG_PAIR_RESPONSE` | Hub → Node | Resposta de pareamento (assigned_slot) |
| 0x04 | `LORA_MSG_HEARTBEAT` | Node → Hub | Heartbeat periódico |
| 0x05 | `LORA_MSG_NAK` | Hub → Node | Rejeição (full, pairing_disabled) |
| 0x06 | `LORA_MSG_GW_ANNOUNCE` | Hub → Node | Anúncio do gateway (discovery) |
| 0x07 | `LORA_MSG_COMMAND` | Hub → Node | Comando ON/OFF ou ação |

**`sensor_id`**: o `chip_id` do MCU (6 bytes, mesmo formato que ESP-NOW). Para MCUs simples o chip_id pode ser derivado do EFUSE MAC ou de um ID gravado em EEPROM/Flash.

## Pareamento LoRa Nodes (sem WiFi)

Nodes LoRa sem WiFi não podem se registrar via HTTP. O pareamento segue broadcast LoRa:

1. Node LoRa envia `LORA_MSG_PAIR_REQUEST` broadcast na primeira ligação (ou periodicamente)
2. Hub recebe, extrai `sensor_id` + `sensor_type` do payload
3. Hub adiciona ao `sensor_registry` com `paired=true`, `client_chip` = tipo de MCU
4. Hub envia `LORA_MSG_PAIR_RESPONSE` unicast para o MAC do node (ou broadcast se ESP8266→hub)
5. Node armazena `assigned_slot`
6. A partir daí, node envia dados/heartbeat via LoRa (unicast preferred para ESP8266→ESP32 broadcast, regra 18)

**Nota regra 18**: em produção, node ESP8266 com LoRa enviando para hub ESP32 → **BROADCAST obrigatório** (mesma lógica do ESP-NOW). O hub responde por LoRa também. Node ESP32→ESP32 → unicast OK.

## `lora_handler` — Implementação

### `lora_handler_init()`
- Configura pinos SPI (SX1278 no TTGO usa VSPI por padrão)
- Chama `radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SS_PIN, LORA_DIO0_PIN, LORA_RST_PIN, LORA_DIO1_PIN)`
- Registra ISR de rx-done (`radio.setDio1Action(lora_rx_isr)`)
- Coloca rádio em modo RX contínuo (`radio.startReceive()`)

### ISR — `lora_rx_isr()`
- Seta flag `s_lora_rx_flag = true`
- Armazena comprimento do pacote recebido
- NÃO faz parsing dentro da ISR (muito pesado)

### `lora_handler_loop()` (non-blocking)
```cpp
void lora_handler_loop() {
    if (!s_lora_rx_flag) return;
    s_lora_rx_flag = false;

    int len = radio.readData(s_lora_buf, sizeof(s_lora_buf));
    if (len < 11) return; // overhead mínimo não atendido

    lora_parse_packet(s_lora_buf, len);
}
```

### `lora_parse_packet()`
- Extrai msg_type, seq, sensor_id, rssi, payload_len, payload
- Valida checksum (se implementado no payload) ou verifica tamanho mínimo
- Busca `sensor_id` no `sensor_registry`
- Se não encontrado e msg_type == PAIR_REQUEST → processa pareamento
- Se encontrado → chama `sensor_registry_update_state()` (mesma lógica que `espnow_handler.cpp`)
- Se msg_type == SENSOR_DATA e slot válido → enfileira para MQTT (`queue_bridge_state(slot)`)

### `lora_handler_send_command()` / `lora_handler_send_ack()`
- Monta pacote (PAIR_RESPONSE, NAK, COMMAND)
- Chama `radio.transmit(buf, len)`
- Não bloqueante: usa `radio.startTransmit()` + espera por ISR de tx-done com timeout

## Loop Non-Blocking (Regra 15)

```
lora_handler_loop() em cada tick:
  - Se rx_flag set → chama lora_parse_packet() (não faz SPI blocking por mais de 1ms)
  - yield() entre operações SPI
  - NENHUM delay() bloqueante
  - Timeout de tx (se comando enviado, espera ACK por N ms e dá retry limitado)
```

## O que Não Muda

- `espnow_handler` — zero alterações, continua no hub/src/local
- `sensor_registry` — já é transport-agnostic; recebe dados do lado hub seja de ESP-NOW ou LoRa
- `mqtt_client` — mesma interface, recebe estado de qualquer slot pareado
- `web_server` / dashboards — mesma interface
- `hub_32` env PlatformIO — inalterado
- `espnow_protocol.h` em shared — sem alteração

## Riscos e Mitigações

| Risco | Mitigação |
|-------|-----------|
| RadioLib ocupa ~50-80KB flash | `HABILITA_LORA` isola — Só compilado no env `hub_32_lora`; TTGO LORA32 tem 4MB flash  |
| SPI conflicts — LoRa e WiFi compartilham barramento | TTGO LORA32 já configurado para isolar SPI do LoRa do WiFi (VSPI vs HSPI ou pinos dedicados) |
| ISR do RadioLib conflita com ESP-NOW recv_cb | RadioLib usa DIO GPIO interrupt (não ISR do ESP-NOW hw FIFO) — separado em hardware |
| LoRa SF alta → airtime longo → collisions em envios simultâneos | SF configurable; nodes bateria enviam em intervalos espaçados; hub faz debounce de rx |
| MCUs simples sem chip_id único | MCU mínimo grava um ID único em EEPROM na primeira configuração |
| Regra 18 (ESP8266 LoRa node → ESP32 hub) | LoRa também precisa ser BROADCAST se node ESP8266 enviando para hub ESP32 |