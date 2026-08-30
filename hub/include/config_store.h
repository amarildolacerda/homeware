#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>

// Config file paths on LittleFS
#define CONFIG_FILE_HUB     "/hub_config.json"
#define CONFIG_FILE_TELEGRAM "/telegram.json"

// --- Telegram Config ---
struct TelegramConfig {
    bool     enabled;
    char     token[64];
    char     chat_id[20];
    uint32_t poll_interval;   // ms
    uint8_t  alerts_level;    // bitmask: bit0=critical, bit1=alert, bit2=warning, bit3=info
    uint16_t alerts_type;     // bitmask: bit0=gas, bit1=smoke, bit2=offline, bit3=reconnect, ...
};

// Initialize LittleFS and load config
void config_store_init();

// Telegram config
bool config_telegram_load(TelegramConfig *cfg);
bool config_telegram_save(const TelegramConfig *cfg);
bool config_telegram_load_defaults(TelegramConfig *cfg);

// Generic helpers
bool config_file_exists(const char *path);
bool config_file_remove(const char *path);

#endif
