# Completar SPEC_ESPNOW — Design

## Objetivo

Implementar todos os 7 gaps identificados entre a `SPEC_ESPNOW.md` e a
implementação real do protocolo ESP-NOW (gateway + clients + repeater).

## Estrutura

3 fases independentes, entregues em ordem, cada uma testável isoladamente.

---

## Fase 1 — Protocolo + Documentação

### §4 — Atualizar tabela msg_type

Adicionar entradas faltantes na tabela da SPEC_ESPNOW.md:

| 0x0C | `ESPNOW_MSG_RESTART` | Gateway → Client | Reinicia dispositivo alvo |
| 0x0D | `ESPNOW_MSG_NAK` | Gateway/Repeater → Client | Negativa de pareamento/falha |

### §5 — Adicionar SENSOR_TYPE_DHT_RELE

Adicionar linha na tabela (type reservado — existe no enum mas sem
implementação de client ou payload específico ainda):

| 11 | `SENSOR_TYPE_DHT_RELE` | (reservado) | temperature, humidity, state |

### Validação

- `shared/espnow_protocol.h` já tem `ESPNOW_MSG_RESTART = 0x0C`,
  `ESPNOW_MSG_NAK = 0x0D` e `espnow_restart_t` / `espnow_nak_t`.
- `SENSOR_TYPE_DHT_RELE = 11` já definido no enum.
- Nenhuma alteração de código necessária — apenas documentação.

---

## Fase 2 — Gateway

### §6 — Flag pairing_enabled

**Objetivo**: Flag persistente em EEPROM que controla se o gateway exige modo
de pareamento (`s_pairing_mode`) para aceitar `PAIR_REQUEST`.

**Locais de mudança** (`gateway/`):

1. **`include/config.h`**: Definir:
   - `EEPROM_PAIRING_EN_OFFSET` (após `EEPROM_WIFI_DNS_OFFSET +
     EEPROM_WIFI_DNS_SIZE`)
   - `PAIRING_ENABLED_DEFAULT` = `false` (Fase 1, aceita tudo)

2. **`src/web_server.cpp`**:
   - Novo endpoint `GET /api/config/pairing` → retorna `{"enabled": bool}`
   - Novo endpoint `POST /api/config/pairing` → recebe `{"enabled": bool}`,
     salva em EEPROM
   - Em `/api/info`, adicionar campo `"pairing_required": bool`

3. **`src/espnow_handler.cpp`**:
   - No RX callback, `case ESPNOW_MSG_PAIR_REQUEST`:
     - Se `pairing_enabled == true && !s_pairing_mode` → responder NAK
       (`ESPNOW_MSG_NAK` com `NAK_REASON_PAIRING_DISABLED`) e ignorar
     - Se `pairing_enabled == false` → comportamento atual (aceita sempre)

4. **Persistência**: nova função `pairing_config_load()` / `pairing_config_save()`
   seguindo padrão de `wifi_creds_load/save`.

### §14 — Helper de decisão unicast/broadcast

**Objetivo**: Substituir envio sempre-broadcast por decisão baseada em
`client_chip` do sensor destino.

**Regra**:
- `client_chip == HW_CHIP_ESP8266` → **unicast** para `s->mac`
- `client_chip == HW_CHIP_ESP32` → **broadcast**
- `client_chip == HW_CHIP_UNKNOWN` → **broadcast** (fallback seguro)

**Locais de mudança** (`gateway/`):

1. **`include/espnow_handler.h`**:
   - Declarar `const uint8_t* espnow_dest_for_chip(const uint8_t *mac,
     uint8_t client_chip)`

2. **`src/espnow_handler.cpp`**:
   - Implementar helper
   - Antes de cada `esp_now_send()` que envia para sensor conhecido:
     - `espnow_send_command()` → usar helper
     - `espnow_send_restart()` → usar helper
     - `send_ack()` → continuar broadcast (ACK é broadcast, não endereçado)
     - `send_pair_response()` → continuar broadcast
   - Antes de unicast, garantir peer adicionado no canal via
     `espnow_add_peer_wrapper(dest, WiFi.channel())`

3. **Garantia**: `virtual_sensor_t.client_chip` já é populado pelo
   `espnow_pair_request_t.client_chip` que todos os clients enviam.

---

## Fase 3 — Clients + Repeater

### §9.1 — Client: heartbeat antes de restart

**Objetivo**: Client enviar heartbeat/último estado antes de resetar, para
gateway marcar `online=false` imediatamente.

**Mudança em todos os 6 clients** (lampada, onoff, chuva, dht_gas, pir,
repeater):

No handler de `ESPNOW_MSG_RESTART` no `espnow_recv_cb`, antes do
`ESP.restart()`:

```cpp
if (s_paired) {
    espnow_send_heartbeat();  // ou espnow_send_data() para atuadores
    delay(50);  // best-effort: tempo p/ ESP-NOW enviar quadro antes de resetar
}
ESP.restart();
```

Repeater: antes de resetar, enviar `ESPNOW_MSG_REPEATER_STATUS` atualizado.

### §13.2 — Repeater: NAK + pair forwarding

**Objetivo**: Repeater deve responder NAK quando não tem gateway, e repassar
`PAIR_RESPONSE` do gateway ao client originário.

**Mudanças** (`clients/esp8266_repeater/src/main.cpp`):

1. **NAK quando sem gateway**:
   - Constante `GATEWAY_TIMEOUT_MS = 60000` (já usado
     internamente como `s_last_gateway_comm` timeout).
   - No `espnow_recv_cb`, ao receber `ESPNOW_MSG_PAIR_REQUEST`:
     - Se `!s_gateway_configured || (millis() - s_last_gateway_comm >
       GATEWAY_TIMEOUT_MS)` → enviar `ESPNOW_MSG_NAK` com
       `NAK_REASON_NO_GATEWAY` para o sender
     - Se gateway OK → deixar o fluxo normal de forwarding

2. **Repassar PAIR_RESPONSE**:
   - A `PAIR_RESPONSE` do gateway já é encaminhada pelo fluxo existente
     (gateway → broadcast → repeater → forward para clients).
   - Verificar se `lookup_sequence()` no RX callback consegue casar a
     `PAIR_RESPONSE.sequence` com o `PAIR_REQUEST` original do client.
     - Se sim: unicast para o MAC do client
     - Se não: broadcast para todos os clients

3. **Timeout de gateway**: já existe `s_last_gateway_comm` (60s). Usar como
   indicador de "gateway ausente".

### §13.3 — Repeater: notificar clients sobre queda do gateway

**Objetivo**: Quando o repeater detecta perda do gateway, enviar NAK broadcast
avisando clients que precisam re-parear.

**Mudanças** (`clients/esp8266_repeater/src/main.cpp`):

1. Na `loop()`, verificar periodicamente:
   - Se `s_gateway_configured && gateway_timeout_since(s_last_gateway_comm)`:
     - `s_gateway_configured = false`
     - Enviar `ESPNOW_MSG_NAK` broadcast com
       `NAK_REASON_GATEWAY_LOST`
     - Limpar peers de clients (forçar novo pareamento)

2. **NAK nos clients**: clients comuns (lampada, onoff, etc.) ao receber
   `ESPNOW_MSG_NAK` com `NAK_REASON_GATEWAY_LOST` devem:
   - Marcar `s_paired = false`, `s_gateway_connected = false`
   - Reiniciar máquina de pareamento

---

## Especificação dos tipos NAK

### espnow_nak_t (já definido em espnow_protocol.h)

```c
typedef struct __attribute__((packed)) {
    uint8_t msg_type;     // ESPNOW_MSG_NAK = 0x0D
    uint16_t sequence;
    uint8_t target_mac[6];
    uint8_t reason;       // nak_reason_t
} espnow_nak_t;
```

### nak_reason_t (já definido)

```c
NAK_REASON_NONE = 0,
NAK_REASON_NO_GATEWAY = 1,
NAK_REASON_GATEWAY_LOST = 2,
NAK_REASON_PAIRING_DISABLED = 3,
NAK_REASON_FULL = 4,
```

---

## Arquivos alterados (resumo)

| Fase | Arquivo | Mudança |
|------|---------|---------|
| 1 | `gateway/SPEC_ESPNOW.md` | Tabelas §4 e §5 |
| 2 | `gateway/include/config.h` | EEPROM_PAIRING_EN_OFFSET |
| 2 | `gateway/include/espnow_handler.h` | `espnow_dest_for_chip()` |
| 2 | `gateway/src/web_server.cpp` | `/api/config/pairing` + `/api/info` |
| 2 | `gateway/src/espnow_handler.cpp` | Flag pairing + unicast helper |
| 3 | `clients/*/src/main.cpp` (6x) | Heartbeat antes de restart |
| 3 | `clients/esp8266_repeater/src/main.cpp` | NAK + pair forwarding + loss notify |

---

## Não escopo

- Bridge Python (`bridge/app/`) — não mexe com ESP-NOW diretamente
- Dashboard frontend — botão "pairing_required" seria exibido/oculto pelo
  frontend, mas isso é decisão de UI futura

---

## Teste

- Fase 1: revisão visual da SPEC
- Fase 2: testar pareamento com flag on/off; verificar unicast vs broadcast
  no monitor serial
- Fase 3: testar restart via dashboard; testar repeater sem gateway (deve
  NAK); testar queda do gateway (clients devem re-parear ao voltar)
