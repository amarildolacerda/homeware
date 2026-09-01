#ifndef HUB_CONFIG_STORE_H
#define HUB_CONFIG_STORE_H

// Include shared config_store (SensorSlot, config_sensors_*, config_mqtt_*, etc.)
#include "../../shared/src/config_store.h"

// --- Telegram Config (hub-only) ---
#define CONFIG_FILE_TELEGRAM "/telegram.json"

struct TelegramConfig {
    bool     enabled;
    char     token[64];
    char     chat_id[20];
    uint32_t poll_interval;   // ms
    uint8_t  alerts_level;    // bitmask: bit0=critical, bit1=alert, bit2=warning, bit3=info
    uint16_t alerts_type;     // bitmask: bit0=gas, bit1=smoke, bit2=offline, bit3=reconnect, ...
};

// Telegram config
bool config_telegram_load(TelegramConfig *cfg);
bool config_telegram_save(const TelegramConfig *cfg);
bool config_telegram_load_defaults(TelegramConfig *cfg);

#endif
