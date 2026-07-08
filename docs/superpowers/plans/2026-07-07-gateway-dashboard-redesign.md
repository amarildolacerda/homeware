# Gateway Dashboard Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the gateway web dashboard with light theme, sidebar navigation, sensor icons/indicators, log buffer, and responsive layout.

**Architecture:** SPA leve com shell PROGMEM + 3 páginas (Overview, Settings, Logs) carregadas via `fetch()` no container. Buffer circular de 50 logs servido via `/api/logs`.

**Tech Stack:** ESP8266, ESP8266WebServer, PROGMEM, JavaScript vanilla, CSS3

**Design spec:** `docs/superpowers/specs/2026-07-07-gateway-dashboard-redesign.md`

**Style Guide:** `.opencode/skills/style-guide/SKILL.md` (tema claro)

---

### Task 1: Log buffer

**Files:**
- Create: `gateway/include/log_buffer.h`
- Create: `gateway/src/log_buffer.cpp`
- Modify: `gateway/include/config.h:1-29` — add `LOG_BUFFER_SIZE 50`

**Interfaces:**
- Produces: `void log_add(const char *level, const char *fmt, ...)`, `const char* log_get_json()`

- [ ] **Create `config.h` — add LOG_BUFFER_SIZE**

Add after `#define STATUS_LED_GPIO 2`:
```cpp
#define LOG_BUFFER_SIZE 50
#define LOG_MSG_MAX 64
```

- [ ] **Create `include/log_buffer.h`**

```cpp
#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <Arduino.h>

void log_buffer_init();
void log_add(const char *level, const char *fmt, ...);
const char* log_get_json();

#endif
```

- [ ] **Create `src/log_buffer.cpp`**

```cpp
#include "log_buffer.h"
#include "config.h"
#include <stdarg.h>

typedef struct {
    unsigned long time;
    char msg[LOG_MSG_MAX];
    char level[8];
} log_entry_t;

static log_entry_t s_buffer[LOG_BUFFER_SIZE];
static int s_head = 0;
static int s_count = 0;
static char s_json[4096];

void log_buffer_init() {
    s_head = 0;
    s_count = 0;
    log_add("info", "Gateway iniciado v" FW_VERSION);
}

void log_add(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_buffer[s_head].msg, LOG_MSG_MAX, fmt, args);
    va_end(args);
    s_buffer[s_head].time = millis();
    strncpy(s_buffer[s_head].level, level, sizeof(s_buffer[s_head].level) - 1);
    s_buffer[s_head].level[sizeof(s_buffer[s_head].level) - 1] = '\0';
    s_head = (s_head + 1) % LOG_BUFFER_SIZE;
    if (s_count < LOG_BUFFER_SIZE) s_count++;
}

const char* log_get_json() {
    int pos = 0;
    pos += snprintf(s_json + pos, sizeof(s_json) - pos, "{\"logs\":[");
    int start = (s_count < LOG_BUFFER_SIZE) ? 0 : s_head;
    int count = s_count;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        if (pos > 9) {
            s_json[pos++] = ',';
            if (pos >= (int)sizeof(s_json)) break;
        }
        pos += snprintf(s_json + pos, sizeof(s_json) - pos,
            "{\"t\":%lu,\"msg\":\"%s\",\"level\":\"%s\"}",
            s_buffer[idx].time, s_buffer[idx].msg, s_buffer[idx].level);
        if (pos >= (int)sizeof(s_json)) break;
    }
    pos += snprintf(s_json + pos, sizeof(s_json) - pos, "]}");
    s_json[pos] = '\0';
    return s_json;
}
```

- [ ] **Add `log_add()` calls in `src/main.cpp`**

In `setup()`, after `espnow_handler_init()`:
```cpp
log_buffer_init();
```

At boot end, change restart message:
```cpp
log_add("warn", "Reiniciando...");
```

- [ ] **Add `log_add()` calls in `src/web_server.cpp`**

In `/api/restart` handler, before restart:
```cpp
log_add("warn", "Reiniciando via web");
```

In `/api/pair/start`:
```cpp
log_add("info", "Pareamento iniciado");
```

In `/api/pair/stop`:
```cpp
log_add("info", "Pareamento finalizado");
```

In rename sensor handler:
```cpp
log_add("info", "Sensor slot %d renomeado para \"%s\"", slot, name);
```

In remove sensor handler:
```cpp
log_add("warn", "Sensor slot %d removido", slot);
```

In `/api/clear`:
```cpp
log_add("warn", "Todos os sensores removidos");
```

In sensor command handler:
```cpp
log_add("info", "Comando %s enviado para slot %d", state ? "ON" : "OFF", slot);
```

- [ ] **Add `log_add()` calls in `src/mqtt_client.cpp`**

Connect success:
```cpp
log_add("info", "MQTT conectado a %s:%d", host, port);
```

Connect failure:
```cpp
log_add("error", "MQTT falhou: %s", error_str);
```

Disconnect:
```cpp
log_add("warn", "MQTT desconectado");
```

- [ ] **Add `log_add()` in `src/espnow_handler.cpp`**

Sensor paired:
```cpp
log_add("info", "Sensor %s pareado slot %d", mac_str, slot);
```

Sensor data received:
```cpp
log_add("info", "Dados recebidos slot %d seq %d", slot, sequence);
```

Sensor offline/timeout:
```cpp
log_add("warn", "Sensor slot %d offline", slot);
```

- [ ] **Include log_buffer.h in affected files**

Add `#include "log_buffer.h"` in: `main.cpp`, `web_server.cpp`, `mqtt_client.cpp`, `espnow_handler.cpp`

---

### Task 2: Pages (pages.h)

**Files:**
- Modify: `gateway/include/pages.h` — replace `PAGE_DASHBOARD` with `PAGE_SHELL`, `PAGE_OVERVIEW`, `PAGE_SETTINGS`, `PAGE_LOGS`

**Interfaces:**
- Consumes: style guide CSS variables (light theme)
- Produces: `PAGE_SHELL`, `PAGE_OVERVIEW`, `PAGE_SETTINGS`, `PAGE_LOGS` PROGMEM strings

- [ ] **Write `PAGE_SHELL`** (~2KB)

HTML structure:
```html
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>ESP-NOW Gateway</title>
<style>
:root {
  --bg:#f4f4f4; --surface:#fff; --surface-2:#f9fafb;
  --text:#1f2937; --muted:#6b7280; --muted-subtle:#9ca3af;
  --primary:#5e6ad2; --primary-strong:#828fff; --primary-focus:#eef0ff;
  --border:#e5e7eb; --border-strong:#d1d5db;
  --success:#16a34a; --danger:#dc2626; --warn:#f59e0b; --info:#2563eb;
}
* { box-sizing:border-box; margin:0; padding:0; }
body { font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif; background:var(--bg); color:var(--text); min-height:100vh; display:flex; }
.sidebar { width:200px; background:var(--surface); border-right:1px solid var(--border); padding:24px 0; display:flex; flex-direction:column; position:fixed; top:0; left:0; height:100vh; z-index:10; }
.sidebar .logo { padding:0 20px 20px; border-bottom:1px solid var(--border); margin-bottom:8px; }
.sidebar .logo h1 { font-size:1rem; font-weight:700; color:var(--primary); }
.sidebar .logo span { font-size:0.75rem; color:var(--muted); }
.sidebar nav { flex:1; }
.sidebar nav a { display:flex; align-items:center; gap:10px; padding:12px 20px; color:var(--muted); text-decoration:none; font-size:0.85rem; font-weight:500; transition:all .15s; border-left:3px solid transparent; }
.sidebar nav a:hover { color:var(--text); background:var(--primary-focus); }
.sidebar nav a.active { color:var(--primary); background:var(--primary-focus); border-left-color:var(--primary); font-weight:600; }
.sidebar nav a .icon { width:20px; text-align:center; font-size:1.1rem; }
.sidebar .footer-nav { padding:12px 20px; border-top:1px solid var(--border); font-size:0.7rem; color:var(--muted-subtle); }
.main { margin-left:200px; flex:1; padding:24px; max-width:960px; }
#page { min-height: calc(100vh - 80px); }
.mqtt-footer { position:fixed; bottom:0; right:0; left:200px; background:var(--surface); border-top:1px solid var(--border); padding:8px 24px; display:flex; align-items:center; gap:8px; font-size:0.78rem; z-index:10; }
.mqtt-footer .dot { width:8px; height:8px; border-radius:50%; display:inline-block; }
.mqtt-footer .dot.on { background:var(--success); }
.mqtt-footer .dot.off { background:var(--danger); }
@media(max-width:700px) {
  .sidebar { width:60px; }
  .sidebar .logo h1, .sidebar .logo span, .sidebar nav a span:last-child, .sidebar .footer-nav { display:none; }
  .sidebar nav a { justify-content:center; padding:14px; border-left:none; }
  .sidebar nav a.active { border-left:none; background:var(--primary-focus); }
  .main { margin-left:60px; padding:16px; }
  .mqtt-footer { left:60px; }
}
</style>
</head>
<body>
<div class="sidebar">
  <div class="logo"><h1>ESP-NOW</h1><span>Gateway</span></div>
  <nav>
    <a href="#" onclick="navigate('overview');return false" class="active" id="nav-overview"><span class="icon">&#x1F3E0;</span><span>Visão Geral</span></a>
    <a href="#" onclick="navigate('settings');return false" id="nav-settings"><span class="icon">&#x2699;</span><span>Configurações</span></a>
    <a href="#" onclick="navigate('logs');return false" id="nav-logs"><span class="icon">&#x1F4CB;</span><span>Logs</span></a>
  </nav>
  <div class="footer-nav" id="fw-sidebar">v0.0.20</div>
</div>
<div class="main"><div id="page"><div class="loading" style="text-align:center;padding:60px 20px;color:var(--muted)">carregando...</div></div></div>
<div class="mqtt-footer" id="mqtt-footer"><span class="dot off" id="mqtt-dot"></span><span id="mqtt-footer-text">MQTT: desconectado</span></div>
<script>
function navigate(page) {
  document.querySelectorAll('.sidebar nav a').forEach(a => a.classList.remove('active'));
  const link = document.getElementById('nav-'+page);
  if (link) link.classList.add('active');
  const el = document.getElementById('page');
  el.innerHTML = '<div class="loading" style="text-align:center;padding:60px 20px;color:var(--muted)">carregando...</div>';
  fetch('/'+page).then(r=>r.text()).then(html=>{ el.innerHTML = html; }).catch(()=>{
    el.innerHTML = '<div style="text-align:center;padding:60px 20px;color:var(--danger)">Erro ao carregar</div>';
  });
}
let s_mqtt_connected = false;
function updateMqttFooter(connected, host, port) {
  const dot = document.getElementById('mqtt-dot');
  const txt = document.getElementById('mqtt-footer-text');
  dot.className = 'dot ' + (connected ? 'on' : 'off');
  txt.textContent = connected ? 'MQTT: Conectado ao ' + (host||'broker') : 'MQTT: desconectado';
  s_mqtt_connected = connected;
}
function showToast(msg, err) {
  const t = document.getElementById('toast');
  if (!t) return;
  t.textContent = msg;
  t.style.background = err ? '#dc2626' : '#16a34a';
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 4000);
}
navigate('overview');
</script>
```

- [ ] **Write `PAGE_OVERVIEW`** (~5KB)

Structure:
- Stats row: 4 metrics in cards (paired, online, RX, uptime)
- Filter buttons: Todos, Temp, Contato, Interruptor, Gás, Chuva, Tanque
- Sensor grid with icons per type, battery/RSSI bars, expand/collapse details
- Toast element for notifications

Key CSS additions (inline in the page or shared in shell):
```css
.stats { display:grid; grid-template-columns:repeat(4,1fr); gap:12px; margin-bottom:20px; }
.stat { background:var(--surface); border:1px solid var(--border); border-radius:10px; padding:16px; text-align:center; }
.stat-value { font-size:1.4rem; font-weight:700; color:var(--primary); }
.stat-label { font-size:0.7rem; color:var(--muted-subtle); text-transform:uppercase; letter-spacing:.05em; margin-top:4px; }
.filters { display:flex; gap:6px; flex-wrap:wrap; margin-bottom:16px; }
.filter-btn { padding:6px 14px; border:1px solid var(--border); border-radius:9999px; background:var(--surface); color:var(--muted); font-size:0.78rem; cursor:pointer; transition:all .15s; }
.filter-btn.active { background:var(--primary); color:#fff; border-color:var(--primary); }
.grid { display:grid; gap:12px; }
.grid-2 { grid-template-columns:repeat(auto-fill,minmax(280px,1fr)); }
.device { background:var(--surface); border:1px solid var(--border); border-radius:10px; padding:14px; transition:border-color .15s; }
.device:hover { border-color:var(--primary-strong); }
.device.offline { border-color:var(--danger); opacity:.7; }
.device-head { display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:8px; }
.device-icon { font-size:1.5rem; width:36px; text-align:center; }
.device-name { font-weight:600; font-size:0.9rem; }
.device-type { font-size:0.7rem; color:var(--muted-subtle); }
.badge { display:inline-block; padding:2px 8px; border-radius:9999px; font-size:0.7rem; font-weight:600; }
.badge.online { background:#dcfce7; color:#16a34a; }
.badge.offline { background:#fef2f2; color:#dc2626; }
.badge.warn { background:#fef3c7; color:#d97706; }
.metrics { display:grid; grid-template-columns:1fr 1fr; gap:8px; margin:10px 0; }
.metric-bar { display:flex; align-items:center; gap:6px; font-size:0.78rem; }
.metric-bar .bar { flex:1; height:6px; background:var(--border); border-radius:3px; overflow:hidden; }
.metric-bar .bar .fill { height:100%; border-radius:3px; transition:width .3s; }
.metric-bar .bar .fill.green { background:var(--success); }
.metric-bar .bar .fill.yellow { background:var(--warn); }
.metric-bar .bar .fill.red { background:var(--danger); }
.details { overflow:hidden; max-height:0; transition:max-height .3s; }
.details.open { max-height:500px; }
.details-inner { display:grid; grid-template-columns:1fr 1fr; gap:6px; font-size:0.78rem; padding-top:8px; border-top:1px solid var(--border); margin-top:8px; }
.details-inner .label { color:var(--muted-subtle); }
.details-inner .value { font-weight:500; }
.device-actions { display:flex; gap:6px; margin-top:8px; flex-wrap:wrap; }
.btn-sm { padding:6px 12px; font-size:0.75rem; border-radius:6px; border:1px solid var(--border); background:var(--surface); color:var(--text); cursor:pointer; }
.btn-sm:hover { background:var(--surface-2); }
.btn-sm.danger { color:var(--danger); border-color:var(--danger); }
.btn-sm.danger:hover { background:#fef2f2; }
.collapse-btn { background:none; border:none; color:var(--muted); cursor:pointer; font-size:0.75rem; padding:4px 0; }
.collapse-btn:hover { color:var(--primary); }
.toast { position:fixed; bottom:60px; right:20px; padding:10px 16px; border-radius:8px; background:#1f2937; color:#fff; z-index:100; display:none; font-size:0.85rem; }
.toast.show { display:block; animation:slideIn .3s; }
@keyframes slideIn { from{opacity:0;transform:translateY(10px)} to{opacity:1;transform:translateY(0)} }
@media(max-width:600px){ .stats { grid-template-columns:1fr 1fr; } }
```

JavaScript for Overview:
- `loadData()` — fetch `/api/info` + `/api/sensors`, render stats + grid
- `renderSensors(sensors)` — build HTML per sensor with icons, bars, details
- `filterSensors(type)` — show/hide by type
- `toggleDetails(slot)` — expand/collapse
- `renameSensor(slot)`, `removeSensor(slot)`, `toggleSensor(slot, state)` — API calls
- `renderState(s)` — state values per type
- Auto-refresh every 5s
- Update MQTT footer via `parent.updateMqttFooter()`

Icon mapping for sensor types:
```
type 1 → &#x1F321;  type 2 → &#x1F514;  type 3 → &#x1F3C3;
type 4 → &#x1F4A8;  type 5 → &#x2614;   type 6 → &#x1F4A7;
type 7 → &#x1F321;+&#x1F4A8;  type 8 → &#x1F50C;
```

- [ ] **Write `PAGE_SETTINGS`** (~2KB)

MQTT form card + Bridge info card + actions card. Uses same CSS variables from shell.

```html
<div style="max-width:500px">
  <div class="card">
    <h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">MQTT</h2>
    <div class="row"><span class="label">Host</span><span class="value" id="s-host">--</span></div>
    <div class="row"><span class="label">Porta</span><span class="value" id="s-port">--</span></div>
    <div class="row"><span class="label">Usuário</span><span class="value" id="s-user">--</span></div>
    <div class="row"><span class="label">Status</span><span class="value" id="s-status">--</span></div>
    <button class="btn btn-primary" onclick="showMqttForm()" style="margin-top:12px">Configurar MQTT</button>
  </div>
  <div class="card" style="margin-top:12px">
    <h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">Bridge</h2>
    <div class="row"><span class="label">Device ID</span><span class="value" id="s-device">--</span></div>
    <div class="row"><span class="label">Firmware</span><span class="value" id="s-fw">--</span></div>
  </div>
  <div style="display:flex;gap:8px;margin-top:16px;flex-wrap:wrap">
    <button class="btn btn-primary" onclick="doRestart()">Reiniciar</button>
    <button class="btn btn-secondary" onclick="doReregister()">Re-registrar</button>
  </div>
</div>
<div class="modal" id="mqtt-modal">...</div>
<script>
// load settings, form handling, restart, reregister
</script>
```

CSS for Settings:
```css
.card { background:var(--surface); border:1px solid var(--border); border-radius:10px; padding:16px; }
.row { display:flex; justify-content:space-between; align-items:center; padding:8px 0; border-bottom:1px solid var(--border); font-size:0.85rem; }
.row:last-child { border-bottom:none; }
.label { color:var(--muted-subtle); }
.value { font-weight:600; }
.btn { padding:10px 18px; border:none; border-radius:8px; font-weight:600; cursor:pointer; font-size:0.85rem; min-height:44px; }
.btn-primary { background:var(--primary); color:#fff; }
.btn-primary:hover { filter:brightness(1.1); }
.btn-secondary { background:var(--border); color:var(--text); }
.btn-secondary:hover { background:var(--border-strong); }
.modal { position:fixed; inset:0; background:rgba(0,0,0,.5); display:none; align-items:center; justify-content:center; z-index:100; }
.modal.show { display:flex; }
.modal-content { background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:24px; width:100%; max-width:400px; }
.form-group { margin-bottom:14px; }
.form-group label { display:block; margin-bottom:4px; font-size:0.82rem; color:var(--muted); }
.form-group input { width:100%; padding:10px 12px; border:1px solid var(--border); border-radius:8px; font-size:16px; background:var(--surface-2); color:var(--text); }
```

- [ ] **Write `PAGE_LOGS`** (~2KB)

```html
<div>
  <h2 style="font-size:0.95rem;font-weight:600;margin-bottom:16px;color:var(--primary)">Logs</h2>
  <div id="log-table">
    <div class="loading" style="text-align:center;padding:40px;color:var(--muted)">carregando...</div>
  </div>
</div>
<script>
async function loadLogs() {
  try {
    const d = await fetch('/api/logs').then(r=>r.json());
    const table = document.getElementById('log-table');
    if (!d.logs || !d.logs.length) {
      table.innerHTML = '<div style="text-align:center;padding:40px;color:var(--muted-subtle)">Nenhum evento registrado</div>';
      return;
    }
    table.innerHTML = '<div style="display:flex;flex-direction:column;gap:4px">' + d.logs.map(l => {
      const s = Math.floor(l.t/1000);
      const m = Math.floor(s/60)%60; const h = Math.floor(s/3600)%24; const sec = s%60;
      const ts = (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(sec<10?'0':'')+sec;
      const color = l.level === 'error' ? 'var(--danger)' : l.level === 'warn' ? 'var(--warn)' : 'var(--muted)';
      return '<div style="display:flex;gap:8px;padding:6px 10px;border-radius:6px;font-size:0.8rem;border-left:3px solid '+color+'"><span style="color:var(--muted-subtle);min-width:60px">'+ts+'</span><span>'+l.msg+'</span></div>';
    }).join('') + '</div>';
  } catch(e) {
    document.getElementById('log-table').innerHTML = '<div style="text-align:center;padding:40px;color:var(--danger)">Erro ao carregar logs</div>';
  }
}
loadLogs();
setInterval(loadLogs, 10000);
</script>
```

---

### Task 3: Server routes (web_server.cpp)

**Files:**
- Modify: `gateway/src/web_server.cpp` — add new page routes + `/api/logs`

**Interfaces:**
- Consumes: `PAGE_SHELL`, `PAGE_OVERVIEW`, `PAGE_SETTINGS`, `PAGE_LOGS`, `log_get_json()`

- [ ] **Add helper function** for serving PROGMEM pages (DRY)

```cpp
static void serve_pgm_page(const char* page) {
    size_t total = strlen_P(page);
    WiFiClient cl = s_server.client();
    cl.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "));
    cl.print(total);
    cl.print(F("\r\nConnection: close\r\n\r\n"));
    PGM_P src = page;
    char buf[256];
    while (total > 0) {
        size_t chunk = total > sizeof(buf) ? sizeof(buf) : total;
        memcpy_P(buf, src, chunk);
        cl.write((const uint8_t*)buf, chunk);
        src += chunk;
        total -= chunk;
        yield();
    }
}
```

- [ ] **Replace `/` route** to serve `PAGE_SHELL`

```cpp
s_server.on("/", HTTP_GET, []() {
    serve_pgm_page(PAGE_SHELL);
});
```

- [ ] **Add `/overview` route**

```cpp
s_server.on("/overview", HTTP_GET, []() {
    serve_pgm_page(PAGE_OVERVIEW);
});
```

- [ ] **Add `/settings` route**

```cpp
s_server.on("/settings", HTTP_GET, []() {
    serve_pgm_page(PAGE_SETTINGS);
});
```

- [ ] **Add `/logs` route**

```cpp
s_server.on("/logs", HTTP_GET, []() {
    serve_pgm_page(PAGE_LOGS);
});
```

- [ ] **Add `/api/logs` route**

```cpp
s_server.on("/api/logs", HTTP_GET, []() {
    s_server.send(200, "application/json", log_get_json());
});
```

- [ ] **Keep existing routes**: `/docs`, `/api/info`, `/api/sensors`, `/api/pair/*`, `/api/broadcast`, `/api/sensor/*`, `/api/config/mqtt`, `/api/restart`, `/api/ota`, `/api/clear`

All remain unchanged except `/` which now serves `PAGE_SHELL` instead of `PAGE_DASHBOARD`.

- [ ] **Update `web_server.h`** if `log_get_json()` needs extern (no — it's called internally)

---

### Task 4: Build and verify

- [ ] **Build**: `pio run`

- [ ] **Check PROGMEM usage**: verify no page exceeds ~8KB (to avoid heap issues with `send_P`)

- [ ] **Flash and test**: navigate all 3 pages, verify data loads, logs display, settings save
