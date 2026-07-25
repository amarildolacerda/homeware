# AgriSense IoT Rebranding — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Rename all project artifacts from maker/student-facing names (ESP8266, ESP32, Arduino) to professional platform names under "AgriSense IoT".

**Architecture:** Shared submodule → root config → hub → server → nodes → docs. Each task renames one conceptual unit with coordinated folder, code, and build-config changes.

**Tech Stack:** PlatformIO, C++ (ESP8266/ESP32), Python (bridge/server), git submodules

## Global Constraints

- All changes on `dev` branch only
- shared submodule changes must be committed to `homeware_shared.git` (dev branch) before bumping reference
- Device ID format: `esp8266_<chip_id>` → `agri_<chip_id>`
- HW_CHIP enums: `HW_CHIP_ESP8266` → `HW_CHIP_ESP_1`, `HW_CHIP_ESP32` → `HW_CHIP_ESP_2`, `HW_CHIP_ESP32C3` → `HW_CHIP_ESP_3`
- Architecture: `bridge/` → `server/`, `gateway/` → `hub/`, `clients/` → `nodes/`
- Node folder names: `esp8266_lampada` → `lamp`, `esp8266_dht_gas` → `climate-gas`, `esp8266_pir` → `presence`, `esp8266_pir_bat` → `presence-bat`, `esp8266_chuva` → `rain`, `esp8266_onoff` → `switch`, `esp8266_soil_moisture_bat` → `soil-moisture`, `esp8266_repeater` → `extender`
- PlatformIO env names: drop `esp8266_` prefix (e.g. `esp8266_lamp_ota` → `lamp_ota`)

---

### Task 1: Shared submodule — HW_CHIP enums + platform prefix

**Files:**
- Modify: `shared/src/espnow_protocol.h` (lines 52-56)
- Modify: `shared/src/platform.h` (lines 10, 30)

**Interfaces:**
- Produces: `chip_type_t` enum with renamed values `HW_CHIP_ESP_1`, `HW_CHIP_ESP_2`, `HW_CHIP_ESP_3`; `PLATFORM_PREFIX` changed from `esp8266`/`esp32` to `agri`

- [ ] **Step 1: Edit `espnow_protocol.h`** — rename chip enum values

```cpp
// espnow_protocol.h line 52-56 — change:
typedef enum {
    HW_CHIP_UNKNOWN = 0xFF,
    HW_CHIP_ESP_1 = 0,   // was HW_CHIP_ESP8266
    HW_CHIP_ESP_2 = 1,   // was HW_CHIP_ESP32
} chip_type_t;
```

- [ ] **Step 2: Edit `platform.h`** — change `PLATFORM_PREFIX`

```cpp
// platform.h line 10 — change:
#define PLATFORM_PREFIX "agri"

// platform.h line 30 — change:
#define PLATFORM_PREFIX "agri"
```

- [ ] **Step 3: Commit shared submodule (dev branch)**

```bash
git add src/espnow_protocol.h src/platform.h
git commit -m "refactor: rename HW_CHIP enums to ESP_1/ESP_2, PLATFORM_PREFIX to agri"
git push origin dev
```

---

### Task 2: Root — platformio.ini + build/scripts

**Files:**
- Modify: `platformio.ini`
- Modify: `flash.sh`, `flash.ps1`, `monitor.sh`, `monitor.ps1`, `monitor.py`, `erase.sh`, `erase.ps1` (if they reference client paths)

- [ ] **Step 1: Update platformio.ini env names and build_src_filter paths**

Change every `env:esp8266_*` to `env:*` (drop prefix). Change every `clients/esp8266_*` to `nodes/*`. Change `gateway/` to `hub/`.

Env name mapping:
```
esp8266_gateway       → hub_8266
esp8266_gateway_ota   → hub_8266_ota
esp32_gateway         → hub_32
esp32_gateway_ota     → hub_32_ota
esp32C3_gateway       → hub_32c3
esp8266_lamp          → lamp
esp8266_lamp_ota      → lamp_ota
esp8266_onoff         → switch
esp8266_onoff_ota     → switch_ota
esp8266_pir           → presence
esp8266_pir_ota       → presence_ota
esp8266_dht           → climate-gas
esp8266_dht_ota       → climate-gas_ota
esp8266_soil_moisture → soil-moisture
esp8266_soil_moisture_ota → soil-moisture_ota
```

build_src_filter changes:
```
clients/esp8266_lampada/src/** → nodes/lamp/src/**
clients/esp8266_onoff/src/** → nodes/switch/src/**
clients/esp8266_pir/src/** → nodes/presence/src/**
clients/esp8266_dht_gas/src/** → nodes/climate-gas/src/**
clients/esp8266_soil_moisture/src/** → nodes/soil-moisture/src/**
gateway/src/** → hub/src/**
```

build_flags includes:
```
-Iclients/esp8266_lampada/include → -Inodes/lamp/include
-Iclients/esp8266_onoff/include → -Inodes/switch/include
-Iclients/esp8266_pir/include → -Inodes/presence/include
-Iclients/esp8266_dht_gas/include → -Inodes/climate-gas/include
-Iclients/esp8266_soil_moisture/include → -Inodes/soil-moisture/include
-Igateway/include → -Ihub/include
-Igateway/src → -Ihub/src
```

Gateway boot partitions path:
```
gateway/partitions.csv → hub/partitions.csv
```

- [ ] **Step 2: Update build scripts** — check `flash.sh`, `monitor.sh` etc for any client path references. If found, update paths. If scripts use `clients/esp8266_*` references, update to `nodes/*`.

- [ ] **Step 3: Commit**

```bash
git add platformio.ini flash.sh monitor.sh flash.ps1 monitor.ps1 erase.sh erase.ps1 2>/dev/null
git commit -m "refactor: rename platformio envs and paths for agrisense"
```

---

### Task 3: Hub (formerly gateway) — folder rename + device ID

**Files:**
- Rename: `gateway/` → `hub/`
- Modify: `hub/src/main.cpp` (dashboard strings, device ID format)
- Modify: `hub/src/sensor_registry.cpp` (device ID format)
- Modify: `hub/src/bridge_client.cpp` (device ID format)
- Modify: `hub/partitions.csv` (if exists, path already updated in Task 2)

- [ ] **Step 1: Rename folder**

```bash
git mv gateway hub
```

- [ ] **Step 2: Update device_id format** — search all `.cpp`/`.h` files in hub/ for `esp8266_` and replace with `agri_`

Files likely to contain:
- `hub/src/sensor_registry.cpp` — `snprintf(s->bridge_device_id, ..., "esp8266_gw_%02X%02X%02X_sensor_%d", ...)`
  → `snprintf(s->bridge_device_id, ..., "agri_gw_%02X%02X%02X_sensor_%d", ...)`
- `hub/src/bridge_client.cpp` — `"esp8266_%06x_sensor_%d"` → `"agri_%06x_sensor_%d"`

- [ ] **Step 3: Update dashboard strings** — in hub dashboard HTML, remove any "ESP8266"/"ESP32" mentions. Change display name/header from "Gateway" to "Hub".

- [ ] **Step 4: Verify includes** — ensure `hub/src/` includes resolve with the new path structure. No `#include "../gateway/"` references.

- [ ] **Step 5: Commit**

```bash
git add hub/
git commit -m "refactor: rename gateway to hub, update device_id to agri_ prefix"
```

---

### Task 4: Server (formerly bridge submodule) — folder rename + device ID

**Files:**
- Rename: `bridge/` → `server/`
- Modify: `.gitmodules` (submodule path)
- Modify: `server/bridge_python/*.py` (device ID format if any)

- [ ] **Step 1: Update .gitmodules** — change submodule path from `bridge` to `server`

```ini
[submodule "server"]
    path = server
    url = https://github.com/amarildolacerda/homeware_bridge.git
    branch = dev
```

- [ ] **Step 2: Rename submodule directory**

```bash
git mv bridge server
```

- [ ] **Step 3: Update device_id in Python server** — grep for `esp8266_` in `server/bridge_python/` and replace with `agri_`

- [ ] **Step 4: Commit**

```bash
git add .gitmodules server/
git commit -m "refactor: rename bridge submodule to server"
```

---

### Task 5: Nodes (formerly clients) — folder renames + device ID

**Files:**
- Rename: `clients/` → `nodes/`
- Rename all `clients/esp8266_*` → `nodes/*` per approved mapping
- Modify: every node's `src/main.cpp` (device_id format)

- [ ] **Step 1: Rename clients/ → nodes/**

```bash
git mv clients nodes
```

- [ ] **Step 2: Rename each node folder**

```bash
# Create new folder structure
mkdir -p nodes/lamp nodes/switch nodes/presence nodes/presence-bat nodes/climate-gas nodes/rain nodes/soil-moisture nodes/extender

# Move contents (git mv within nodes/ for each)
git mv nodes/esp8266_lampada/* nodes/lamp/
git mv nodes/esp8266_onoff/* nodes/switch/
git mv nodes/esp8266_pir/* nodes/presence/
git mv nodes/esp8266_pir_bat/* nodes/presence-bat/
git mv nodes/esp8266_dht_gas/* nodes/climate-gas/
git mv nodes/esp8266_chuva/* nodes/rain/
git mv nodes/esp8266_soil_moisture_bat/* nodes/soil-moisture/
git mv nodes/esp8266_repeater/* nodes/extender/
git mv nodes/SPEC.md nodes/SPEC.md  # will be overwritten

# Remove old folders
rm -rf nodes/esp8266_*
```

- [ ] **Step 3: Update device_id format in every main.cpp** — replace `"esp8266_%06x"` with `"agri_%06x"` in all node `src/main.cpp` files:

```cpp
// In each main.cpp, change:
snprintf(s_device_id, sizeof(s_device_id), "esp8266_%06x", chip_id);
// to:
snprintf(s_device_id, sizeof(s_device_id), "agri_%06x", chip_id);
```

Affected files:
- `nodes/lamp/src/main.cpp`
- `nodes/switch/src/main.cpp`
- `nodes/presence/src/main.cpp`
- `nodes/presence-bat/src/main.cpp`
- `nodes/climate-gas/src/main.cpp`
- `nodes/rain/src/main.cpp`
- `nodes/soil-moisture/src/main.cpp`
- `nodes/extender/src/main.cpp`

Also check for any `"esp8266_"` string literals in dashboard HTML within node sources.

- [ ] **Step 4: Add `HW_CHIP_` constant in each node's setup** — update `espnow_pair_request_t.client_chip` assignment from `HW_CHIP_ESP8266` to `HW_CHIP_ESP_1` (if present).

- [ ] **Step 5: Commit**

```bash
git add nodes/
git commit -m "refactor: rename clients to nodes, update device_id to agri_ prefix"
```

---

### Task 6: Docs — README, AGENTS.md, CLIENTS_SPEC

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `nodes/SPEC.md` (was clients/SPEC.md)
- Modify: `hub/SPEC_ESPNOW.md` (was gateway/SPEC_ESPNOW.md)

- [ ] **Step 1: Update README.md** — rewrite architecture diagram, device table, references:

Architecture diagram:
```
Sensores/Atuadores ──► Hub (ESP-NOW ch.1) ──► Server (HTTP) ──► Home Assistant (MQTT)
  (AgriSense Nodes)           (Hub)                    (Python)            (MQTT Discovery)
```

Device table: update names from `esp8266_lampada` → `lamp`, etc. Remove "ESP8266" mentions.

- [ ] **Step 2: Update AGENTS.md**
- Architecture section: `clients` → `nodes`, `gateway` → `hub`, `bridge` → `server`
- Rule 1: `esp8266_<chip_id>` → `agri_<chip_id>`
- Rule 13: "HA bridge Python" → "Server Python"
- Clients estáveis section: update paths, names, versions
- All ESP8266/ESP32 mentions that appear in client-facing descriptions → generic "AgriSense IoT"

- [ ] **Step 3: Update nodes/SPEC.md** — replace `esp8266_%06x` with `agri_%06x`, update example paths, update env names

- [ ] **Step 4: Update hub/SPEC_ESPNOW.md** — replace ESP8266/ESP32 chip names with generic architecture names where applicable (keep technical protocol details about broadcast/unicast but update architecture references)

- [ ] **Step 5: Commit**

```bash
git add README.md AGENTS.md nodes/SPEC.md hub/SPEC_ESPNOW.md
git commit -m "docs: update references for agrisense rebranding"
```

---

### Task 7: Bump shared submodule reference

- [ ] **Step 1: Update submodule pointer** after shared is committed

```bash
cd shared
git checkout dev
git pull origin dev  # ensure we have the HW_CHIP rename commit
cd ..
git add shared
```

- [ ] **Step 2: Commit submodule bump**

```bash
git commit -m "chore: bump shared submodule for HW_CHIP rename"
```

---

### Task 8: Verify build

- [ ] **Step 1: Verify syntax** — at minimum, verify one node builds

```bash
# Try building a node
pio run -d . -e lamp
# Try building hub
pio run -d . -e hub_8266
```

- [ ] **Step 2: Fix any build errors** related to path resolution, include guards, or renamed references.

- [ ] **Step 3: Commit fixes** if needed.

---

### Task 9: Final status commit

- [ ] **Step 1: Check git status** for any remaining uncommitted changes

```bash
git status
git diff --stat
```

- [ ] **Step 2: Verify no stale `esp8266_` or `ESP8266` references remain in customer-facing code/docs**

```bash
rg "esp8266_" --include="*.md" --include="*.cpp" --include="*.h" --include="*.py" --include="*.ini"
rg "ESP8266" --include="*.md" --include="*.cpp" --include="*.h" --include="*.py" --include="*.ini"
```

Expected: only technical internal references (chip-specific code, not visible to customers).

- [ ] **Step 3: Commit wrap-up**

```bash
git add -A
git commit -m "chore: final cleanup for agrisense rebranding"
```
