# Display Interface Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract display code from hub and onoff-lora into shared/ with a clean abstract interface + reusable page manager + per-driver implementations for TTGO and Heltec OLEDs.

**Architecture:** Shared layer has DisplayInterface (abstract), Ssd1306DisplayTtgo, Ssd1306DisplayHeltec, and PageManager. Hub and onoff-lora create the appropriate driver + PageManager with device-specific page callbacks. Build flags `DISPLAY_TTGO` / `DISPLAY_HELTEC` replace `HABILITA_DISPLAY`.

**Tech Stack:** C++17, Adafruit_SSD1306 (TTGO), HT_SSD1306Wire (Heltec), PlatformIO native test env.

## Global Constraints

- `shared/` is a git submodule — commits to both shared (dev) and parent (dev)
- Every new `.cpp` in shared/ must have `#ifdef DISPLAY_TTGO` or `#ifdef DISPLAY_HELTEC` compile guard
- PageManager must be non-blocking (millis(), no delay()), max 4 pages
- DisplayInterface is pure virtual — all methods must be overridable
- Native tests must compile without Arduino dependencies (use MockDisplay)
- Replace `HABILITA_DISPLAY` → `DISPLAY_TTGO` or `DISPLAY_HELTEC` in all platformio.ini files
- Remove `hub/include/display_config.h` — pins defined per driver default config
- GPIO, LED, button, relay logic must remain unchanged
- HTTP API endpoints must remain unchanged

---

### Task 1: DisplayInterface + PageManager + MockDisplay + Tests

**Files:**
- Create: `shared/src/display_interface.h`
- Create: `shared/src/page_manager.h`
- Create: `shared/src/page_manager.cpp`
- Create: `tests/unit/test/mock_display.h`
- Create: `tests/unit/test/test_page_manager.cpp`

**Interfaces:**
- Produces: `DisplayInterface` (abstract class), `PageManager` class, `page_render_t` typedef

- [ ] **Step 1: Create display_interface.h**

```cpp
#ifndef HW_SHARED_DISPLAY_INTERFACE_H
#define HW_SHARED_DISPLAY_INTERFACE_H

#include <stdint.h>

class DisplayInterface {
public:
    virtual ~DisplayInterface() {}
    virtual bool begin() = 0;
    virtual void clear() = 0;
    virtual void set_cursor(int x, int y) = 0;
    virtual void set_text_size(int size) = 0;
    virtual void print(const char* str) = 0;
    virtual void printf(const char* fmt, ...) = 0;
    virtual void display() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

#endif
```

- [ ] **Step 2: Create mock_display.h for native tests**

```cpp
#pragma once
#include "display_interface.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

class MockDisplay : public DisplayInterface {
public:
    bool m_begin_ret = true;
    bool m_begin_called = false;
    char m_last_text[128] = "";
    int m_cursor_x = 0, m_cursor_y = 0;
    int m_text_size = 1;
    int m_clear_count = 0;
    int m_display_count = 0;

    bool begin() override { m_begin_called = true; return m_begin_ret; }
    void clear() override { m_clear_count++; m_last_text[0] = '\0'; }
    void set_cursor(int x, int y) override { m_cursor_x = x; m_cursor_y = y; }
    void set_text_size(int size) override { m_text_size = size; }
    void print(const char* str) override {
        strncpy(m_last_text, str, sizeof(m_last_text) - 1);
    }
    void printf(const char* fmt, ...) override {
        va_list args;
        va_start(args, fmt);
        vsnprintf(m_last_text, sizeof(m_last_text), fmt, args);
        va_end(args);
    }
    void display() override { m_display_count++; }
    int width() const override { return 128; }
    int height() const override { return 64; }
};
```

- [ ] **Step 3: Create page_manager.h**

```cpp
#ifndef HW_SHARED_PAGE_MANAGER_H
#define HW_SHARED_PAGE_MANAGER_H

#include "display_interface.h"
#include <stdint.h>

using page_render_t = void (*)(DisplayInterface&);

class PageManager {
public:
    PageManager(DisplayInterface* display);

    void set_page_interval(unsigned long ms);
    bool add_page(page_render_t render_fn);
    void set_footer(page_render_t footer_fn);
    int  current() const { return m_current; }
    int  loop();

private:
    DisplayInterface* m_display;
    unsigned long m_interval_ms = 5000;
    unsigned long m_last_switch_ms = 0;
    uint8_t m_current = 0;
    uint8_t m_count = 0;
    static const uint8_t MAX_PAGES = 4;
    page_render_t m_pages[MAX_PAGES];
    page_render_t m_footer = nullptr;
};

#endif
```

- [ ] **Step 4: Create page_manager.cpp**

```cpp
#include "page_manager.h"

PageManager::PageManager(DisplayInterface* display)
    : m_display(display)
    , m_last_switch_ms(0)
    , m_current(0)
    , m_count(0)
{
    for (int i = 0; i < MAX_PAGES; i++) m_pages[i] = nullptr;
}

void PageManager::set_page_interval(unsigned long ms) {
    if (ms > 0) m_interval_ms = ms;
}

bool PageManager::add_page(page_render_t render_fn) {
    if (m_count >= MAX_PAGES || !render_fn) return false;
    m_pages[m_count++] = render_fn;
    return true;
}

void PageManager::set_footer(page_render_t footer_fn) {
    m_footer = footer_fn;
}

int PageManager::loop() {
    if (!m_display || m_count == 0) return -1;

    unsigned long now = millis();
    if (now - m_last_switch_ms < m_interval_ms) return m_current;
    m_last_switch_ms = now;

    m_current = (m_current + 1) % m_count;

    m_display->clear();
    if (m_pages[m_current]) {
        (*m_pages[m_current])(*m_display);
    }
    if (m_footer) {
        (*m_footer)(*m_display);
    }
    m_display->display();
    return m_current;
}
```

- [ ] **Step 5: Create test_page_manager.cpp**

```cpp
#include <unity.h>
#include "mock_display.h"
#include "page_manager.h"
#include <cstring>

static MockDisplay s_mock;
static PageManager s_mgr(&s_mock);
static int s_page1_called = 0;
static int s_page2_called = 0;
static int s_footer_called = 0;

static void page1(DisplayInterface& d) {
    s_page1_called++;
    d.print("Page 1");
}
static void page2(DisplayInterface& d) {
    s_page2_called++;
    d.print("Page 2");
}
static void footer(DisplayInterface& d) {
    s_footer_called++;
    d.print("Footer");
}
```

Test cases:

```cpp
void test_pagemanager_no_pages() {
    TEST_ASSERT_EQUAL(-1, s_mgr.loop());
}

void test_pagemanager_add_page() {
    TEST_ASSERT_TRUE(s_mgr.add_page(page1));
    TEST_ASSERT_TRUE(s_mgr.add_page(page2));
    TEST_ASSERT_FALSE(s_mgr.add_page(nullptr));
}

void test_pagemanager_switches_page() {
    s_mgr.set_page_interval(1);

    int first = s_mgr.loop();
    int second = s_mgr.loop();
    TEST_ASSERT_NOT_EQUAL(first, second);
}

void test_pagemanager_footer() {
    s_mgr.set_footer(footer);
    s_mgr.loop();
    TEST_ASSERT_TRUE(s_footer_called > 0);
}

void test_pagemanager_page_render() {
    int page = s_mgr.loop();
    TEST_ASSERT_EQUAL(1, page);
    TEST_ASSERT_EQUAL(0, strcmp(s_mock.m_last_text, "Page 2"));
}

void test_pagemanager_cycles_back() {
    s_mgr.loop(); // page 1 (starts at 0, switch to 1 on first loop)
    s_mgr.loop(); // page 0
    s_mgr.loop(); // page 1
    int page = s_mgr.loop(); // page 0
    TEST_ASSERT_EQUAL(0, page);
    TEST_ASSERT_EQUAL(0, strcmp(s_mock.m_last_text, "Page 1"));
}
```

- [ ] **Step 6: Add tests to test_native.cpp and run**

```bash
cd tests/unit && pio test -e native
```

Expected: all tests PASS (add to existing test runner, similar to Task 4 pattern with forward declarations).

- [ ] **Step 7: Commit**

```bash
git -C shared add src/display_interface.h src/page_manager.h src/page_manager.cpp
git -C shared commit -m "feat(shared): add DisplayInterface + PageManager"
git add shared tests/unit/test/mock_display.h tests/unit/test/test_page_manager.cpp
git commit -m "feat(shared): add DisplayInterface + PageManager (with tests)"
```

---

### Task 2: Ssd1306DisplayTtgo driver

**Files:**
- Create: `shared/src/display_ttgo.h`
- Create: `shared/src/display_ttgo.cpp`
- Modify: `nodes/onoff-lora/platformio.ini` (add `-D DISPLAY_TTGO`)

**Interfaces:**
- Consumes: `DisplayInterface`
- Produces: `Ssd1306DisplayTtgo : DisplayInterface`

- [ ] **Step 1: Create display_ttgo.h**

```cpp
#ifndef HW_SHARED_DISPLAY_TTGO_H
#define HW_SHARED_DISPLAY_TTGO_H

#include "display_interface.h"
#include <stdint.h>

struct Ssd1306TtgoConfig {
    int8_t sda = 21;
    int8_t scl = 22;
    int8_t rst = -1;
    uint8_t addr = 0x3C;
    int width = 128;
    int height = 64;
};

class Ssd1306DisplayTtgo : public DisplayInterface {
public:
    Ssd1306DisplayTtgo(const Ssd1306TtgoConfig& cfg = Ssd1306TtgoConfig{});
    bool begin() override;
    void clear() override;
    void set_cursor(int x, int y) override;
    void set_text_size(int size) override;
    void print(const char* str) override;
    void printf(const char* fmt, ...) override;
    void display() override;
    int width() const override;
    int height() const override;
private:
    Ssd1306TtgoConfig m_cfg;
#ifdef DISPLAY_TTGO
    Adafruit_SSD1306 m_display;
#endif
};

#endif
```

- [ ] **Step 2: Create display_ttgo.cpp**

```cpp
#ifdef DISPLAY_TTGO

#include "display_ttgo.h"
#include <stdarg.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>

// Constructor needs to be in .cpp because Adafruit_SSD1306 constructor needs params
Ssd1306DisplayTtgo::Ssd1306DisplayTtgo(const Ssd1306TtgoConfig& cfg)
    : m_cfg(cfg)
#ifdef DISPLAY_TTGO
    , m_display(cfg.width, cfg.height, &Wire, cfg.rst)
#endif
{}

bool Ssd1306DisplayTtgo::begin() {
#ifdef DISPLAY_TTGO
    Wire.begin(m_cfg.sda, m_cfg.scl);
    return m_display.begin(SSD1306_SWITCHCAPVCC, m_cfg.addr);
#else
    return false;
#endif
}

void Ssd1306DisplayTtgo::clear() {
#ifdef DISPLAY_TTGO
    m_display.clearDisplay();
#endif
}

void Ssd1306DisplayTtgo::set_cursor(int x, int y) {
#ifdef DISPLAY_TTGO
    m_display.setCursor(x, y);
#endif
}

void Ssd1306DisplayTtgo::set_text_size(int size) {
#ifdef DISPLAY_TTGO
    m_display.setTextSize(size);
#endif
}

void Ssd1306DisplayTtgo::print(const char* str) {
#ifdef DISPLAY_TTGO
    m_display.print(str);
#endif
}

void Ssd1306DisplayTtgo::printf(const char* fmt, ...) {
#ifdef DISPLAY_TTGO
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    m_display.print(buf);
#endif
}

void Ssd1306DisplayTtgo::display() {
#ifdef DISPLAY_TTGO
    m_display.display();
#endif
}

int Ssd1306DisplayTtgo::width() const { return m_cfg.width; }
int Ssd1306DisplayTtgo::height() const { return m_cfg.height; }

#endif // DISPLAY_TTGO
```

- [ ] **Step 3: Build test via onoff-lora**

```bash
cd nodes/onoff-lora && pio run -e lora_esp32
```

Expected: SUCCESS (no errors).

- [ ] **Step 4: Commit**

```bash
git -C shared add src/display_ttgo.h src/display_ttgo.cpp
git -C shared commit -m "feat(shared): add Ssd1306DisplayTtgo driver"
git add shared
git commit -m "feat(shared): add Ssd1306DisplayTtgo driver"
```

---

### Task 3: Ssd1306DisplayHeltec driver

**Files:**
- Create: `shared/src/display_heltec.h`
- Create: `shared/src/display_heltec.cpp`
- Modify: `hub/platformio.ini` (replace `HABILITA_DISPLAY` with `DISPLAY_HELTEC`)

**Interfaces:**
- Consumes: `DisplayInterface`
- Produces: `Ssd1306DisplayHeltec : DisplayInterface`

- [ ] **Step 1: Create display_heltec.h**

```cpp
#ifndef HW_SHARED_DISPLAY_HELTEC_H
#define HW_SHARED_DISPLAY_HELTEC_H

#include "display_interface.h"
#include <stdint.h>

struct Ssd1306HeltecConfig {
    int8_t sda = 4;
    int8_t scl = 15;
    int8_t rst = 16;
    uint8_t addr = 0x3C;
    int width = 128;
    int height = 64;
};

class Ssd1306DisplayHeltec : public DisplayInterface {
public:
    Ssd1306DisplayHeltec(const Ssd1306HeltecConfig& cfg = Ssd1306HeltecConfig{});
    bool begin() override;
    void clear() override;
    void set_cursor(int x, int y) override;
    void set_text_size(int size) override;
    void print(const char* str) override;
    void printf(const char* fmt, ...) override;
    void display() override;
    int width() const override;
    int height() const override;
private:
    Ssd1306HeltecConfig m_cfg;
    int m_cx = 0, m_cy = 0;
#ifdef DISPLAY_HELTEC
    SSD1306Wire m_display;
#endif
};

#endif
```

- [ ] **Step 2: Create display_heltec.cpp**

```cpp
#ifdef DISPLAY_HELTEC

#include "display_heltec.h"
#include <HT_SSD1306Wire.h>
#include <Wire.h>
#include <stdio.h>
#include <stdarg.h>

Ssd1306DisplayHeltec::Ssd1306DisplayHeltec(const Ssd1306HeltecConfig& cfg)
    : m_cfg(cfg)
#ifdef DISPLAY_HELTEC
    , m_display(cfg.addr, 400000, cfg.sda, cfg.scl, GEOMETRY_128_64, cfg.rst)
#endif
{}

bool Ssd1306DisplayHeltec::begin() {
#ifdef DISPLAY_HELTEC
    pinMode(m_cfg.rst, OUTPUT);
    digitalWrite(m_cfg.rst, LOW);
    delay(50);
    digitalWrite(m_cfg.rst, HIGH);
    delay(50);
    if (!m_display.init()) return false;
    m_display.displayOn();
    m_display.flipScreenVertically();
    return true;
#else
    return false;
#endif
}

void Ssd1306DisplayHeltec::clear() {
#ifdef DISPLAY_HELTEC
    m_display.clear();
#endif
}

void Ssd1306DisplayHeltec::set_cursor(int x, int y) {
    m_cx = x; m_cy = y;
}

void Ssd1306DisplayHeltec::set_text_size(int size) {
#ifdef DISPLAY_HELTEC
    switch (size) {
        case 1: m_display.setFont(ArialMT_Plain_10); break;
        case 2: m_display.setFont(ArialMT_Plain_16); break;
        default: m_display.setFont(ArialMT_Plain_10); break;
    }
#endif
}

void Ssd1306DisplayHeltec::print(const char* str) {
#ifdef DISPLAY_HELTEC
    m_display.drawString(m_cx, m_cy, str);
    m_cx += m_display.getStringWidth(str);
#endif
}

void Ssd1306DisplayHeltec::printf(const char* fmt, ...) {
#ifdef DISPLAY_HELTEC
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    print(buf);
#endif
}

void Ssd1306DisplayHeltec::display() {
#ifdef DISPLAY_HELTEC
    m_display.display();
#endif
}

int Ssd1306DisplayHeltec::width() const { return m_cfg.width; }
int Ssd1306DisplayHeltec::height() const { return m_cfg.height; }

#endif // DISPLAY_HELTEC
```

- [ ] **Step 3: Build test via hub_32_lora_heltec**

```bash
cd hub && pio run -e hub_32_lora_heltec
```

Expected: SUCCESS (must compile after platformio.ini flag change).

- [ ] **Step 4: Commit**

```bash
git -C shared add src/display_heltec.h src/display_heltec.cpp
git -C shared commit -m "feat(shared): add Ssd1306DisplayHeltec driver"
git add shared
git commit -m "feat(shared): add Ssd1306DisplayHeltec driver"
```

---

### Task 4: Refactor hub display_handler

**Files:**
- Modify: `hub/src/display_handler.h`
- Modify: `hub/src/display_handler.cpp`
- Modify: `hub/platformio.ini` (replace `HABILITA_DISPLAY` → `DISPLAY_TTGO` / `DISPLAY_HELTEC`)
- Delete: `hub/include/display_config.h`

**Interfaces:**
- Consumes: `Ssd1306DisplayTtgo`, `Ssd1306DisplayHeltec`, `PageManager`
- Produces: `display_handler_init()`, `display_handler_loop()` (same API, unchanged)

- [ ] **Step 1: Rewrite display_handler.h**

```cpp
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)
void display_handler_init(void);
void display_handler_loop(void);
#else
static inline void display_handler_init(void) {}
static inline void display_handler_loop(void) {}
#endif

#endif
```

- [ ] **Step 2: Rewrite display_handler.cpp**

Remove macro-based Heltec/TTGO code. Use the new classes:

```cpp
#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)

#include "display_handler.h"
#include "page_manager.h"
#include "config.h"
#include "sensor_registry.h"
#include "platform.h"
#include "common_console.h"
#include "mqtt_client.h"
#include "radio_manager.h"
#include "common_util.h"
#include <time.h>

extern RadioManager s_radio_mgr;

#if defined(DISPLAY_TTGO)
#include "display_ttgo.h"
static Ssd1306DisplayTtgo s_display;
#elif defined(DISPLAY_HELTEC)
#include "display_heltec.h"
static Ssd1306DisplayHeltec s_display;
#endif

static PageManager s_pages(&s_display);
static bool s_display_ok = false;
static int s_last_rssi = 0;
static char s_last_device_name[32] = "";
static char s_chip_id_str[16];
static char s_platform_str[16];

static void find_newest_device(void) {
    // same as current implementation — searches sensor_registry
    int newest_slot = -1;
    unsigned long newest_time = 0;
    for (int i = 0; i < MAX_VIRTUAL_SENSORS; i++) {
        virtual_sensor_t *s = sensor_registry_get(i);
        if (s && s->last_seen > newest_time) {
            newest_time = s->last_seen;
            newest_slot = i;
        }
    }
    if (newest_slot >= 0) {
        virtual_sensor_t *s = sensor_registry_get(newest_slot);
        if (s) {
            s_last_rssi = s->last_rssi;
            strncpy(s_last_device_name, s->name, sizeof(s_last_device_name) - 1);
            s_last_device_name[sizeof(s_last_device_name) - 1] = '\0';
        }
    }
}
```

Page render functions stay the same as current implementation but use `DisplayInterface&` parameter instead of global `display` object:

```cpp
static void render_page_0(DisplayInterface& d) {
    d.set_cursor(0, 0);
    d.print("WiFi: ");
    d.print(WiFi.localIP().toString().c_str());
    d.set_cursor(0, 12);
    d.printf("Ch:%d RSSI:%d", WiFi.channel(), WiFi.RSSI());
    d.set_cursor(0, 24);
    d.print("MQTT: ");
    d.print(mqtt_client_is_connected() ? "OK" : "---");
    d.set_cursor(0, 36);
    d.print("NTP: ");
    d.print(gateway_ntp_synced() ? "sync" : "wait");
}

static void render_page_1(DisplayInterface& d) {
    d.set_cursor(0, 0);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    d.printf("Devices: %d/%d", online, paired);
    d.set_cursor(0, 12);
    char uptime_buf[24];
    uptime_to_str(millis(), uptime_buf, sizeof(uptime_buf));
    d.printf("Up: %s", uptime_buf);
    d.set_cursor(0, 24);
    d.printf("RX: %lu ACK: %lu", s_radio_mgr.total_rx_count(), s_radio_mgr.total_ack_count());
    d.set_cursor(0, 36);
    if (strlen(s_last_device_name) > 0) {
        d.printf("Last: %s", s_last_device_name);
    }
}

static void render_page_2(DisplayInterface& d) {
    d.set_cursor(0, 0);
    d.printf("FW: %s", FW_VERSION);
    d.set_cursor(0, 12);
    int t = temperatureRead();
    if (t > 0 && t < 200) {
        d.printf("Temp: %d C", t);
    }
    d.set_cursor(0, 24);
    d.printf("WiFi RSSI: %d dBm", WiFi.RSSI());
    d.set_cursor(0, 36);
    d.printf("Dev RSSI: %d dBm", s_last_rssi);
}

static void render_footer(DisplayInterface& d) {
    d.set_cursor(0, 52);
    int paired = sensor_registry_count_paired();
    int online = sensor_registry_count_online();
    d.printf("D:%d/%d", online, paired);
    d.printf("   %ddBm", WiFi.RSSI());
    // clock
    if (gateway_ntp_synced()) {
        time_t now = gateway_ntp_epoch();
        struct tm *ti = localtime(&now);
        d.printf("       %02d:%02d", ti->tm_hour, ti->tm_min);
    } else {
        d.printf("       --:--");
    }
}
```

Init and loop:

```cpp
void display_handler_init(void) {
    if (!s_display.begin()) {
        console.printf("[Display] begin() FAIL\n");
        return;
    }
    s_display_ok = true;
    console.printf("[Display] begin() OK\n");

    s_display.set_text_size(1);
    s_display.clear();
    s_display.print("Booting...");
    s_display.display();

    snprintf(s_chip_id_str, sizeof(s_chip_id_str), "0x%06x", chip_id());
    snprintf(s_platform_str, sizeof(s_platform_str), "%s", PLATFORM_PREFIX);

    s_pages.add_page(render_page_0);
    s_pages.add_page(render_page_1);
    s_pages.add_page(render_page_2);
    s_pages.set_footer(render_footer);
}

void display_handler_loop(void) {
    if (!s_display_ok) return;
    find_newest_device();
    s_pages.loop();
}

#endif // DISPLAY_TTGO || DISPLAY_HELTEC
```

- [ ] **Step 3: Update hub/platformio.ini**

Replace `-D HABILITA_DISPLAY` with `-D DISPLAY_TTGO` (hub_32_lora) and `-D DISPLAY_HELTEC` (hub_32_lora_heltec).

- [ ] **Step 4: Remove hub/include/display_config.h**

```bash
git rm hub/include/display_config.h
```

- [ ] **Step 5: Build test both envs**

```bash
cd hub && pio run -e hub_32_lora
pio run -e hub_32_lora_heltec
```

Expected: both SUCCESS.

- [ ] **Step 6: Commit**

```bash
git add hub/src/display_handler.h hub/src/display_handler.cpp hub/platformio.ini
git rm hub/include/display_config.h
git commit -m "refactor(hub): use shared DisplayInterface + PageManager, replace HABILITA_DISPLAY"
```

---

### Task 5: Refactor onoff-lora display

**Files:**
- Create: `nodes/onoff-lora/src/display.h`
- Create: `nodes/onoff-lora/src/display.cpp`
- Modify: `nodes/onoff-lora/src/main.cpp` (remove inline display, call display_init/loop)
- Modify: `nodes/onoff-lora/platformio.ini` (add `-D DISPLAY_TTGO`)

- [ ] **Step 1: Create display.h**

```cpp
#ifndef ONOFF_LORA_DISPLAY_H
#define ONOFF_LORA_DISPLAY_H

void display_init();
void display_loop();

#endif
```

- [ ] **Step 2: Create display.cpp**

```cpp
#include "display.h"
#include "display_ttgo.h"
#include "page_manager.h"
#include "config.h"
#include "myWiFiManager.h"
#include <WiFi.h>
#include <Arduino.h>

extern bool s_relay;
extern uint8_t s_my_mac[6];
extern char s_device_id[32];
extern char s_device_name[32];

static Ssd1306DisplayTtgo s_display;
static PageManager s_pages(&s_display);

// We need access to the protocol for is_paired/last_rssi/rx_count/tx_count
// Declare it extern — shared global from main.cpp
class LoraNodeProtocol;
extern LoraNodeProtocol s_proto;

static void render_page_0(DisplayInterface& d) {
    d.set_cursor(0, 0);
    d.print("LoRa Switch");
    d.set_cursor(0, 12);
    d.print(s_relay ? "ON" : "OFF");
    d.set_cursor(0, 24);
    d.print(s_proto.is_paired() ? "Pareado" : "---");
    d.set_cursor(0, 36);
    d.printf("RSSI: %d", s_proto.last_rssi());
    d.set_cursor(0, 48);
    d.print(WiFi.localIP().toString().c_str());
}

static void render_page_1(DisplayInterface& d) {
    unsigned long sec = millis() / 1000;
    int dd = sec / 86400; sec %= 86400;
    int hh = sec / 3600; sec %= 3600;
    int mm = sec / 60; sec %= 60;
    d.set_cursor(0, 0);
    d.printf("TX: %lu", s_proto.tx_count());
    d.set_cursor(0, 12);
    d.printf("RX: %lu", s_proto.rx_count());
    d.set_cursor(0, 24);
    d.printf("Mem: %u", ESP.getFreeHeap());
    d.set_cursor(0, 36);
    d.printf("Uptime: %dd %02d:%02d:%02d", dd, hh, mm, (int)sec);
    d.set_cursor(0, 48);
    d.printf("WiFi: %s", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "---");
}

void display_init() {
    if (!s_display.begin()) return;
    s_display.set_text_size(1);
    s_display.clear();
    s_display.print("Booting...");
    s_display.display();
    s_pages.add_page(render_page_0);
    s_pages.add_page(render_page_1);
}

void display_loop() {
    s_pages.loop();
}
```

- [ ] **Step 3: Update main.cpp**

Remove inline display functions:
```cpp
// REMOVE these static functions:
// display_page0()
// display_page1()
// display_update()
// display_init()
// #define DISPLAY_SDA 21 ... DISPLAY_H 64
// static Adafruit_SSD1306 s_display(...)
```

Add include:
```cpp
#include "display.h"
```

Replace calls in `setup()`:
```cpp
// OLD: display_init();
// NEW:
display_init();
```

Replace calls in `loop()`:
```cpp
// Remove the inline display_update block:
// static unsigned long last_display = 0;
// if (millis() - last_display > 2000) { last_display = millis(); display_update(); }
//
// Add:
display_loop();
```

- [ ] **Step 4: Update platformio.ini**

Add to build_flags:
```ini
-D DISPLAY_TTGO
```

- [ ] **Step 5: Build test**

```bash
cd nodes/onoff-lora && pio run -e lora_esp32
```

Expected: SUCCESS.

- [ ] **Step 6: Commit**

```bash
git add nodes/onoff-lora/src/display.h nodes/onoff-lora/src/display.cpp nodes/onoff-lora/src/main.cpp nodes/onoff-lora/platformio.ini
git commit -m "refactor(onoff-lora): extract display to shared DisplayInterface + PageManager"
```

---

### Task 6: Integration verification

**Files:** (none — verification only)

- [ ] **Step 1: Run native unit tests**

```bash
cd tests/unit && pio test -e native
```

Expected: all tests PASS (including PageManager tests).

- [ ] **Step 2: Build all hub envs**

```bash
cd hub && pio run -e hub_32_lora && pio run -e hub_32_lora_heltec
```

Expected: both SUCCESS.

- [ ] **Step 3: Build onoff-lora**

```bash
cd nodes/onoff-lora && pio run -e lora_esp32
```

Expected: SUCCESS.

- [ ] **Step 4: Verify no HABILITA_DISPLAY remains**

```bash
git grep HABILITA_DISPLAY
```

Expected: no matches.

- [ ] **Step 5: Verify display_config.h removed**

```bash
git ls-files hub/include/display_config.h
```

Expected: no output (file removed).

- [ ] **Step 6: Commit any remaining fixes**
