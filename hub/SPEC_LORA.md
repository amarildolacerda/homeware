# SPEC_LORA — Protocolo LoRa (Hub ↔ Nodes)

Especificação do protocolo de rádio LoRa entre o hub e nodes LoRa via `RadioInterface` + `sandeepmistry/LoRa` (SX1278).
Referente a `FW_VERSION` atual do hub. Compilado condicionalmente com `LORA_ENABLED`.

## 1. Geral

- Transporte: **LoRa** (sem WiFi/IP entre node e hub).
- Chip: SX1278 (Semtech) via `sandeepmistry/LoRa`.
- Frequência: `LORA_FREQ` (868.0 MHz EU / 915.0 MHz US/AU).
- Spreading Factor: `LORA_SF` (10), ajustável (8–12). SF10~300m com obstáculos.
- Bandwidth: `LORA_BW` (125 kHz).
- Coding Rate: `LORA_CR` (7 = 4/7).
- Potência TX: `LORA_TX_POWER` (17 dBm).
- O LoRa coexiste com ESP-NOW no mesmo hub (TTGO LORA32). Ambos alimentam o mesmo `sensor_registry` → MQTT → HA.
- Nodes LoRa podem ser MCUs sem WiFi (apenas LoRa) ou ESP32+LoRa.

## 2. Frame Binário

Overhead fixo de **11 bytes** + payload (máx. ~200 bytes):

```
┌──────────┬────────┬────────────┬──────┬─────────────┬──────────┐
│ msg_type │  seq   │  sensor_id │ rssi │ payload_len │  payload │
│  1 byte  │ 2 bytes│  6 bytes   │1 byte│   1 byte    │  N bytes │
└──────────┴────────┴────────────┴──────┴─────────────┴──────────┘
```

| Campo | Offset | Tamanho | Descrição |
|-------|--------|---------|-----------|
| `msg_type` | 0 | 1 | Tipo da mensagem (0x01–0x07) |
| `seq` | 1 | 2 | Sequência (little-endian, wrap 65535) |
| `sensor_id` | 3 | 6 | ID do node (chip_id, 6 bytes) |
| `rssi` | 9 | 1 | RSSI do último recebido (inteiro sinal) |
| `payload_len` | 10 | 1 | Tamanho do payload (0–200) |
| `payload` | 11 | N | Dados do sensor |

## 3. Tipos de Mensagem

| Valor | Constante | Origem → Destino | Descrição |
|-------|-----------|------------------|-----------|
| 0x01 | `LORA_MSG_SENSOR_DATA` | Node → Hub | Leitura de sensor |
| 0x02 | `LORA_MSG_PAIR_REQUEST` | Node → Hub | Solicita pareamento |
| 0x03 | `LORA_MSG_PAIR_RESPONSE` | Hub → Node | Confirma slot atribuído |
| 0x04 | `LORA_MSG_HEARTBEAT` | Node → Hub | Keep-alive |
| 0x05 | `LORA_MSG_NAK` | Hub → Node | Negativa de pareamento |
| 0x06 | `LORA_MSG_GW_ANNOUNCE` | Hub → Node | Anúncio de presença do hub |
| 0x07 | `LORA_MSG_COMMAND` | Hub → Node | Comando ON/OFF ou ação |

## 4. Tipos de Sensor

Os mesmos tipos do protocolo ESP-NOW (`sensor_type_t`), reusando as mesmas `payload_*_t` structs.

| Valor | Nome | Payload |
|-------|------|---------|
| 1 | `SENSOR_TYPE_TEMP_HUM` | `payload_temp_hum_t { float temperature; float humidity; }` |
| 2 | `SENSOR_TYPE_CONTACT` | `payload_contact_t { uint8_t contact_state; uint8_t tamper; }` |
| 3 | `SENSOR_TYPE_MOTION` | `payload_motion_t { uint8_t motion_state; uint8_t occupancy_duration; }` |
| 4 | `SENSOR_TYPE_GAS` | `payload_gas_t { uint16_t gas_level; uint8_t alarm; }` |
| 5 | `SENSOR_TYPE_RAIN` | `payload_rain_t { uint8_t rain_level; uint8_t rain_digital; }` |
| 6 | `SENSOR_TYPE_TANK` | `payload_tank_t { uint16_t level_pct; uint16_t distance_cm; }` |
| 7 | `SENSOR_TYPE_DHT_GAS` | `payload_dht_gas_t { float temperature; float humidity; uint16_t gas_level; uint8_t alarm; }` |
| 8 | `SENSOR_TYPE_ONOFF` | `payload_onoff_t { uint8_t state; }` |
| 9 | `SENSOR_TYPE_LIGHT` | `payload_onoff_t { uint8_t state; }` |
| 10 | `SENSOR_TYPE_REPEATER` | `payload_repeater_status_t { uint16_t received; uint16_t forwarded; uint8_t client_count; uint8_t channel; int16_t rssi; uint32_t uptime_s; uint16_t free_heap; uint8_t ack_failures; }` |
| 11 | `SENSOR_TYPE_DHT_RELE` | (reservado) |

## 5. Pareamento (Pairing)

### Fluxo

1. Node LoRa envia `LORA_MSG_PAIR_REQUEST` (broadcast).
   `sensor_id` = chip_id do node (6 bytes). `payload[0]` = `sensor_type`.
2. Hub recebe e busca `sensor_id` no `sensor_registry`:
   - Se encontrado → reenvia `PAIR_RESPONSE` com slot existente
   - Se não → aloca slot livre (`sensor_registry_find_free_slot`, máx. 64) e adiciona
3. Hub envia `LORA_MSG_PAIR_RESPONSE` (broadcast):
   - `payload[0]` = `assigned_slot`
4. Node armazena o slot e passa a enviar dados normalmente.
5. Pareamento é **automático** — não exige modo de pareamento ativo (mesma política do ESP-NOW, Fase 1).

### `LORA_MSG_PAIR_RESPONSE`

```
msg_type=0x03 | seq(2) | sensor_id(6) | rssi=0 | payload_len=1 | assigned_slot(1)
```

### `LORA_MSG_NAK`

```
msg_type=0x05 | seq(2) | sensor_id(6) | rssi=0 | payload_len=1 | reason(1)
```

| reason | Significado |
|--------|-------------|
| 0x01 | Registry cheio (`PAIR_STATUS_FULL`) |
| 0x02 | Pareamento negado (`PAIR_STATUS_DENIED`) |

## 6. Dados de Sensor

Node envia `LORA_MSG_SENSOR_DATA`:

```
msg_type=0x01 | seq(2) | sensor_id(6) | rssi(1) | payload_len(1) | payload(N)
```

O hub:
1. Busca slot por `sensor_id` no registry
2. Se encontrado → `sensor_registry_update_state(slot, ...)` com os dados do payload
3. Enfileira para MQTT (`queue_bridge_state(slot)`)
4. Atualiza `last_seen` e `online=true`

O `rssi` no frame é preenchido pelo node (RSSI da última recepção do hub, ou 0 se desconhecido).

## 7. Heartbeat

Node envia `LORA_MSG_HEARTBEAT` periodicamente (intervalo configurável no node):

```
msg_type=0x04 | seq(2) | sensor_id(6) | rssi=0 | payload_len=0
```

Hub atualiza `last_seen` do slot. Timeout: `SENSOR_TIMEOUT_MS = 300000` (5 min) → `online=false`.

## 8. Comando

Hub → Node via `LORA_MSG_COMMAND`:

```
msg_type=0x07 | seq(2) | sensor_id(6) | rssi=0 | payload_len=1 | command(1)
```

| command | Significado |
|---------|-------------|
| 0x00 | OFF |
| 0x01 | ON |

Disparado via `POST /api/sensor/{slot}/command {"state":0|1}`. O hub localiza o `sensor_id` pelo slot e envia o comando via LoRa.

## 9. Pinos (TTGO LORA32 T3_v1.6)

```c
// SX1278 via sandeepmistry/LoRa
#define LORA_SS     18
#define LORA_RST    14
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27

#define LORA_FREQ       868.0
#define LORA_SF         10
#define LORA_BW         125
#define LORA_CR         7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17
```

SPI custom: `SPI.begin(SCK=5, MISO=19, MOSI=27, SS=18)`. Compatível com OLED I2C (SDA=4, SCL=15) — sem conflito de GPIO.

## 10. Broadcast vs Unicast (ESP-NOW regra 18 aplicada ao LoRa)

LoRa é naturalmente broadcast (meio compartilhado), mas o RadioLib permite endereçamento por `sensor_id`:

- **Node LoRa → Hub**: sempre broadcast (não há MAC no LoRa, o `sensor_id` identifica o node)
- **Hub → Node LoRa**: broadcast também (filtragem por `sensor_id` no node). O node ignora mensagens não endereçadas a ele.

## 11. Loop Non-Blocking (Polling)

- `LoraHandler::loop()` chama `LoRa.parsePacket()` a cada iteração — polling mode, sem ISR
- `handle_rx()` lê bytes via `LoRa.available()`/`LoRa.read()` e entrega ao `rx_callback`
- Após `send()`, `LoRa.receive()` reabilita RX contínuo
- Sem `delay()` no loop — usa `millis()` para timeouts e retries

## 12. Arquivos de Implementação

| Arquivo | Descrição |
|---------|-----------|
| `shared/src/radio_interface.h` | Classe abstrata `RadioInterface` (init/send/loop/is_ready + rx_callback) |
| `shared/src/lora_protocol.h` | Structs `lora_frame_t`, `lora_pair_response_t`, `lora_nak_t`, `lora_pair_request_t` + enum `lora_msg_type_t` |
| `hub/include/lora_config.h` | Pinos TTGO e parâmetros de rádio |
| `hub/src/lora_handler.h` | `class LoraHandler : public RadioInterface` — declaração |
| `hub/src/lora_handler.cpp` | sandeepmistry/LoRa init, send, polling `parsePacket()`, callback dispatch |
| `hub/src/main.cpp` | Instância `LoraHandler`, callback `lora_rx_cb` que faz parsing + dispatch |

## 13. Regras de Implementação

- [ ] `HABILITA_LORA` guarda todo código LoRa (compilação condicional)
- [ ] `LoraHandler::loop()` non-blocking — sem `delay()`
- [ ] `LoRa.receive()` reabilitado após `endPacket()` no `send()` — retorna ao RX contínuo
- [ ] `sensor_registry` inalterado — recebe dados de ESP-NOW e LoRa igualmente
- [ ] `espnow_handler` zero modificações
- [ ] Polling com `LoRa.parsePacket()` — sem ISR
- [ ] LoRa handler não faz dispatch direto — entrega dados via `rx_callback`. O dispatch para `sensor_registry`/`mqtt_client` é responsabilidade do caller.
- [ ] `queue_bridge_state(slot)` usado para enfileirar estados → MQTT
- [ ] `FW_VERSION` igual entre hub e todos os nodes (regra 13)
