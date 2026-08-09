# onoff-lora-avr — Node Relay LoRa para Arduino Nano

**Data:** 2026-08-08
**Status:** Aprovado
**Plataforma:** ATmega328P (Arduino Nano)
**Rádio:** Seeed Studio Grove LoRa Radio 868MHz v1.0 (SX1276 + ATMega168)

---

## 1. Visão Geral

Node relay ON/OFF que comunica com o hub existente via LoRa 868MHz. Arduino Nano controla relé, botão físico e LED de status. Módulo Seeed Grove LoRa opera em modo raw SPI pass-through (firmware custom no ATMega168).

```
┌─────────────────┐      SoftwareSerial      ┌──────────────────┐
│  Arduino Nano   │ ──────────────────────── │ Seeed Grove LoRa │
│  (ATmega328P)   │     TX→RX, RX→TX         │ (ATMega168+SX1276)│
│                 │                           │ Firmware: raw SPI │
│ - Relay ON/OFF  │                           │ pass-through      │
│ - Button        │                           └────────┬─────────┘
│ - LED status    │                                    │
│ - Serial console│                               LoRa 868MHz
│ - Protocol LoRa │                                    │
└─────────────────┘                                    │
                                                      │
                    ┌─────────────────────────────────┘
                    │
            ┌───────┴───────┐
            │  Hub (ESP32)   │
            │  Sandeepmistry │
            │  /LoRa (SPI)   │
            │  + WiFi → MQTT │
            │  → Home Assist. │
            └────────────────┘
```

## 2. Firmware ATMega168 (módulo Seeed)

O ATMega168 do módulo Seeed Grove LoRa será reescrito para operar como **bridge SPI raw** entre UART e SX1276.

### Comandos UART (Nano → ATMega168)

| Comando | Formato | Descrição |
|---------|---------|-----------|
| TX | `T` + len_hi + len_lo + [data...] | Envia pacote LoRa |
| Habilitar RX | `R` | Coloca SX1276 em modo RX contínuo |
| Status | `?` | Retorna `T`(pronto) ou `B`(busy) |

### Respostas ATMega168 → Nano

| Resposta | Formato | Descrição |
|----------|---------|-----------|
| TX OK | `T` | Pacote transmitido (DIO0 disparou) |
| RX data | `D` + len_hi + len_lo + [data...] | Pacote recebido |
| Erro | `E` | Erro na operação |

### Configuração SX1276

| Parâmetro | Valor | Nota |
|-----------|-------|------|
| Modo | LoRa | |
| Frequência | 868.0 MHz | ISM Europa/Brasil |
| Spreading Factor | 10 | |
| Bandwidth | 125 kHz | |
| Coding Rate | 4/7 | |
| Preamble | 8 symbols | |
| TX Power | 17 dBm | PA_BOOST |
| CRC | On | |
| Implicit Header | Off | |

### Tamanho estimado

~250 bytes de flash (ATMega168 tem 16KB disponível).

## 3. Protocolo LoRa

O node AVR usa o mesmo `lora_frame_t` do hub. Sem customização de protocolo.

### Struct (idêntica ao shared/lora_protocol.h)

```c
typedef struct {
    uint8_t  msg_type;       // 1 byte
    uint16_t sequence;       // 2 bytes
    uint8_t  sensor_id[6];   // 6 bytes (MAC)
    int8_t   rssi;           // 1 byte (preenchido pelo receptor)
    uint8_t  payload_len;    // 1 byte
    uint8_t  payload[];      // 0-200 bytes
} lora_frame_t;              // Total: 11 + payload_len
```

### Mensagens enviadas pelo node

| Tipo | msg_type | Payload | Quando |
|------|----------|---------|--------|
| Pair Request | 0x02 | sensor_type(1) + device_name(16) | Boot + retry a cada 5s |
| Sensor Data | 0x01 | relay_state(1) | Mudança de estado + periódico (60s) |
| Heartbeat | 0x05 | vazio | A cada 60s enquanto pareado |

### Mensagens recebidas do hub

| Tipo | msg_type | Payload | Ação do node |
|------|----------|---------|--------------|
| Pair Response | 0x03 | assigned_slot(1) | Salvar slot, marcar `paired = true` |
| Command | 0x07 | command(1) | 0x01=ON, 0x00=OFF, 0xFF=restart |
| NAK | 0x0D | reason(1) | Log no serial, retry |

### Device ID

`avr_<signature>` — usa 3 bytes da signature do ATmega328P (único por chip):
```c
const uint8_t sig0 = pgm_read_byte(0x00);
const uint8_t sig1 = pgm_read_byte(0x01);
const uint8_t sig2 = pgm_read_byte(0x02);
// device_id = "avr_" + hex(sig0) + hex(sig1) + hex(sig2)
```

### State Machine

```
BOOT → PAIRING → PAIRED
  │       │         │
  │       │         └─→ enviar SENSOR_DATA a cada 60s
  │       │             receber COMMAND
  │       │             enviar HEARTBEAT a cada 60s
  │       │
  │       └─→ enviar PAIR_REQUEST a cada 5s (max 20 tentativas)
  │           receber PAIR_RESPONSE → PAIRED
  │           timeout → LED piscando, aguardar botão 'p'
  │
  └─→ ler relay da EEPROM, init hardware
```

## 4. Hardware (Arduino Nano)

### Pinos

| Pino | Função | Direção | Nota |
|------|--------|---------|------|
| D2 | SoftwareSerial RX | Input | Conectado ao TX do módulo Seeed |
| D3 | SoftwareSerial TX | Output | Conectado ao RX do módulo Seeed |
| D4 | LED status | Output | HIGH=ON, LOW=OFF (inverted) |
| D5 | Relé | Output | HIGH=ligar (configurable via `RELAY_ON`) |
| D6 | Botão | Input | INPUT_PULLUP, ativo LOW |
| D7 | Livre | - | Futuros sensores |
| A0 | Livre | - | Futuros sensores |

### Conexões

```
Arduino Nano        Seeed Grove LoRa
┌──────────┐       ┌──────────────┐
│ D2  (RX) │←──────│ TX           │
│ D3  (TX) │──────→│ RX           │
│ GND      │───────│ GND          │
│ 5V       │───────│ VCC          │
└──────────┘       └──────────────┘
```

### Definições

| Define | Valor | Nota |
|--------|-------|------|
| `LORA_RX_PIN` | 2 | SoftwareSerial RX |
| `LORA_TX_PIN` | 3 | SoftwareSerial TX |
| `RELAY_PIN` | 5 | |
| `BUTTON_PIN` | 6 | |
| `LED_PIN` | 4 | |
| `SERIAL_BAUD` | 115200 | Debug USB |
| `LORA_BAUD` | 9600 | SoftwareSerial → Seeed |
| `DEVICE_NAME` | "LoRa AVR Switch" | Configurável |

### Alimentação

- USB (5V) para desenvolvimento
- Fonte externa 7-12V no barrel jack para produção

## 5. Console Serial

### Comandos

| Comando | Função |
|---------|--------|
| `h` / `?` | Ajuda |
| `s` | Status (device_id, relay, pareado, RSSI, uptime, RX/TX) |
| `l` | Alterna relé (toggle) |
| `p` | Força re-pareamento |
| `r` | Reinicia |

### Exemplo de status

```
--- Status ---
Device:  avr_A1B2C3
Nome:    LoRa AVR Switch
Relé:    OFF
Pareado: Sim
RSSI:    -45 dBm (LoRa)
Slot:    0
Uptime:  3600s
RX/TX:   120 / 45
```

## 6. LED Status

| Estado | LED |
|--------|-----|
| Pareado + relé ON | Aceso (LOW) |
| Pareado + relé OFF | Apagado (HIGH) |
| Em pareamento | Pisca 250ms |
| Boot (antes de parear) | Pisca 500ms |

## 7. Funcionalidades

### Relé

- **Botão físico D6:** toggle com debounce 50ms
- **Comando remoto LoRa:** `set_relay(command == 0x01)` — idempotente
- **EEPROM:** estado salvo no offset 200, restaurado no boot
- **Feedback:** toda mudança envia `SENSOR_DATA` ao hub imediatamente

### Pareamento

- Boot: envia `PAIR_REQUEST` a cada 5s (máx 20 tentativas)
- Recebe `PAIR_RESPONSE` → salva slot, marca `paired = true`
- Botão 'p' no serial: reseta `paired`, reinicia pareamento
- Hub armazena node em `sensor_registry` com MAC, tipo, slot

### Heartbeat

- Envia `MSG_HEARTBEAT` a cada 60s enquanto pareado
- Se hub não receber por >5min → node repete pareamento

## 8. Estrutura de Arquivos

```
nodes/onoff-lora-avr/
├── platformio.ini
├── build.sh
├── flash.sh
├── monitor.sh
├── include/
│   ├── config.h          # Pinos, intervalos, defines
│   └── lora_avr.h        # Protocolo, structs, funções LoRa
├── src/
│   ├── main.cpp          # Setup, loop, console, relé, botão
│   └── lora_avr.cpp      # Comunicação LoRa via SoftwareSerial
└── firmware/
    └── lora_bridge/       # Firmware ATMega168 (módulo Seeed)
        ├── lora_bridge.ino
        └── README.md
```

### platformio.ini

```ini
[env:nano_lora]
platform = atmelavr
board = nanoatmega328
framework = arduino
monitor_speed = 115200
build_flags =
    -DLORA_RX_PIN=2
    -DLORA_TX_PIN=3
    -DRELAY_PIN=5
    -DBUTTON_PIN=6
    -DLED_PIN=4
    -DLORA_FREQ=868.0
    -DLORA_SF=10
    -DLORA_BW=125
    -DLORA_CR=7
    -DLORA_TX_POWER=17
    -DDEVICE_NAME=\"LoRa AVR Switch\"
```

### EEPROM Layout

| Offset | Tamanho | Conteúdo |
|--------|---------|----------|
| 200 | 1 byte | Relay state (0 ou 1) |
| 201 | 1 byte | Paired flag (0 ou 1) |
| 202-207 | 6 bytes | My MAC (signature bytes) |

## 9. Riscos e Trade-offs

| Risco | Impacto | Mitigação |
|-------|---------|-----------|
| ATMega168 firmware custom | Precisa gravar via ISP | Usar Arduino Nano como ISP ou gravador USBASP |
| SoftwareSerial + LoRa | CPU-intensive, pode perder bytes | 9600 baud (lento), buffer suficiente para 200 bytes |
| Sem WiFi/dashboard | Só console serial para debug | Futuro: módulo ESP01 como co-processor WiFi |
| RAM limitada (2KB) | Protocolo leve, sem JSON | Usar structs packed, sem ArduinoJson |
| Flash limitada (32KB) | Código enxuto | ~10KB estimado, sobra para futuras features |
| Sem OTA | Atualização via cable/ISP | Trade-off aceitável para node simples |
| Device ID fixo | Usa signature do ATmega328P | Único por chip, mas não configurável |
| shared/ não usado | Código independente, duplica protocolo | Aceitável — AVR não compartilha código com ESP |

## 10. Hub — Alterações Necessárias

O hub existente **não precisa de alterações** — ele já recebe `lora_frame_t` via `Sandeepmistry/LoRa` e processa `MSG_PAIR_REQUEST`, `MSG_SENSOR_DATA`, `MSG_HEARTBEAT` no `lora_rx_cb`. O node AVR envia exatamente o mesmo formato.

Única verificação necessária: o hub deve aceitar `radio_type = RADIO_LORA` para nodes AVR (já suportado no `sensor_registry_add`).

## 11. Dependências

### Arduino Nano (platformio)
- Nenhuma lib externa — tudo via `SoftwareSerial.h`, `SPI.h`, `EEPROM.h`

### Firmware ATMega168
- Nenhuma lib externa — código raw SPI direto ao SX1276 via registradores
