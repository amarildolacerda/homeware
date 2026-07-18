# Complete SPEC_ESPNOW — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align the ESP-NOW protocol implementation with the SPEC_ESPNOW.md specification by closing 7 documented gaps across gateway, clients, and repeater.

**Architecture:** 3 phases delivered sequentially — (1) doc-only, (2) gateway changes (pairing flag + unicast decision), (3) client/repeater changes (restart confirm, NAK, loss notification).

**Tech Stack:** C++ (ESP8266/ESP32 Arduino), ESP-NOW, PlatformIO

## Global Constraints

- `FW_VERSION` idêntico entre gateway, bridge e clients (v0.0.25)
- `device_name`/name com 32 bytes
- Loop non-blocking (`millis()`, sem `delay()`)
- Broadcast para ESP8266→ESP32, unicast para ESP8266→ESP8266
- EEPROM layout no gateway: offsets defined in `gateway/include/config.h`

---
## File Structure

### Modified files per task:

| Task | File | Change |
|------|------|--------|
| 1 | `gateway/SPEC_ESPNOW.md` | Update msg_type and sensor_type tables |
| 2 | `gateway/include/config.h` | Add EEPROM_PAIRING_EN_OFFSET |
| 2 | `gateway/src/web_server.cpp` | Add `/api/config/pairing` + `/api/info` field |
| 2 | `gateway/src/espnow_handler.cpp` | Check pairing flag in PAIR_REQUEST + NAK response |
| 3 | `gateway/include/espnow_handler.h` | Declare `espnow_dest_for_chip()` |
| 3 | `gateway/src/espnow_handler.cpp` | Implement helper + apply to command/restart |
| 4 | `clients/*/src/main.cpp` (6 files) | Heartbeat before restart |
| 5 | `clients/esp8266_repeater/src/main.cpp` | NAK on no-gateway + PAIR_RESPONSE forwarding |
| 6 | `clients/esp8266_repeater/src/main.cpp` | NAK broadcast on gateway loss |

---

### Task 1: Fase 1 — Atualizar tabelas msg_type e sensor_type na SPEC

**Files:**
- Modify: `gateway/SPEC_ESPNOW.md`

**Interfaces:**
- Consumes: (none)
- Produces: SPEC document matching implementation

- [ ] **Step 1: Adicionar 0x0C RESTART e 0x0D NAK na tabela §4**

Editar a tabela em `gateway/SPEC_ESPNOW.md` linha 113-124. Adicionar após a linha do REPEATER_STATUS:

```markdown
| 0x0C | `ESPNOW_MSG_RESTART` | Gateway → Client | Reinicia dispositivo alvo |
| 0x0D | `ESPNOW_MSG_NAK` | Gateway/Repeater → Client | Negativa de pareamento/falha |
```

- [ ] **Step 2: Adicionar SENSOR_TYPE_DHT_RELE na tabela §5**

Editar a tabela em `gateway/SPEC_ESPNOW.md` linha 128-140. Adicionar após a linha do REPEATER:

```markdown
| 11 | `SENSOR_TYPE_DHT_RELE` | (reservado) | temperature, humidity, state |
```

- [ ] **Step 3: Commit**

```bash
git add gateway/SPEC_ESPNOW.md
git commit -m "docs(spec): add RESTART/NAK msg types and DHT_RELE sensor type
```

---

### Task 2: Fase 2 Item A — Flag pairing_enabled no gateway

**Files:**
- Modify: `gateway/include/config.h`
- Modify: `gateway/src/web_server.cpp`
- Modify: `gateway/src/espnow_handler.cpp`

**Interfaces:**
- Consumes: `espnow_nak_t` / `NAK_REASON_PAIRING_DISABLED` from `shared/espnow_protocol.h`
- Produces: `pairing_config_load()`, `pairing_config_save()`, `GET/POST /api/config/pairing`, `pairing_required` field in `/api/info`

- [ ] **Step 1: Adicionar EEPROM_PAIRING_EN_OFFSET em config.h**

Em `gateway/include/config.h`, adicionar após `EEPROM_WIFI_DNS_SIZE` e atualizar `EEPROM_SIZE`:

```c
#define EEPROM_PAIRING_EN_OFFSET (EEPROM_WIFI_DNS_OFFSET + EEPROM_WIFI_DNS_SIZE)
#define EEPROM_SIZE (EEPROM_PAIRING_EN_OFFSET + 1)
```

- [ ] **Step 2: Adicionar funções de load/save da flag em web_server.cpp**

Em `gateway/src/web_server.cpp`, adicionar antes de `web_server_init()`:

```cpp
#define PAIRING_ENABLED_DEFAULT false

static bool pairing_config_load() {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t val = EEPROM.read(EEPROM_PAIRING_EN_OFFSET);
    EEPROM.end();
    if (val == 0 || val == 1) return (bool)val;
    return PAIRING_ENABLED_DEFAULT;
}

static void pairing_config_save(bool enabled) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.write(EEPROM_PAIRING_EN_OFFSET, enabled ? 1 : 0);
    EEPROM.commit();
    EEPROM.end();
}
```

- [ ] **Step 3: Adicionar endpoints GET/POST /api/config/pairing em web_server.cpp**

Em `gateway/src/web_server.cpp`, dentro de `web_server_init()`, adicionar após o bloco `/api/config/wifi` (após linha ~449):

```cpp
    s_server.on("/api/config/pairing", HTTP_GET, []() {
        JsonDocument doc;
        doc["enabled"] = pairing_config_load();
        String json;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
    });

    s_server.on("/api/config/pairing", HTTP_POST, []() {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (err || !doc.containsKey("enabled")) {
            s_server.send(400, "application/json", "{\"error\":\"enabled required\"}");
            return;
        }
        bool enabled = doc["enabled"];
        pairing_config_save(enabled);
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
    });
```

- [ ] **Step 4: Adicionar pairing_required ao /api/info**

Em `gateway/src/web_server.cpp`, no handler de `/api/info`, adicionar no JsonDocument:

```cpp
        doc["pairing_required"] = pairing_config_load();
```

- [ ] **Step 5: Aplicar flag no espnow_recv_cb**

Em `gateway/src/espnow_handler.cpp`, no `case ESPNOW_MSG_PAIR_REQUEST:`, após o early-return de MAC já existente (linha ~84), adicionar verificação:

```cpp
            /* Pairing flag check (SPEC_ESPNOW §6) */
            {
                EEPROM.begin(EEPROM_SIZE);
                bool pairing_required = EEPROM.read(EEPROM_PAIRING_EN_OFFSET) == 1;
                EEPROM.end();
                if (pairing_required && !s_pairing_mode) {
                    console.printf("[ESP-NOW] Pair request ignored (pairing disabled, window closed)\n");
                    espnow_nak_t nak;
                    memset(&nak, 0, sizeof(nak));
                    nak.msg_type = ESPNOW_MSG_NAK;
                    nak.sequence = req->sequence;
                    mac_copy(nak.target_mac, req->sensor_mac);
                    nak.reason = NAK_REASON_PAIRING_DISABLED;
                    esp_now_send((uint8_t*)s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
                    return;
                }
            }
```

Adicionar `#include "config.h"` no topo se não existir (já existe).

- [ ] **Step 6: Verificar build**

```bash
pio run -e esp32_gateway
```

Se falhar pelo `build_shared.py` (erro pre-existente), pelo menos verificar com `pio check` ou compilar manualmente os arquivos alterados.

- [ ] **Step 7: Commit**

```bash
git add gateway/include/config.h gateway/src/web_server.cpp gateway/src/espnow_handler.cpp
git commit -m "feat(gateway): add pairing_enabled flag (EEPROM + API + NAK on disabled)
```

---

### Task 3: Fase 2 Item B — Helper unicast/broadcast por client_chip

**Files:**
- Modify: `gateway/include/espnow_handler.h`
- Modify: `gateway/src/espnow_handler.cpp`

**Interfaces:**
- Consumes: `virtual_sensor_t.client_chip` (já populado), `espnow_add_peer_wrapper()`
- Produces: `espnow_dest_for_chip(mac, client_chip)` → pointer to MAC address

- [ ] **Step 1: Declarar helper no header**

Em `gateway/include/espnow_handler.h`, adicionar antes de `espnow_send_command`:

```cpp
const uint8_t* espnow_dest_for_chip(const uint8_t *mac, uint8_t client_chip);
```

- [ ] **Step 2: Implementar helper em espnow_handler.cpp**

Em `gateway/src/espnow_handler.cpp`, adicionar antes de `espnow_send_command()`:

```cpp
const uint8_t* espnow_dest_for_chip(const uint8_t *mac, uint8_t client_chip) {
    if (client_chip == HW_CHIP_ESP8266) {
        /* Unicast funciona para ESP8266→ESP8266 e ESP32→ESP8266 */
        espnow_add_peer_wrapper((uint8_t*)mac, WiFi.channel());
        return mac;
    }
    /* ESP32 destino ou desconhecido → broadcast seguro */
    return s_bcast_addr;
}
```

- [ ] **Step 3: Aplicar em espnow_send_command**

Modificar `espnow_send_command()` em `gateway/src/espnow_handler.cpp` para usar o helper:

```cpp
bool espnow_send_command(const uint8_t *mac, uint8_t slot, uint8_t state) {
    ...
    mac_copy(cmd.target_mac, mac);
    cmd.command = state;

    virtual_sensor_t *s = sensor_registry_get(slot);
    uint8_t chip = (s && s->paired) ? s->client_chip : HW_CHIP_UNKNOWN;
    const uint8_t *dest = espnow_dest_for_chip(mac, chip);

    int ret = esp_now_send((uint8_t*)dest, (uint8_t*)&cmd, sizeof(cmd));
    ...
}
```

- [ ] **Step 4: Aplicar em espnow_send_restart**

Modificar `espnow_send_restart()` da mesma forma:

```cpp
bool espnow_send_restart(const uint8_t *mac, uint8_t slot) {
    ...
    mac_copy(rst.target_mac, mac);

    virtual_sensor_t *s = sensor_registry_get(slot);
    uint8_t chip = (s && s->paired) ? s->client_chip : HW_CHIP_UNKNOWN;
    const uint8_t *dest = espnow_dest_for_chip(mac, chip);

    int ret = esp_now_send((uint8_t*)dest, (uint8_t*)&rst, sizeof(rst));
    ...
}
```

Note: a declaração no header precisa mudar para `bool espnow_send_restart(const uint8_t *mac, uint8_t slot);` e o caller em web_server.cpp precisa passar o slot.

- [ ] **Step 5: Atualizar web_server.cpp para passar slot ao espnow_send_restart**

Em `gateway/src/web_server.cpp`, no handler restart, alterar:

```cpp
            if (espnow_send_restart(s->mac, s->slot)) {
```

- [ ] **Step 6: Verificar build e commit**

```bash
pio run -e esp32_gateway
git add gateway/include/espnow_handler.h gateway/src/espnow_handler.cpp gateway/src/web_server.cpp
git commit -m "feat(gateway): unicast decision by client_chip (ESP8266=unicast, ESP32=broadcast)
```

---

### Task 4: Fase 3 Item A — Heartbeat antes de restart em todos os clients

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp`
- Modify: `clients/esp8266_onoff/src/main.cpp`
- Modify: `clients/esp8266_chuva/src/main.cpp`
- Modify: `clients/esp8266_dht_gas/src/main.cpp`
- Modify: `clients/esp8266_pir/src/main.cpp`
- Modify: `clients/esp8266_repeater/src/main.cpp`

**Interfaces:**
- Consumes: `ESPNOW_MSG_RESTART` handler (já adicionado em todos), `espnow_send_heartbeat()`
- Produces: last-state send before restart

- [ ] **Step 1: Lampada — adicionar heartbeat antes de restart**

Em `clients/esp8266_lampada/src/main.cpp`, no `case ESPNOW_MSG_RESTART:`, antes de `ESP.restart()`:

```cpp
            if (s_paired) {
                espnow_send_heartbeat();
                delay(50);
            }
            delay(100);
            ESP.restart();
```

- [ ] **Step 2: Onoff — mesma alteração**

Em `clients/esp8266_onoff/src/main.cpp`, mesmo local, mesmo código.

- [ ] **Step 3: Chuva — mesma alteração**

Em `clients/esp8266_chuva/src/main.cpp`, mesmo local, mesmo código.

- [ ] **Step 4: DHT Gas — mesma alteração**

Em `clients/esp8266_dht_gas/src/main.cpp`, mesmo local, mesmo código.

- [ ] **Step 5: PIR — mesma alteração**

Em `clients/esp8266_pir/src/main.cpp`, mesmo local, mesmo código.

- [ ] **Step 6: Repeater — enviar REPEATER_STATUS antes de restart**

Em `clients/esp8266_repeater/src/main.cpp`, no restart check, antes de `ESP.restart()`:

```cpp
    if (msg_type == ESPNOW_MSG_RESTART && len >= sizeof(espnow_restart_t))
    {
        espnow_restart_t *rst = (espnow_restart_t *)data;
        if (mac_equal(rst->target_mac, s_my_mac))
        {
            console.printf("[%s] Restart command received, rebooting...\n", TAG);
            /* Enviar último status antes de resetar */
            espnow_send_repeater_status();
            delay(50);
            ESP.restart();
        }
    }
```

Note: verificar se `espnow_send_repeater_status()` existe no repeater. Se chamar de forma diferente, usar o nome correto.

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_lampada/src/main.cpp clients/esp8266_onoff/src/main.cpp clients/esp8266_chuva/src/main.cpp clients/esp8266_dht_gas/src/main.cpp clients/esp8266_pir/src/main.cpp clients/esp8266_repeater/src/main.cpp
git commit -m "feat(clients): send heartbeat before ESP.restart() on restart command
```

---

### Task 5: Fase 3 Item B — Repeater NAK + pair forwarding

**Files:**
- Modify: `clients/esp8266_repeater/src/main.cpp`

**Interfaces:**
- Consumes: `ESPNOW_MSG_NAK`, `NAK_REASON_NO_GATEWAY`, `espnow_nak_t`, `lookup_sequence()`/`fwd_push()`
- Produces: NAK response on PAIR_REQUEST when gateway absent

- [ ] **Step 1: Adicionar constante GATEWAY_TIMEOUT_MS**

Em `clients/esp8266_repeater/src/main.cpp`, adicionar na seção de defines/globals:

```cpp
#define GATEWAY_TIMEOUT_MS 60000
```

- [ ] **Step 2: Adicionar verificação de gateway no tratamento de PAIR_REQUEST**

No `espnow_recv_cb` do repeater, antes de aprender peers (após a extração de `msg_type`/`sequence`), adicionar:

```cpp
    /* NAK se não temos gateway (SPEC_ESPNOW §13.2) */
    if (msg_type == ESPNOW_MSG_PAIR_REQUEST && len >= sizeof(espnow_pair_request_t))
    {
        if (!s_gateway_configured || (millis() - s_last_gateway_comm > GATEWAY_TIMEOUT_MS))
        {
            espnow_pair_request_t *req = (espnow_pair_request_t *)data;
            espnow_nak_t nak;
            memset(&nak, 0, sizeof(nak));
            nak.msg_type = ESPNOW_MSG_NAK;
            nak.sequence = req->sequence;
            mac_copy(nak.target_mac, req->sensor_mac);
            nak.reason = NAK_REASON_NO_GATEWAY;
            esp_now_send(s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
            console.printf("[%s] NAK sent: no gateway for PAIR_REQUEST from %02X:%02X:%02X:%02X:%02X:%02X\n",
                           TAG, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;  /* não repassa */
        }
        /* Se tem gateway, deixa o fluxo normal de forwarding repassar */
    }
```

A inserção deve ser feita **após** a extração da `sequence` (para ter acesso a `req->sequence`) e **antes** das verificações de forwarding. O local correto é após o bloco `else if (msg_type == ESPNOW_MSG_GW_ANNOUNCE ...)` (logo após a linha `return;` desse bloco) e antes do `if (s_gateway_configured ...)`.

- [ ] **Step 3: Commit**

```bash
git add clients/esp8266_repeater/src/main.cpp
git commit -m "feat(repeater): NAK on PAIR_REQUEST when gateway absent (§13.2)
```

---

### Task 6: Fase 3 Item C — Repeater: notificar clients sobre queda do gateway

**Files:**
- Modify: `clients/esp8266_repeater/src/main.cpp`

**Interfaces:**
- Consumes: `NAK_REASON_GATEWAY_LOST`, `espnow_send()`, `s_client_peers`
- Produces: broadcast NAK on gateway timeout

- [ ] **Step 1: Adicionar lógica de detecção na loop() do repeater**

Em `clients/esp8266_repeater/src/main.cpp`, na função `loop()`, adicionar verificação periódica (ex: a cada 5s):

```cpp
    /* §13.3: detectar queda do gateway e notificar clients */
    {
        static unsigned long s_last_gateway_check = 0;
        if (now - s_last_gateway_check > 5000) {
            s_last_gateway_check = now;
            if (s_gateway_configured && (now - s_last_gateway_comm > GATEWAY_TIMEOUT_MS)) {
                s_gateway_configured = false;
                console.printf("[%s] Gateway lost! Notifying clients...\n", TAG);
                espnow_nak_t nak;
                memset(&nak, 0, sizeof(nak));
                nak.msg_type = ESPNOW_MSG_NAK;
                nak.sequence = 0;
                memset(nak.target_mac, 0xFF, 6);  /* broadcast */
                nak.reason = NAK_REASON_GATEWAY_LOST;
                esp_now_send(s_bcast_addr, (uint8_t*)&nak, sizeof(nak));
            }
        }
    }
```

- [ ] **Step 2: Adicionar handling de NAK nos clients comuns**

Em cada client (lampada, onoff, chuva, dht_gas, pir), adicionar no `espnow_recv_cb` handler de `ESPNOW_MSG_NAK`:

```cpp
    case ESPNOW_MSG_NAK:
    {
        if (len < sizeof(espnow_nak_t)) return;
        espnow_nak_t *nak = (espnow_nak_t *)data;
        if (nak->reason == NAK_REASON_GATEWAY_LOST) {
            console.printf("[%s] Gateway lost notification (NAK), re-pairing...\n", TAG);
            s_paired = false;
            s_gateway_connected = false;
            s_pair_attempts = 0;
        }
        break;
    }
```

Inserir no switch de cada client. Para lampada e onoff, inserir antes de `ESPNOW_MSG_TIME_SYNC`. Para chuva/dht_gas/pir, inserir após o `case ESPNOW_MSG_ACK`.

- [ ] **Step 3: Commit**

```bash
git add clients/esp8266_repeater/src/main.cpp clients/esp8266_lampada/src/main.cpp clients/esp8266_onoff/src/main.cpp clients/esp8266_chuva/src/main.cpp clients/esp8266_dht_gas/src/main.cpp clients/esp8266_pir/src/main.cpp
git commit -m "feat(repeater): notify clients on gateway loss (§13.3) + NAK handling in clients
```

---

## Self-Review Checklist

- [ ] **Spec coverage:** Task 1 → §4/§5; Task 2 → §6; Task 3 → §14; Task 4 → §9.1; Task 5 → §13.2; Task 6 → §13.3. Todos os 7 gaps cobertos.
- [ ] **No placeholders:** All steps have actual code, not TODOs.
- [ ] **Type consistency:** `espnow_send_restart` signature changed to include `slot` param — both declaration and callers updated.
- [ ] **Build check:** Tasks 2-3 modify gateway; Tasks 4-6 modify clients.
