# Hub Operation Mode — Design

## Overview

Adicionar ao hub três modos de operação configuráveis via EEPROM, console e API web:

| Modo | Nome | Descrição |
|------|------|-----------|
| 0 | Terminal | Modo atual — STA conecta ao roteador, envia dados ao bridge via MQTT |
| 1 | Ponto de Acesso | AP puro — recebe dados dos nodes via ESP-NOW, serve dashboard local, sem MQTT |
| 2 | Híbrido | AP + STA simultâneo (WIFI_AP_STA) — recebe via ESP-NOW e envia ao bridge |

## Decisões de Design

- **Canal AP**: Fixo, canal 1
- **SSID AP**: `device_name` (ex: `agri_xxxx_gateway`)
- **Senha AP**: `password123` (fixa)
- **Watchdog**: Desabilitado no modo AP puro; no modo Terminal/Híbrido mantém o comportamento atual (reinicia se sem RX por 5min com sensores pareados)
- **MQTT**: Desabilitado no modo AP puro; habilitado no modo Terminal/Híbrido
- **Dashboard**: Sempre disponível em todos os modos (via IP do STA ou 192.168.4.1 no AP)
- **Persistência**: Modo salvo em EEPROM, persiste após reinício
- **Alternância**: Console comando `m` + API `GET/POST /api/config/mode`

## EEPROM Layout

```
EEPROM_OP_MODE_OFFSET = EEPROM_PAIRING_EN_OFFSET + 1
EEPROM_SIZE = EEPROM_OP_MODE_OFFSET + 1
```

Byte único: `0` = Terminal, `1` = AP, `2` = Híbrido. Default: `0`.

## Fluxo por Modo

### Modo 0 — Terminal (atual)
```
setup() → web_server_wifi_setup() → STA connect (EEPROM → STATIC → fallback) → web_server_init()
loop(): watchdog, MQTT, radio, OTA, dashboard via IP do STA
```

### Modo 1 — Ponto de Acesso
```
setup() → web_server_wifi_setup() → WiFi.mode(WIFI_AP) → softAP(device_name, "password123", canal 1) → web_server_init()
loop(): watchdog DESABILITADO, SEM MQTT, radio (ESP-NOW), dashboard via 192.168.4.1
```

### Modo 2 — Híbrido
```
setup() → web_server_wifi_setup() → WiFi.mode(WIFI_AP_STA) → softAP + STA connect → web_server_init()
loop(): watchdog (se STA conectado), MQTT (se STA conectado), radio, dashboard via IP do STA
```

## Arquivos Afetados

| Arquivo | Mudança |
|---------|---------|
| `hub/include/config.h` | `EEPROM_OP_MODE_OFFSET`, `OP_MODE_*` defines, `AP_CHANNEL`, `AP_PASS` |
| `hub/src/web_server.cpp` | `op_mode_load()`, `op_mode_save()`, modifica `web_server_wifi_setup()` e `web_server_maintain_wifi()` |
| `hub/src/main.cpp` | Comando `m`, watchdog condicional, MQTT condicional |
| `hub/include/web_server.h` | Declara `op_mode_load()`, `op_mode_save()` |

## API

### GET /api/config/mode
```json
{ "mode": 0 }
```

### POST /api/config/mode
```json
{ "mode": 1 }
```
Retorna `{"status":"ok"}` e reinicia após 300ms.

## Console

```
m    - Modo atual: Terminal (0)
m 0  - Modo: Terminal
m 1  - Modo: Ponto de Acesso
m 2  - Modo: Híbrido
```

## Watchdog

- **Modo 0 (Terminal)**: `g_hub_rx_wd.check(rx_healthy)` como atual
- **Modo 1 (AP)**: Sem chamada ao watchdog (pulado no loop)
- **Modo 2 (Híbrido)**: Watchdog roda apenas se `WiFi.status() == WL_CONNECTED` (STA conectado)

## MQTT

- **Modo 0 (Terminal)**: `mqtt_client_connect()` no setup, `mqtt_client_loop()` no loop
- **Modo 1 (AP)**: Sem chamada ao MQTT
- **Modo 2 (Híbrido)**: `mqtt_client_connect()` no setup, `mqtt_client_loop()` no loop (funciona mesmo se STA cai — MQTT reconecta)

## web_server_wifi_setup() — Modificado

```
op_mode = op_mode_load()

switch (op_mode):
  case 0 (Terminal):
    fluxo atual (EEPROM creds → STATIC_WIFI → fallback AP portal)

  case 1 (AP):
    WiFi.mode(WIFI_AP)
    WiFi.softAP(device_name, "password123", 1)
    web_server_init()
    return true

  case 2 (Híbrido):
    WiFi.mode(WIFI_AP_STA)
    WiFi.softAP(device_name, "password123", 1)
    // Depois tenta conectar STA (mesmo fluxo do Terminal)
    WiFi.begin(saved_ssid, saved_pass)
    // Aguarda 20s para STA conectar (não bloqueante — AP já está ativo)
    web_server_init()
    return true
```

## web_server_maintain_wifi() — Modificado

```
op_mode = op_mode_load()

if (op_mode == 1) return;  // AP puro: não reconecta

// Modo 0 e 2: reconecta STA se caiu
fluxo atual de reconexão
```

## Riscos e Mitigações

| Risco | Mitigação |
|-------|-----------|
| ESP8266 não suporta AP+STA simultâneo | Testar — ESP8266 suporta `WIFI_AP_STA` mas com limitações de memória |
| Canal AP conflita com nodes em canal diferente | Canal fixo 1 — todos os devices devem estar no mesmo canal |
| Dashboard consome memória no AP | Dashboard já é compacto (regra 28); monitorar heap |
| EEPROM corrompida | Byte de modo com validação (0-2); default 0 se inválido |
