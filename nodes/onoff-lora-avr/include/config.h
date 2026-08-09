#pragma once

// ── Device ──
#define DEVICE_NAME "LoRa AVR Switch"

// ── Pins ──
#ifndef LORA_RX_PIN
#define LORA_RX_PIN 2
#endif
#ifndef LORA_TX_PIN
#define LORA_TX_PIN 3
#endif
#ifndef LED_PIN
#define LED_PIN 4
#endif
#ifndef RELAY_PIN
#define RELAY_PIN 5
#endif
#ifndef BUTTON_PIN
#define BUTTON_PIN 6
#endif

// ── Relay ──
#ifndef RELAY_ON
#define RELAY_ON HIGH
#endif

// ── Serial ──
#ifndef SERIAL_BAUD
#define SERIAL_BAUD 115200
#endif
#ifndef LORA_BAUD
#define LORA_BAUD 9600
#endif

// ── LoRa ──
#ifndef LORA_FREQ
#define LORA_FREQ 868.0
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_BW
#define LORA_BW 125
#endif
#ifndef LORA_CR
#define LORA_CR 7
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 17
#endif

// ── Timing ──
#ifndef PAIR_INTERVAL_MS
#define PAIR_INTERVAL_MS 5000
#endif
#ifndef PAIR_MAX_ATTEMPTS
#define PAIR_MAX_ATTEMPTS 20
#endif
#ifndef STATE_UPDATE_INTERVAL_MS
#define STATE_UPDATE_INTERVAL_MS 60000
#endif
#ifndef HEARTBEAT_INTERVAL_MS
#define HEARTBEAT_INTERVAL_MS 60000
#endif
#ifndef LED_BLINK_PAIR_MS
#define LED_BLINK_PAIR_MS 250
#endif
#ifndef LED_BLINK_BOOT_MS
#define LED_BLINK_BOOT_MS 500
#endif
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 50
#endif

// ── EEPROM ──
#define EEPROM_SIZE 512
#define EEPROM_RELAY_STATE 200
#define EEPROM_PAIRED_FLAG 201
#define EEPROM_MY_MAC      202  // 6 bytes (202-207)
