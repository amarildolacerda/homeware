#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "timer.h"

static timer_config_t s_timers[MAX_TIMERS];
static uint8_t s_last_fired_minute[MAX_TIMERS]; // track per-timer to avoid re-fire
static bool s_timers_loaded = false;

bool timer_init() {
    memset(s_timers, 0, sizeof(s_timers));
    memset(s_last_fired_minute, 0xFF, sizeof(s_last_fired_minute));
    timer_load();
    s_timers_loaded = true;
    return true;
}

void timer_load() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < MAX_TIMERS; i++) {
        int addr = EEPROM_TIMER_BASE + i * sizeof(timer_config_t);
        uint8_t *dst = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            dst[j] = EEPROM.read(addr + j);
    }
    EEPROM.end();
}

void timer_save() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < MAX_TIMERS; i++) {
        int addr = EEPROM_TIMER_BASE + i * sizeof(timer_config_t);
        uint8_t *src = (uint8_t*)&s_timers[i];
        for (size_t j = 0; j < sizeof(timer_config_t); j++)
            EEPROM.write(addr + j, src[j]);
    }
    EEPROM.commit();
    EEPROM.end();
}

bool timer_get(int index, timer_config_t *out) {
    if (index < 0 || index >= MAX_TIMERS) return false;
    if (out) *out = s_timers[index];
    return true;
}

bool timer_set(int index, const timer_config_t *cfg) {
    if (index < 0 || index >= MAX_TIMERS || !cfg) return false;
    s_timers[index] = *cfg;
    s_last_fired_minute[index] = 0xFF;
    return true;
}

int8_t timer_check(unsigned long current_epoch, int timezone_offset) {
    if (!s_timers_loaded) return -1;
    if (current_epoch < 100000) return -1; // time not synced

    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday; // 0=Sun
    uint8_t now_minute_id = now_hour * 60 + now_min;

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].enabled) continue;

        uint8_t timer_minute_id = s_timers[i].hour * 60 + s_timers[i].minute;
        if (timer_minute_id != now_minute_id) continue;
        if (s_last_fired_minute[i] == now_minute_id) continue;

        // check days_mask: if 0, run daily; else check bit
        if (s_timers[i].days_mask != 0) {
            if (!(s_timers[i].days_mask & (1 << now_wday))) continue;
        }

        s_last_fired_minute[i] = now_minute_id;
        return (int8_t)s_timers[i].action;
    }
    return -1;
}

void timer_get_next(unsigned long current_epoch, int timezone_offset,
                    unsigned long *next_epoch, uint8_t *next_action) {
    *next_epoch = 0;
    *next_action = 0;
    if (!s_timers_loaded || current_epoch < 100000) return;

    time_t lt_epoch = (time_t)(current_epoch + (timezone_offset * 3600));
    struct tm *lt = localtime(&lt_epoch);
    uint8_t now_hour = lt->tm_hour;
    uint8_t now_min = lt->tm_min;
    uint8_t now_wday = lt->tm_wday;
    uint16_t now_slot = now_hour * 60 + now_min;

    unsigned long best_epoch = 0;
    uint8_t best_action = 0;

    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].enabled) continue;
        uint16_t timer_slot = s_timers[i].hour * 60 + s_timers[i].minute;

        // check days_mask
        bool day_ok = (s_timers[i].days_mask == 0);
        if (!day_ok) {
            for (int d = 0; d < 7; d++) {
                if (s_timers[i].days_mask & (1 << d)) { day_ok = true; break; }
            }
        }
        if (!day_ok) continue;

        // Simple: find next timer today (or earliest tomorrow)
        unsigned long candidate = current_epoch;
        if (timer_slot <= now_slot) {
            // next occurrence is tomorrow (or later this week)
            candidate += 86400; // next day
        }
        candidate += (timer_slot - now_slot) * 60;
        candidate -= timezone_offset * 3600;

        if (best_epoch == 0 || candidate < best_epoch) {
            best_epoch = candidate;
            best_action = s_timers[i].action;
        }
    }

    *next_epoch = best_epoch;
    *next_action = best_action;
}

void timer_to_json(JsonDocument &doc) {
    JsonArray arr = doc["timers"].to<JsonArray>();
    for (int i = 0; i < MAX_TIMERS; i++) {
        JsonObject t = arr.add<JsonObject>();
        t["hour"] = s_timers[i].hour;
        t["minute"] = s_timers[i].minute;
        t["action"] = s_timers[i].action;
        t["days_mask"] = s_timers[i].days_mask;
        t["enabled"] = s_timers[i].enabled;
    }
}

bool timer_from_json(JsonDocument &doc) {
    if (!doc.containsKey("timers")) return false;
    JsonArray arr = doc["timers"].as<JsonArray>();
    int count = arr.size();
    if (count > MAX_TIMERS) count = MAX_TIMERS;
    for (int i = 0; i < count; i++) {
        JsonObject t = arr[i];
        s_timers[i].hour = t["hour"] | 0;
        s_timers[i].minute = t["minute"] | 0;
        s_timers[i].action = t["action"] | 0;
        s_timers[i].days_mask = t["days_mask"] | 0;
        s_timers[i].enabled = t["enabled"] | false;
    }
    return true;
}
