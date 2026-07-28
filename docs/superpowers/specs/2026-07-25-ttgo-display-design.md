# TTGO LORA32 Display Handler (SSD1306)

## Goal

Add SSD1306 OLED display support to the hub for TTGO LORA32 T3_v1.6, controlled by `HABILITA_DISPLAY_TTGO` compile flag.

## Architecture

`display_handler` is a standalone module in `hub/src/`, guarded by `#ifdef HABILITA_DISPLAY_TTGO`. Zero impact on `hub_32` env. Same pattern as `lora_handler`.

## Pinout (T3_v1.6)

- SDA = 21
- SCL = 22
- RST = -1 (not used, tied to EN via 4k7 on T3_v1.6)
- I2C address = 0x3C
- NOT use GPIO4/15 — GPIO15 é SX1278 DIO1 (conflita com I2C SCL)
- GPIO16 é PSRAM CS no ESP32-WROVER — nunca usar como RST do OLED

## Display Content

3 pages, auto-cycle every 5 seconds via `millis()` non-blocking:

| Page | Content |
|------|---------|
| 1 | IP, Paired/Online, Uptime |
| 2 | FW version, Chip ID, Platform |
| 3 | Last device RSSI, ESP32 internal temp |

## Dependencies (only in `hub_32_lora` env)

- `adafruit/Adafruit SSD1306 @ ^2.5`
- `adafruit/Adafruit GFX Library @ ^1.11`

## Files

- `hub/include/display_config.h` — I2C pin defines
- `hub/src/display_handler.h` — declarations
- `hub/src/display_handler.cpp` — init + render loop
- `hub/src/main.cpp` — `#ifdef HABILITA_DISPLAY_TTGO` hooks
- `hub/platformio.ini` — flag + lib_deps in `hub_32_lora`

## Non-blocking

- No `delay()` — display update timer via `millis()`
- Page cycle timer via `millis()`
- `display_handler_loop()` returns immediately if no update due
