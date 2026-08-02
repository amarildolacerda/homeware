# Unificação de Flags de Rádio - Plano de Implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unificar `LORA_DEVICE`+`HABILITA_LORA`→`LORA_ENABLED` e `HABILITA_ESPNOW`→`ESPNOW_ENABLED`

**Architecture:** Rename mecânico via find-replace. Sem mudança de lógica. Guard em `espnow_protocol.h` ajustado para `#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)`.

**Tech Stack:** PlatformIO, C/C++ preprocessor flags

## Global Constraints

- Todos os hub e nodes devem compilar após as alterações
- Flags: `LORA_ENABLED` (substitui `LORA_DEVICE` + `HABILITA_LORA`), `ESPNOW_ENABLED` (substitui `HABILITA_ESPNOW`)
- Guard shared: `#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)`

---

### Task 1: Atualizar hub/platformio.ini

**Files:**
- Modify: `hub/platformio.ini`

**Steps:**

- [ ] **Step 1: Rename flags em hub/platformio.ini**

Substituir:
- `HABILITA_ESPNOW` → `ESPNOW_ENABLED` (env `hub_espnow`)
- `HABILITA_LORA` + `LORA_DEVICE` → `LORA_ENABLED` (env `hub_32_lora`)
- `HABILITA_LORA` + `LORA_DEVICE` + `HABILITA_ESPNOW` → `LORA_ENABLED` + `ESPNOW_ENABLED` (env `hub_32_lora_heltec`)

- [ ] **Step 2: Verificar que nenhuma referência antiga permanece**

```bash
grep -n "HABILITA_\|LORA_DEVICE" hub/platformio.ini
```
Expected: 0 resultados

---

### Task 2: Atualizar nodes/onoff-lora/platformio.ini

**Files:**
- Modify: `nodes/onoff-lora/platformio.ini`

**Steps:**

- [ ] **Step 1: Rename LORA_DEVICE → LORA_ENABLED**

- [ ] **Step 2: Verificar**

```bash
grep -n "LORA_DEVICE" nodes/onoff-lora/platformio.ini
```
Expected: 0 resultados

---

### Task 3: Atualizar shared/src/espnow_protocol.h

**Files:**
- Modify: `shared/src/espnow_protocol.h:10-16` (include guard)
- Modify: `shared/src/espnow_protocol.h:228-248` (wrapper guard)

**Steps:**

- [ ] **Step 1: Atualizar include guard (linha 10)**

```cpp
// Antes:
#if !defined(LORA_DEVICE) || defined(HABILITA_ESPNOW)

// Depois:
#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)
```

- [ ] **Step 2: Atualizar wrapper guard (linha 228)**

```cpp
// Antes:
#if !defined(LORA_DEVICE) || defined(HABILITA_ESPNOW)
// ...
#endif // !LORA_DEVICE || HABILITA_ESPNOW

// Depois:
#if defined(ESPNOW_ENABLED) || !defined(LORA_ENABLED)
// ...
#endif // ESPNOW_ENABLED || !LORA_ENABLED
```

---

### Task 4: Atualizar shared/src/lora_spi_radio.cpp

**Files:**
- Modify: `shared/src/lora_spi_radio.cpp:2,57`

**Steps:**

- [ ] **Step 1: Rename LORA_DEVICE → LORA_ENABLED**

```cpp
// Antes:
#ifdef LORA_DEVICE
// ...
#endif // LORA_DEVICE

// Depois:
#ifdef LORA_ENABLED
// ...
#endif // LORA_ENABLED
```

---

### Task 5: Atualizar hub source code

**Files:**
- Modify: `hub/src/main.cpp` (9 ocorrências)
- Modify: `hub/src/espnow_handler.cpp:11,471`
- Modify: `hub/include/espnow_handler.h:9,119`
- Modify: `hub/src/lora_handler.cpp:1`
- Modify: `hub/include/pages.h:100,156,1298,1338,1454`

**Steps:**

- [ ] **Step 1: hub/main.cpp** — `HABILITA_ESPNOW` → `ESPNOW_ENABLED`, `HABILITA_LORA` → `LORA_ENABLED`

- [ ] **Step 2: hub/src/espnow_handler.cpp** — `HABILITA_ESPNOW` → `ESPNOW_ENABLED`

- [ ] **Step 3: hub/include/espnow_handler.h** — `HABILITA_ESPNOW` → `ESPNOW_ENABLED`

- [ ] **Step 4: hub/src/lora_handler.cpp** — `HABILITA_LORA` → `LORA_ENABLED`

- [ ] **Step 5: hub/include/pages.h** — `HABILITA_LORA` → `LORA_ENABLED`

- [ ] **Step 6: Verificar zero referências antigas**

```bash
grep -rn "HABILITA_\|LORA_DEVICE" hub/src/ hub/include/
```
Expected: 0 resultados

---

### Task 6: Atualizar docs

**Files:**
- Modify: `hub/SPEC_LORA.md`
- Modify: `nodes/SPEC.md`

**Steps:**

- [ ] **Step 1: hub/SPEC_LORA.md** — `HABILITA_LORA` → `LORA_ENABLED`

- [ ] **Step 2: nodes/SPEC.md** — `LORA_DEVICE` → `LORA_ENABLED`

---

### Task 7: Build verification

**Steps:**

- [ ] **Step 1: Build hub ESP-NOW**

```bash
source .venv/bin/activate && pio run -d hub -e hub_8266
```
Expected: SUCCESS

- [ ] **Step 2: Build hub LoRa Heltec**

```bash
pio run -d hub -e hub_32_lora_heltec
```
Expected: SUCCESS

- [ ] **Step 3: Build hub LoRa TTGO**

```bash
pio run -d hub -e hub_32_lora
```
Expected: SUCCESS

- [ ] **Step 4: Verificar zero referências antigas no projeto**

```bash
grep -rn "HABILITA_\|LORA_DEVICE" hub/ nodes/ shared/src/ --include="*.cpp" --include="*.h" --include="*.ini"
```
Expected: 0 resultados

- [ ] **Step 5: Commit**

```bash
git add -A && git commit -m "refactor: unifica flags de rádio LORA_ENABLED/ESPNOW_ENABLED"
```
