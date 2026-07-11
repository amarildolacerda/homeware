# Lampada Repeater Statistics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add repeater statistics to the lampada client showing which terminals used it as a repeater and how many packets were retransmitted.

**Architecture:** Add tracking structures and logic to the existing repeater code path in `espnow_recv_cb`, extend `/api/state` with repeater fields, add sidebar navigation item and section in dashboard.

**Tech Stack:** Arduino (ESP8266), ArduinoJson, ESP8266WebServer, PROGMEM HTML

## Global Constraints

- ESP8266 memory limited - keep additions minimal
- Follow existing code style (static variables, C-style arrays)
- Use `FPSTR()` for PROGMEM strings
- Dashboard must be responsive and compact
- Max 5 repeater clients tracked (50 bytes RAM)

---

### Task 1: Add repeater tracking data structures

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp:48-66`

**Interfaces:**
- Produces: `s_rep_clients[]`, `s_rep_client_num`, `s_repeater_fwd`, `track_repeater_client()`

- [ ] **Step 1: Add repeater statistics variables after existing counters**

After line 50 (`static uint32_t s_on_count = 0;`), add:

```c
#define MAX_REPEATER_CLIENTS 5

typedef struct {
    uint8_t mac[6];
    uint32_t pkt_count;
} repeater_client_t;

static repeater_client_t s_rep_clients[MAX_REPEATER_CLIENTS];
static int s_rep_client_num = 0;
static uint32_t s_repeater_fwd = 0;
```

- [ ] **Step 2: Add track_repeater_client function**

After the `s_broadcast_mac` definition (line 72), add:

```c
static void track_repeater_client(const uint8_t *mac)
{
    for (int i = 0; i < s_rep_client_num; i++)
    {
        if (memcmp(s_rep_clients[i].mac, mac, 6) == 0)
        {
            s_rep_clients[i].pkt_count++;
            return;
        }
    }
    if (s_rep_client_num < MAX_REPEATER_CLIENTS)
    {
        memcpy(s_rep_clients[s_rep_client_num].mac, mac, 6);
        s_rep_clients[s_rep_client_num].pkt_count = 1;
        s_rep_client_num++;
    }
}
```

- [ ] **Step 3: Verify compilation**

Run: `cd /mnt/c/project/homeware && python -c "import subprocess; subprocess.run(['pio', 'run', '-e', 'lampada'], check=True)"`
Expected: Build succeeds (no functional changes yet)

- [ ] **Step 4: Commit**

```bash
git add clients/esp8266_lampada/src/main.cpp
git commit -m "feat(lampada): add repeater tracking data structures"
```

---

### Task 2: Add tracking logic to repeater code path

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp:462-475`

**Interfaces:**
- Consumes: `track_repeater_client()`, `s_repeater_fwd`
- Produces: Updated repeater forwarding with statistics

- [ ] **Step 1: Add tracking to repeater forwarding**

Replace the repeater block (lines 462-475) with:

```c
    /* Repeater: forward messages between clients and gateway */
    if (s_paired && s_use_repeater)
    {
        if (mac_equal(mac, s_gateway_mac))
        {
            /* From gateway -> broadcast (other devices check target_mac) */
            esp_now_send(s_broadcast_mac, data, len);
            s_repeater_fwd++;
        }
        else
        {
            /* From client -> forward to gateway */
            track_repeater_client(mac);
            esp_now_send(s_gateway_mac, data, len);
            s_repeater_fwd++;
        }
    }
```

- [ ] **Step 2: Verify compilation**

Run: `cd /mnt/c/project/homeware && python -c "import subprocess; subprocess.run(['pio', 'run', '-e', 'lampada'], check=True)"`
Expected: Build succeeds

- [ ] **Step 3: Commit**

```bash
git add clients/esp8266_lampada/src/main.cpp
git commit -m "feat(lampada): add repeater packet tracking"
```

---

### Task 3: Extend /api/state with repeater fields

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp:866-898`

**Interfaces:**
- Consumes: `s_use_repeater`, `s_repeater_fwd`, `s_rep_clients[]`, `s_rep_client_num`
- Produces: JSON fields `repeater_active`, `repeater_fwd`, `repeater_clients[]`

- [ ] **Step 1: Add repeater fields to handle_api_state**

In `handle_api_state()`, after `doc["free_heap"] = ESP.getFreeHeap();` (line 893), add:

```c
        doc["repeater_active"] = s_use_repeater;
        if (s_use_repeater)
        {
            doc["repeater_fwd"] = s_repeater_fwd;
            JsonArray rep_clients = doc["repeater_clients"].to<JsonArray>();
            for (int i = 0; i < s_rep_client_num; i++)
            {
                JsonObject obj = rep_clients.add<JsonObject>();
                char mac_str[18];
                mac_to_str(s_rep_clients[i].mac, mac_str, sizeof(mac_str));
                obj["mac"] = mac_str;
                obj["packets"] = s_rep_clients[i].pkt_count;
            }
        }
```

- [ ] **Step 2: Verify compilation**

Run: `cd /mnt/c/project/homeware && python -c "import subprocess; subprocess.run(['pio', 'run', '-e', 'lampada'], check=True)"`
Expected: Build succeeds

- [ ] **Step 3: Commit**

```bash
git add clients/esp8266_lampada/src/main.cpp
git commit -m "feat(lampada): extend /api/state with repeater statistics"
```

---

### Task 4: Add Repeater sidebar and section to dashboard

**Files:**
- Modify: `clients/esp8266_lampada/include/pages.h:69-133` (sidebar nav and content sections)

**Interfaces:**
- Consumes: `repeater_active`, `repeater_fwd`, `repeater_clients[]` from `/api/state`
- Produces: Sidebar nav item, repeater section with stats

- [ ] **Step 1: Add Repeater nav item in sidebar**

In the sidebar nav (after the Configurações nav-item, line 81), add:

```html
<div class="nav-item" data-section="repeater" onclick="showSection('repeater')" id="navRepeater" style="display:none"><span>📡</span><span>Repeater</span></div>
```

- [ ] **Step 2: Add Repeater section content**

After the `secConfig` div (after line 132), add:

```html
<div class="section" id="secRepeater">
<h1>Repeater</h1>
<div class="row"><span class="label">Total Retransmitidos</span><span class="value" id="repFwd">0</span></div>
<div id="repClientList"></div>
</div>
```

- [ ] **Step 3: Add JavaScript for repeater data**

In the `<script>` section, after the `fetchSettings` function (around line 202), add:

```javascript
async function fetchRepeater(){
  try{
    let r=await fetch('/api/state');
    let d=await r.json();
    let nav=document.getElementById('navRepeater');
    if(d.repeater_active){
      nav.style.display='flex';
      document.getElementById('repFwd').textContent=d.repeater_fwd||0;
      let list=document.getElementById('repClientList');
      list.innerHTML='';
      if(d.repeater_clients&&d.repeater_clients.length){
        d.repeater_clients.forEach(function(c){
          let div=document.createElement('div');
          div.className='row';
          div.innerHTML='<span class="label">'+c.mac+'</span><span class="value">'+c.packets+' pkts</span>';
          list.appendChild(div);
        });
      } else {
        list.innerHTML='<div class="row"><span class="label">Nenhum cliente visto</span></div>';
      }
    } else {
      nav.style.display='none';
    }
  }catch(e){}
}
```

- [ ] **Step 4: Update setInterval to include fetchRepeater**

Change the existing `setInterval` (line 217) to:

```javascript
setInterval(function(){fetchState();fetchRepeater();if(currentSection==='timer')fetchTimers()},3000);
```

- [ ] **Step 5: Add fetchRepeater to initial calls**

After `fetchState();fetchSettings();fetchTimers();` (line 218), add:

```javascript
fetchRepeater();
```

- [ ] **Step 6: Verify compilation**

Run: `cd /mnt/c/project/homeware && python -c "import subprocess; subprocess.run(['pio', 'run', '-e', 'lampada'], check=True)"`
Expected: Build succeeds

- [ ] **Step 7: Commit**

```bash
git add clients/esp8266_lampada/include/pages.h
git commit -m "feat(lampada): add repeater section to dashboard"
```

---

### Task 5: Add serial command for repeater stats

**Files:**
- Modify: `clients/esp8266_lampada/src/main.cpp` (serial handler)

**Interfaces:**
- Consumes: `s_repeater_fwd`, `s_rep_clients[]`, `s_rep_client_num`
- Produces: Serial output on 'p' command

- [ ] **Step 1: Find the serial handler**

Locate the `handle_serial` function and its switch statement.

- [ ] **Step 2: Add 'p' case to serial handler**

In the switch statement, add a new case:

```c
    case 'p':
    case 'P':
        if (s_use_repeater)
        {
            console.printf("\n--- Repeater Stats ---\n");
            console.printf("  Forwarded: %lu\n", s_repeater_fwd);
            console.printf("  Clients:   %d\n", s_rep_client_num);
            for (int i = 0; i < s_rep_client_num; i++)
            {
                char mac_str[18];
                mac_to_str(s_rep_clients[i].mac, mac_str, sizeof(mac_str));
                console.printf("  %d: %s (%lu pkts)\n", i, mac_str, s_rep_clients[i].pkt_count);
            }
            console.printf("----------------------\n\n");
        }
        else
        {
            console.printf("[%s] Repeater mode disabled\n", TAG);
        }
        break;
```

- [ ] **Step 3: Update help text**

In the help case ('h'/'H'/'?'), add:

```c
        console.printf("  p    - repeater stats\n");
```

- [ ] **Step 4: Reset repeater counters in 'c' command**

In the 'c'/'C' case, add:

```c
        s_repeater_fwd = 0;
        for (int i = 0; i < s_rep_client_num; i++)
            s_rep_clients[i].pkt_count = 0;
```

- [ ] **Step 5: Verify compilation**

Run: `cd /mnt/c/project/homeware && python -c "import subprocess; subprocess.run(['pio', 'run', '-e', 'lampada'], check=True)"`
Expected: Build succeeds

- [ ] **Step 6: Commit**

```bash
git add clients/esp8266_lampada/src/main.cpp
git commit -m "feat(lampada): add serial command for repeater stats"
```
