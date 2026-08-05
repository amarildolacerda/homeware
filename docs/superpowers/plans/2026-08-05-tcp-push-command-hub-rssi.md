# TCP Push Command + Hub RSSI no Sidebar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fazer o hub enviar comandos de estado direto ao node TCP (lamp) via `POST /api/relay` no IP conhecido, com fallback para o polling existente, e exibir o RSSI do hub no header do sidebar do dashboard.

**Architecture:** (1) `TcpRadioHandler::send_command` continua enfileirando o `PendingCommand` (polling) e, se o IP do node é conhecido, enfileira também um `PushCommand`; `loop()` processa a fila de push de forma não-bloqueante (1 tentativa/ciclo, retry 1s, TTL 30s), removendo o fallback de polling quando o push retorna HTTP 200. (2) `/api/info` passa a expor `wifi_rssi`; o dashboard mostra um indicador de 4 barras no header do sidebar.

**Tech Stack:** C++ (Arduino/PlatformIO), ESP32/ESP8266, ArduinoJson v7, HTTPClient, HTML/CSS/JS inline (PROGMEM).

## Global Constraints

- Trabalhar no branch `dev` (verificar com `git branch --show-current`). Commits locais; **não** dar push para `origin/dev` sem confirmação do usuário.
- `loop()` não pode conter `delay()` bloqueante (regra 15). Push processado em `loop()`, máx. 1 tentativa HTTP por ciclo, retry rate-limited.
- Não tocar no submodule `shared/` (defines `TCP_HTTP_PORT=80`, `TCP_COMMAND_TTL_MS=30000` já existem lá). Definir `TCP_PUSH_RETRY_INTERVAL_MS` localmente no `.cpp`.
- Páginas web grandes via PROGMEM; manter o HTML enxuto.
- `POST /api/relay` no lamp aceita `{"state":true|false}` (JSON booleano) — confirmado em `nodes/lamp/src/main.cpp:handle_api_relay`.
- Idempotência: `set_relay(bool)` no lamp seta (não alterna) — dupla entrega é inofensiva.
- Build de verificação: `pio run -e hub_32` (ESP32, `TCP_ENABLED`) e `pio run -e hub_8266` (ESP8266, `TCP_ENABLED`).

---

### Task 1: Expor `wifi_rssi` no `/api/info`

**Files:**
- Modify: `hub/src/web_server.cpp` (handler `/api/info`, após linha 244 `doc["wifi_ssid"] = WiFi.SSID();`)

**Interfaces:**
- Produces: campo JSON `wifi_rssi` (int) no `GET /api/info`. Consumido pela Task 3.

- [ ] **Step 1: Adicionar o campo**

No handler `/api/info`, logo após a linha `doc["wifi_ssid"] = WiFi.SSID();`, adicionar:

```cpp
        doc["wifi_rssi"] = WiFi.RSSI();
```

- [ ] **Step 2: Build de verificação**

Run: `pio run -e hub_32`
Expected: compila sem erros (o handler `/api/info` já usa `WiFi.*`).

- [ ] **Step 3: Commit**

```bash
git add hub/src/web_server.cpp
git commit -m "feat(hub): expor wifi_rssi no /api/info"
```

---

### Task 2: Push de comando TCP com fallback para polling

**Files:**
- Modify: `hub/src/tcp_radio_handler.h:15-19` (struct `PendingCommand`), `:63` (membro `m_pending_commands`), `:65-67` (métodos privados)
- Modify: `hub/src/tcp_radio_handler.cpp:9` (includes), `:177-180` (`loop()`), `:213-230` (`send_command`), adicionar `process_push_queue()` e `remove_pending_command()` após `cleanup_expired_commands()` (`:437`)

**Interfaces:**
- Consumes: `sensor_registry_find_by_mac`, `sensor_registry_get`, `virtual_sensor_t` (`ip[4]`, `bridge_device_id`, `paired`), `TCP_COMMAND_TTL_MS`, `TCP_HTTP_PORT`, `log_add`, `console.printf`.
- Produces: `TcpRadioHandler::process_push_queue()` (chamado no `loop()`), `TcpRadioHandler::remove_pending_command(const std::string&)`. Sem mudança de assinatura pública de `send_command`.

- [ ] **Step 1: Adicionar struct e membros no header**

Em `hub/src/tcp_radio_handler.h`, após o struct `PendingCommand` (linha 19), adicionar:

```cpp
struct PushCommand {
    String device_id;
    uint8_t ip[4];
    uint8_t state;
    unsigned long created_at;
    unsigned long last_attempt_ms;  // 0 = nunca tentado
};
```

Após o membro `std::map<std::string, std::vector<PendingCommand>> m_pending_commands;` (linha 63), adicionar:

```cpp
    std::vector<PushCommand> m_push_queue;
```

Após a declaração `void cleanup_expired_commands();` (linha 66), adicionar:

```cpp
    void process_push_queue();
    void remove_pending_command(const std::string& device_id);
```

- [ ] **Step 2: Adicionar includes no `.cpp`**

Em `hub/src/tcp_radio_handler.cpp`, após `#include "platform.h"` (linha 9), adicionar:

```cpp
#include <HTTPClient.h>
```

(`WiFi` já está disponível transitivamente — o arquivo já usa `WiFi.localIP()`.)

- [ ] **Step 3: Definir intervalo de retry local**

Em `hub/src/tcp_radio_handler.cpp`, logo após os includes (antes de `extern MyWebServer s_server;`), adicionar:

```cpp
#define TCP_PUSH_RETRY_INTERVAL_MS 1000
```

- [ ] **Step 4: Chamar `process_push_queue()` no `loop()`**

Substituir o corpo de `TcpRadioHandler::loop()` (linhas 177-181):

```cpp
void TcpRadioHandler::loop() {
    handle_udp_discover();
    cleanup_expired_commands();
    process_push_queue();
}
```

- [ ] **Step 5: Enfileirar push no `send_command`**

Substituir o corpo de `TcpRadioHandler::send_command` (linhas 213-230) por:

```cpp
bool TcpRadioHandler::send_command(const uint8_t* mac, uint8_t state) {
    int slot = sensor_registry_find_by_mac(mac);
    if (slot < 0) return false;

    virtual_sensor_t* sensor = sensor_registry_get(slot);
    if (!sensor || !sensor->paired) return false;

    PendingCommand cmd;
    cmd.command = (state == 1) ? "on" : "off";
    cmd.slot = slot;
    cmd.created_at = millis();

    m_pending_commands[sensor->bridge_device_id].push_back(cmd);

    // Push direto se o IP do node é conhecido (fallback: polling acima)
    if (sensor->ip[0] || sensor->ip[1] || sensor->ip[2] || sensor->ip[3]) {
        PushCommand pc;
        pc.device_id = sensor->bridge_device_id;
        memcpy(pc.ip, sensor->ip, 4);
        pc.state = state;
        pc.created_at = millis();
        pc.last_attempt_ms = 0;
        m_push_queue.push_back(pc);
        log_add("info", "[tcp] Push queued for %s: %s @ %d.%d.%d.%d", sensor->bridge_device_id,
                cmd.command.c_str(), pc.ip[0], pc.ip[1], pc.ip[2], pc.ip[3]);
    }

    console.printf("[tcp] Command queued for %s: %s\n", sensor->bridge_device_id, cmd.command.c_str());
    log_add("info", "[tcp] Command queued for %s: %s (radio_type=%d)", sensor->bridge_device_id, cmd.command.c_str(), sensor->radio_type);
    return true;
}
```

- [ ] **Step 6: Implementar `process_push_queue()` e `remove_pending_command()`**

Após o fim de `cleanup_expired_commands()` (linha 437), adicionar:

```cpp
void TcpRadioHandler::process_push_queue() {
    unsigned long now = millis();

    // Remove expirados
    m_push_queue.erase(
        std::remove_if(m_push_queue.begin(), m_push_queue.end(),
            [now](const PushCommand& p) { return (now - p.created_at) > TCP_COMMAND_TTL_MS; }),
        m_push_queue.end());

    for (auto it = m_push_queue.begin(); it != m_push_queue.end(); ++it) {
        if (it->last_attempt_ms != 0 && (now - it->last_attempt_ms) < TCP_PUSH_RETRY_INTERVAL_MS)
            continue;  // ainda não é hora de tentar de novo

        it->last_attempt_ms = now;

        char url[64];
        snprintf(url, sizeof(url), "http://%d.%d.%d.%d:%d/api/relay",
                 it->ip[0], it->ip[1], it->ip[2], it->ip[3], TCP_HTTP_PORT);

        WiFiClient client;
        HTTPClient http;
        if (!http.begin(client, url)) {
            break;
        }
#if defined(ARDUINO_ARCH_ESP32)
        http.setConnectTimeout(500);  // limita bloqueio do loop no connect (ESP32)
#endif
        http.setTimeout(800);
        String body = String("{\"state\":") + (it->state ? "true" : "false") + "}";
        int code = http.POST(body);
        http.end();

        if (code == 200) {
            log_add("info", "[tcp] Push OK: %s (HTTP %d)", it->device_id.c_str(), code);
            remove_pending_command(it->device_id.c_str());
            m_push_queue.erase(it);
        } else {
            log_add("warn", "[tcp] Push retry: %s -> HTTP %d (polling fallback active)", it->device_id.c_str(), code);
        }
        break;  // máx. 1 tentativa HTTP por ciclo de loop
    }
}

void TcpRadioHandler::remove_pending_command(const std::string& device_id) {
    auto it = m_pending_commands.find(device_id);
    if (it != m_pending_commands.end() && !it->second.empty()) {
        it->second.erase(it->second.begin());
        log_add("info", "[tcp] Polling fallback removed for %s (push OK)", device_id.c_str());
        if (it->second.empty()) {
            m_pending_commands.erase(it);
        }
    }
}
```

- [ ] **Step 7: Build de verificação (ESP32 e ESP8266)**

Run: `pio run -e hub_32 && pio run -e hub_8266`
Expected: ambos compilam sem erros (o código TCP compila nas duas plataformas via `TCP_ENABLED`).

- [ ] **Step 8: Commit**

```bash
git add hub/src/tcp_radio_handler.h hub/src/tcp_radio_handler.cpp
git commit -m "feat(hub): push TCP direto ao node via POST /api/relay com fallback p/ polling"
```

---

### Task 3: Indicador de RSSI do hub no header do sidebar

**Files:**
- Modify: `hub/include/pages.h:113-115` (CSS `.sidebar .logo`), `:145` (media query), `:155-166` (HTML do logo), e JS (função `updateHubSignal` + chamadas em `loadData` e `loadSettings`)

**Interfaces:**
- Consumes: campo `wifi_rssi` do `GET /api/info` (Task 1).
- Produces: elemento `#hubSignal` com 4 `.bar`; função `updateHubSignal(rssi)`.

- [ ] **Step 1: Adicionar CSS do indicador**

Em `hub/include/pages.h`, após a regra `.sidebar .logo span{...}` (linha 115), adicionar:

```css
.sidebar .logo .logo-row{display:flex;align-items:center;justify-content:space-between;gap:8px;width:100%}
.signal{display:flex;align-items:flex-end;gap:2px;height:14px}
.signal .bar{width:3px;border-radius:1px;background:var(--border-strong)}
.signal .bar:nth-child(1){height:4px}
.signal .bar:nth-child(2){height:7px}
.signal .bar:nth-child(3){height:10px}
.signal .bar:nth-child(4){height:13px}
.signal .bar.on{background:var(--success)}
```

- [ ] **Step 2: Esconder o sinal no modo colapsado**

Na media query `@media(max-width:700px)` (linha 145), adicionar `.sidebar .logo .signal` à lista de `display:none`:

```css
.sidebar .logo h1,.sidebar .logo span,.sidebar .logo .signal,.sidebar nav a span:last-child,.sidebar .footer-nav,.sidebar .nav-group-head .chev,.sidebar .nav-group-head span:not(.icon){display:none}
```

- [ ] **Step 3: Alterar o HTML do logo**

Substituir o bloco do logo (linhas 155-166) por:

```html
<div class="sidebar">
<div class="logo">
<div class="logo-row">
<div class="logo-title">
)rawliteral"
#if defined(LORA_ENABLED) && defined(ESPNOW_ENABLED)
"<h1>LoRa + ESP-NOW</h1><span>Hub</span>"
#elif defined(LORA_ENABLED)
"<h1>LoRa</h1><span>Hub</span>"
#else
"<h1>ESP-NOW</h1><span>Gateway</span>"
#endif
R"rawliteral(
</div>
<div class="signal" id="hubSignal" title="RSSI: --"><span class="bar"></span><span class="bar"></span><span class="bar"></span><span class="bar"></span></div>
</div>
</div>
```

- [ ] **Step 4: Adicionar a função JS `updateHubSignal`**

Imediatamente antes de `async function loadData() {` (linha 905), adicionar:

```js
function updateHubSignal(rssi){
  var el=document.getElementById('hubSignal');
  if(!el) return;
  var bars=el.querySelectorAll('.bar');
  var level=0;
  if(rssi>=-40) level=4;
  else if(rssi>=-60) level=3;
  else if(rssi>=-70) level=2;
  else if(rssi>=-80) level=1;
  for(var i=0;i<bars.length;i++) bars[i].classList.toggle('on', i<level);
  el.title='RSSI: '+rssi+' dBm';
}
```

- [ ] **Step 5: Chamar em `loadData`**

Após a linha `if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;` (linha 915), adicionar:

```js
    if (info.wifi_rssi!==undefined) updateHubSignal(info.wifi_rssi);
```

- [ ] **Step 6: Chamar em `loadSettings`**

Após a linha `if (info.fw_version) document.getElementById('fw-sidebar').textContent = info.fw_version;` (linha 1126), adicionar:

```js
    if (info.wifi_rssi!==undefined) updateHubSignal(info.wifi_rssi);
```

- [ ] **Step 7: Build de verificação**

Run: `pio run -e hub_32`
Expected: compila sem erros (página PROGMEM).

- [ ] **Step 8: Commit**

```bash
git add hub/include/pages.h
git commit -m "feat(hub): indicador de RSSI do hub no header do sidebar"
```

---

## Verificação final (bancada)

- Build: `pio run -e hub_32 && pio run -e hub_8266` — ambos OK.
- Dashboard: abrir `http://<hub_ip>/` → conferir barras de sinal no header do sidebar refletindo `wifi_rssi` do `/api/info` (tooltip `RSSI: X dBm`).
- Push: lamp TCP registrado com IP → comando ON/OFF pelo dashboard/console do hub → observar `POST /api/relay` no log do lamp (atraso < polling interval) e estado atualizado.
- Fallback: desligar o lamp → comando → verificar que o comando fica na fila de polling e é entregue quando o lamp volta (log `Push retry` + `Polling fallback removed` quando o push voltar a funcionar).