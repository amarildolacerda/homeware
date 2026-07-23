# Timer System + Lamp Sync — Design Spec

**Data:** 2026-07-21
**Clients:** `esp8266_onoff`, `esp8266_lampada`
**Status:** Draft

## 1. Visão Geral

Três tipos de timer + feature de sincronização entre lamps:

| Feature | Persistência | Escopo |
|---------|-------------|--------|
| Agenda | LittleFS | onoff + lampada |
| Cíclico | LittleFS | onoff + lampada |
| Pulsação | RAM (volátil) | onoff + lampada |
| Lamp Sync | LittleFS | lampada (emissor) |

## 2. Timer: Agenda

Mantém a lógica existente em `shared/timer.h/.cpp` (já testada). Migração de EEPROM para LittleFS.

- Máx 6 slots
- Config: hour, minute, action (ON/OFF), days_mask, enabled
- Requer epoch do gateway (`ESPNOW_MSG_TIME_SYNC`)
- Check a cada 10s no loop

### Dados (LittleFS `/timers.json`)

```json
{
  "timers": [
    { "hour": 6, "minute": 30, "action": 1, "days_mask": 0, "enabled": true },
    { "hour": 22, "minute": 0, "action": 0, "days_mask": 127, "enabled": true }
  ]
}
```

## 3. Timer: Cíclico

Alterna ON/OFF automaticamente com intervalo único (em minutos).

- Config: duration_min (uint16), enabled (bool)
- Lógica: quandoliga, agenda próximo OFF em duration_min. Quando desliga, agenda próximo ON em duration_min.
- Qualquer transição para OFF reseta o countdown do Cíclico
- Check a cada 1s no loop (millis-based)
- Persiste em LittleFS (sobrevive reboot/OTA)

### Estado volátil (RAM)

```c
bool     s_cyclic_enabled;
uint16_t s_cyclic_duration_min;
unsigned long s_cyclic_next_event;   // millis() do próximo toggle
bool     s_cyclic_waiting_off;      // true=aguardando OFF, false=aguardando ON
```

### Dados (LittleFS `/timers.json`, junto com Agenda)

```json
{
  "cyclic": { "enabled": true, "duration_min": 30 }
}
```

## 4. Timer: Pulsação

Auto-OFF após X minutos. Mantém lógica existente no onoff (já testada).

- Config: duration_min (uint16), enabled (bool)
- Quando relay liga → reseta countdown
- Quando relay desliga → reseta Cíclico (se ativo)
- Volátil (RAM) — mas config salva em LittleFS para restaurar após reboot
- Check a cada 1s no loop

### Estado volátil (RAM)

```c
bool     s_pulse_enabled;
uint16_t s_pulse_duration_min;
unsigned long s_pulse_on_time;       // millis() quando ligou
```

### Dados (LittleFS `/timers.json`, junto com Agenda)

```json
{
  "pulse": { "enabled": true, "duration_min": 5 }
}
```

## 5. Interações entre Timers

```
set_relay(true)  → reseta s_pulse_on_time (inicia countdown auto-OFF)
set_relay(false) → reseta s_cyclic_next_event (inicia countdown próximo ON)
```

Cenário exemplo (Cíclico 30min + Pulsação 5min):
1. Cíclico dispara ON → `set_relay(true)` → Pulsação inicia 5min
2. Após 5min → Pulsação dispara OFF → `set_relay(false)` → Cíclico inicia 30min
3. Após 30min → Cíclico dispara ON → repete ciclo

## 6. Lamp Sync (Sincronização)

Lamp A espelha estado em Lamp B. Qualquer mudança de estado em A (qualquer origem) envia comando ESP-NOW para B via gateway.

### Config

- `sync_device_id[32]` — device_id do lamp alvo (ex: `esp8266_aabbcc`)
- Empty = sync desabilitado
- Salvo em LittleFS `/timers.json`

### Fluxo

1. `set_relay()` em A detecta mudança de estado
2. A envia `ESPNOW_MSG_COMMAND` ao gateway com `target_mac` = MAC de B
3. Gateway encaminha comando para B
4. B aplica estado

### MAC Resolution

O gateway mantém registry de devices. A precisa do MAC de B para enviar comando. Duas opções:
- A envia comando ao gateway com o `device_id` de B, gateway resolve MAC
- A guarda o MAC de B (descoberto via registry)

**Decisão:** usar `device_id` — gateway resolve MAC no `ESPNOW_MSG_COMMAND`. Mais flexível, não precisa de sync de MAC.

### Dados (LittleFS `/timers.json`)

```json
{
  "sync": { "enabled": true, "target_id": "esp8266_aabbcc" }
}
```

## 7. Persistência LittleFS

### Arquivo: `/timers.json`

```json
{
  "timers": [ ... ],
  "cyclic": { "enabled": false, "duration_min": 30 },
  "pulse": { "enabled": false, "duration_min": 5 },
  "sync": { "enabled": false, "target_id": "" }
}
```

### Migração de EEPROM

- Timer (Agenda): migrar de EEPROM offset 161+ para LittleFS
- Pulse: migrar de EEPROM offset 224-226 para LittleFS
- Após primeira leitura LittleFS com sucesso, limpar EEPROM antiga

### Leitura

- `setup()` → `LittleFS.begin()` → ler `/timers.json` → popular structs
- Se arquivo não existe → defaults
- Se JSON inválido → defaults + log erro

### Escrita

- Qualquer mudança via API → gravar `/timers.json` completo
- Usar `serializeJson()` do ArduinoJson

## 8. API

### GET `/api/timers`

```json
{
  "timers": [ { "hour":6, "minute":30, "action":1, "days_mask":0, "enabled":true } ],
  "cyclic": { "enabled":true, "duration_min":30 },
  "pulse": { "enabled":true, "duration_min":5, "remaining_s": 150 },
  "sync": { "enabled":false, "target_id":"" }
}
```

### POST `/api/timers`

Body parcial — só campos enviados são atualizados:

```json
{
  "timers": [ ... ],
  "cyclic": { "enabled":true, "duration_min":30 },
  "pulse": { "enabled":true, "duration_min":5 },
  "sync": { "enabled":true, "target_id":"esp8266_aabbcc" }
}
```

### GET `/api/timer/next`

```json
{
  "has_next": true,
  "next_epoch": 1690000000,
  "next_action": 1,
  "source": "cyclic"
}
```

### GET/POST `/api/pulse`

Mantido para compatibilidade. GET retorna status, POST configura.

## 9. Dashboard (Seção "Ciclo")

Sidebar mantém navegação existente. Seção "Ciclo" com subseções:

### Agenda (existente)
- Lista de timers com toggle enable/disable
- Formulário de adicionar: hora, minuto, ação ON/OFF, dias

### Cíclico (novo)
- Toggle enable/disable
- Input: duração em minutos (1-1440)
- Status: próximo toggle em Xm

### Pulsação (existente, aprimorado)
- Toggle enable/disable
- Input: duração em minutos (1-1440)
- Status: countdown restante

### Sincronização (novo, lampada apenas)
- Toggle enable/disable
- Input: device_id do lamp alvo
- Status: conectado/desconectado

## 10. EEPROM — Limpeza

Após migração para LittleFS:
- Offset 161-223 (timers antigos): limpar com 0xFF
- Offset 224-226 (pulse antigo): limpar com 0xFF
- Flag de migração em LittleFS (`migrated: true`)

## 11. Limitações ESP8266

- LittleFS: 1MB partição, arquivo < 1KB — OK
- RAM: 3 timers (cyclic + pulse + sync state) ~20 bytes — OK
- Check intervals: cyclic/pulse 1s, agenda 10s — não sobrecarrega loop
- JSON: ArduinoJson heap ~512B para parse — OK
