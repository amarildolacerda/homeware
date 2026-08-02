# Display Interface — Shared Abstraction

## Goal

Extract display code from hub and onoff-lora into `shared/` as a clean abstract interface + reusable page manager, with per-driver concrete implementations.

## Architecture

```
shared/src/
├── display_interface.h       ← DisplayInterface (abstract class)
├── display_ttgo.h/.cpp       ← Ssd1306DisplayTtgo : DisplayInterface
├── display_heltec.h/.cpp     ← Ssd1306DisplayHeltec : DisplayInterface
└── page_manager.h/.cpp       ← PageManager

hub/src/
├── display_handler.h/.cpp    ← instancia driver + PageManager, 3 pages + footer

nodes/onoff-lora/src/
├── display.h/.cpp            ← instancia Ssd1306DisplayTtgo + PageManager, 2 pages
├── main.cpp                  ← display_init/loop extraído
```

## DisplayInterface

```cpp
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
```

## PageManager

```cpp
using page_render_t = void (*)(DisplayInterface&);

class PageManager {
public:
    PageManager(DisplayInterface* display);
    void set_page_interval(unsigned long ms);   // default 5000
    bool add_page(page_render_t render_fn);      // max 4
    void set_footer(page_render_t footer_fn);    // optional, rendered after every page
    void loop();                                  // call from main loop
private:
    DisplayInterface* m_display;
    unsigned long m_interval_ms = 5000;
    unsigned long m_last_switch = 0;
    uint8_t m_current = 0;
    uint8_t m_count = 0;
    static const uint8_t MAX_PAGES = 4;
    page_render_t m_pages[MAX_PAGES];
    page_render_t m_footer = nullptr;
};
```

- Non-blocking: `millis()` timer, no `delay()`
- `loop()` returns immediately if interval not reached
- Footer rendered after every page (hub status bar)

## Ssd1306DisplayTtgo

```cpp
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
    bool begin() override;    // display.begin(SSD1306_SWITCHCAPVCC, addr)
    void clear() override;    // clearDisplay()
    void set_cursor(int x, int y) override;
    void set_text_size(int size) override;
    void print(const char* str) override;
    void printf(const char* fmt, ...) override;
    void display() override;
    int width() const override;
    int height() const override;
private:
    Adafruit_SSD1306 m_display;
    Ssd1306TtgoConfig m_cfg;
};

// Compile guard: #ifdef DISPLAY_TTGO
```

## Ssd1306DisplayHeltec

```cpp
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
    bool begin() override;    // reset pulse + display.init()
    void clear() override;
    void set_cursor(int x, int y) override;
    void set_text_size(int size) override;
    void print(const char* str) override;
    void printf(const char* fmt, ...) override;
    void display() override;
    int width() const override;
    int height() const override;
private:
    SSD1306Wire m_display;
    Ssd1306HeltecConfig m_cfg;
    int m_cx = 0, m_cy = 0;    // cursor tracking for drawString API
};

// Compile guard: #ifdef DISPLAY_HELTEC
```

- `printf` uses `snprintf` → `drawString()` since Heltec API lacks `printf`
- Cursor tracking via `m_cx, m_cy` advances on each `print()`

## Build Flags

Replaces `HABILITA_DISPLAY` with:

| Flag | Used by |
|------|---------|
| `-D DISPLAY_TTGO` | hub_32_lora, lora_esp32 (onoff-lora) |
| `-D DISPLAY_HELTEC` | hub_32_lora_heltec |

## Wiring per Device

### Hub (`display_handler.cpp`)

```cpp
#if defined(DISPLAY_TTGO)
  static Ssd1306DisplayTtgo s_display;
#elif defined(DISPLAY_HELTEC)
  static Ssd1306DisplayHeltec s_display;
#endif
static PageManager s_pages(&s_display);

void display_handler_init() {
    if (!s_display.begin()) return;
    s_display.set_text_size(1);
    s_display.clear();
    s_display.print("Booting...");
    s_display.display();
    s_pages.add_page(render_page_0);  // WiFi/MQTT/NTP
    s_pages.add_page(render_page_1);  // Devices/Uptime/RX
    s_pages.add_page(render_page_2);  // FW/Temp/RSSI
    s_pages.set_footer(render_footer); // bottom bar
}
void display_handler_loop() { s_pages.loop(); }
```

- `render_page_X` and `render_footer` are static functions in display_handler.cpp referencing hub globals (sensor_registry, s_radio_mgr, mqtt_client, etc.)
- `display_config.h` removed — pins supplied by each driver's default config

### onoff-lora (`display.h/.cpp`)

```cpp
#include "display_ttgo.h"
#include "page_manager.h"

void display_init();
void display_loop();
```

Implementation creates `Ssd1306DisplayTtgo`, PageManager with 2 pages:
- Page 0: name, relay, pairing, RSSI, IP
- Page 1: TX/RX, mem, uptime, WiFi
- No footer

## Removals

- `hub/include/display_config.h` — pins now in each driver's config struct
- `hub/src/display_handler.cpp` — rewritten from macro-based to clean delegation
- `nodes/onoff-lora/src/main.cpp` — inline display functions removed (display_page0, display_page1, display_update, display_init)
