# Telnet Console Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add remote telnet console to gateway ESP8266 using ESPTelnet library.

**Architecture:** New `console` module wraps Serial + telnet output. Telnet client connects to port 23, receives all debug output, sends commands via line input.

**Tech Stack:** ESP8266, ESPTelnet library, WiFiServer

## Global Constraints

- ESPTelnet library from PlatformIO registry
- Single telnet client at a time
- Non-blocking loop pattern (no delay())
- Serial continues single-key; telnet uses line + Enter
- All debug output (`Serial.printf`/`Serial.println`) routed through `console_printf`

---

### Task 1: Add ESPTelnet dependency

**Files:**
- Modify: `platformio.ini`

- [ ] **Add ESPTelnet to lib_deps**

```
lib_deps =
    ESP Telnet
```

- [ ] **Build to verify library downloads**

```bash
~/.platformio/penv/bin/platformio run 2>&1 | tail -5
```

Expected: SUCCESS, library auto-resolved.

---

### Task 2: Create console module

**Files:**
- Create: `include/console.h`
- Create: `src/console.cpp`

**Interfaces:**
- Produces: `void console_init()`, `void console_loop()`, `void console_printf(const char *fmt, ...)`, `void handle_console(char c)`

- [ ] **Create `include/console.h`**

```cpp
#pragma once
#include <Arduino.h>

void console_init();
void console_loop();
void console_printf(const char *fmt, ...);

#ifdef __cplusplus
extern "C" {
#endif
void handle_console(char c);
#ifdef __cplusplus
}
#endif
```
