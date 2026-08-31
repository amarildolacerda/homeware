#include "config_store.h"
#include "common_console.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// --- Init ---

void config_store_init() {
    if (!LittleFS.begin()) {
        console.println("[CONFIG] LittleFS mount failed, formatting...");
        LittleFS.format();
        LittleFS.begin();
    }
    console.println("[CONFIG] LittleFS mounted");
}

// --- Generic helpers ---

bool config_file_exists(const char *path) {
    return LittleFS.exists(path);
}

bool config_file_remove(const char *path) {
    return LittleFS.remove(path);
}

// --- Telegram Config ---

bool config_telegram_load(TelegramConfig *cfg) {
    // Defaults first
    config_telegram_load_defaults(cfg);

    if (!LittleFS.exists(CONFIG_FILE_TELEGRAM)) {
        console.println("[CONFIG] No telegram.json, using defaults");
        return false;
    }

    File f = LittleFS.open(CONFIG_FILE_TELEGRAM, "r");
    if (!f) {
        console.println("[CONFIG] Failed to open telegram.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        console.printf("[CONFIG] Failed to parse telegram.json: %s\n", err.c_str());
        return false;
    }

    cfg->enabled       = doc["enabled"] | false;

    const char *token  = doc["token"] | "";
    strncpy(cfg->token, token, sizeof(cfg->token) - 1);
    cfg->token[sizeof(cfg->token) - 1] = '\0';

    const char *chatid = doc["chat_id"] | "";
    strncpy(cfg->chat_id, chatid, sizeof(cfg->chat_id) - 1);
    cfg->chat_id[sizeof(cfg->chat_id) - 1] = '\0';

    cfg->poll_interval = doc["poll_interval"] | 2000;
    if (cfg->poll_interval < 1000)  cfg->poll_interval = 1000;
    if (cfg->poll_interval > 60000) cfg->poll_interval = 60000;

    cfg->alerts_level  = doc["alerts_level"]  | 0x0F;
    if (cfg->alerts_level > 0x0F) cfg->alerts_level = 0x0F;

    cfg->alerts_type   = doc["alerts_type"]   | 0x03FF;
    if (cfg->alerts_type > 0x03FF) cfg->alerts_type = 0x03FF;

    console.printf("[CONFIG] Telegram loaded: enabled=%d poll=%ums\n",
                   cfg->enabled, cfg->poll_interval);
    return true;
}

bool config_telegram_save(const TelegramConfig *cfg) {
    JsonDocument doc;
    doc["enabled"]       = cfg->enabled;
    doc["token"]         = cfg->token;
    doc["chat_id"]       = cfg->chat_id;
    doc["poll_interval"] = cfg->poll_interval;
    doc["alerts_level"]  = cfg->alerts_level;
    doc["alerts_type"]   = cfg->alerts_type;

    File f = LittleFS.open(CONFIG_FILE_TELEGRAM, "w");
    if (!f) {
        console.println("[CONFIG] Failed to write telegram.json");
        return false;
    }

    serializeJson(doc, f);
    f.close();

    console.println("[CONFIG] Telegram saved");
    return true;
}

bool config_telegram_load_defaults(TelegramConfig *cfg) {
    memset(cfg, 0, sizeof(TelegramConfig));
    cfg->enabled       = false;
    cfg->token[0]      = '\0';
    cfg->chat_id[0]    = '\0';
    cfg->poll_interval = 2000;
    cfg->alerts_level  = 0x0F;   // all levels enabled
    cfg->alerts_type   = 0x03FF; // all alert types enabled
    return true;
}
