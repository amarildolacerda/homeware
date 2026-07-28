# Heltec HTIT-W32LA Board Variant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add PlatformIO env + conditional pin config for Heltec WiFi LoRa 32 V2 (HTIT-W32LA) alongside existing TTGO LORA32 support.

**Architecture:** Single codebase, compile-time switch via `-D HELTEC_W32LA` build flag in a new PlatformIO env. Pin definitions in `lora_config.h` and `display_config.h` use `#ifdef`. Display init adds a reset pulse for Heltec's OLED.

**Tech Stack:** Arduino framework, ESP32, sandeepmistry/LoRa, Adafruit SSD1306

## Global Constraints

- Keep existing `hub_32_lora` (TTGO) env unchanged
- New env `hub_32_lora_heltec` extends `hub_32` with `-D HELTEC_W32LA`
- No new libraries, no protocol changes, no logic changes beyond pin config + reset pulse
- Follow the pattern of `[env:hub_32_lora]` in platformio.ini

---

### Task 1: PlatformIO env + conditional pin configs

**Files:**
- Modify: `hub/platformio.ini`
- Modify: `hub/include/lora_config.h`
- Modify: `hub/include/display_config.h`

**Interfaces:**
- Consumes: existing `[env:hub_32]` as base
- Produces: `HELTEC_W32LA` build flag available for config headers

- [ ] **Step 1: Add `hub_32_lora_heltec` env to platformio.ini**

Add after the existing `hub_32_lora` block:

```ini
[env:hub_32_lora_heltec]
extends = env:hub_32
build_flags =
    ${env:hub_32.build_flags}
    -D HELTEC_W32LA
    -D HABILITA_LORA
    -D HABILITA_DISPLAY_TTGO
lib_deps =
    ${env:hub_32.lib_deps}
    sandeepmistry/LoRa @ ^0.8.0
    adafruit/Adafruit SSD1306 @ ^2.5.7
    adafruit/Adafruit GFX Library @ ^1.11.9
```

- [ ] **Step 2: Update `lora_config.h` with Heltec pins**

Replace the hardcoded RST/DIO0 with `#ifdef`:

```c
#ifdef HELTEC_W32LA
#define LORA_SS     18
#define LORA_RST    14
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27
#else
// TTGO LORA32 T3_v1.6 — SX1278 via sandeepmistry/LoRa
#define LORA_SS     18
#define LORA_RST    23
#define LORA_DIO0   -1   // DIO0 nao conectado (usa DIO1=33 p/ polling)
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27
#endif
```

- [ ] **Step 3: Update `display_config.h` with Heltec OLED pins**

Replace hardcoded SDA/SCL/RST with `#ifdef`:

```c
#ifdef HELTEC_W32LA
#define DISPLAY_SDA      4
#define DISPLAY_SCL      15
#define DISPLAY_RST      16
#else
#define DISPLAY_SDA      21
#define DISPLAY_SCL      22
#define DISPLAY_RST      -1
#endif
```

- [ ] **Step 4: Verify build for both envs**

```bash
cd /mnt/c/git/homeware/hub
pio run -e hub_32_lora && echo "TTGO OK" || echo "TTGO FAIL"
pio run -e hub_32_lora_heltec && echo "HELTEC OK" || echo "HELTEC FAIL"
```

Expected: both print "OK"

- [ ] **Step 5: Commit**

```bash
git add hub/platformio.ini hub/include/lora_config.h hub/include/display_config.h
git commit -m "feat(hub): add Heltec HTIT-W32LA board variant env"
```

---

### Task 2: OLED reset pulse for Heltec

**Files:**
- Modify: `hub/src/display_handler.cpp`

**Interfaces:**
- Consumes: `HELTEC_W32LA` flag from Task 1
- Consumes: `DISPLAY_RST` (now 16 for Heltec) from `display_config.h`

- [ ] **Step 1: Add Heltec OLED reset pulse before `display.begin()`**

In `display_handler_init()`, before `display.begin()`, add:

```cpp
#ifdef HELTEC_W32LA
    pinMode(DISPLAY_RST, OUTPUT);
    digitalWrite(DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(DISPLAY_RST, HIGH);
    delay(10);
#endif
```

The full function after edit (lines 25-41):

```cpp
void display_handler_init(void) {
#ifdef HELTEC_W32LA
    pinMode(DISPLAY_RST, OUTPUT);
    digitalWrite(DISPLAY_RST, LOW);
    delay(10);
    digitalWrite(DISPLAY_RST, HIGH);
    delay(10);
#endif
    s_display_ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR);
    // ... rest unchanged
```

- [ ] **Step 2: Build check**

```bash
cd /mnt/c/git/homeware/hub
pio run -e hub_32_lora_heltec && echo "HELTEC OK"
pio run -e hub_32_lora && echo "TTGO OK"
```

Expected: both build without errors.

- [ ] **Step 3: Commit**

```bash
git add hub/src/display_handler.cpp
git commit -m "fix(hub): add OLED reset pulse for Heltec HTIT-W32LA"
```
