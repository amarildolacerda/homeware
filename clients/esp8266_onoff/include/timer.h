#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <ArduinoJson.h>

#define MAX_TIMERS 6

typedef struct __attribute__((packed)) {
    uint8_t hour;
    uint8_t minute;
    uint8_t action;       // 0=OFF, 1=ON
    uint8_t days_mask;    // bit0=Sun...bit6=Sat, 0=all
    bool enabled;
} timer_config_t;

bool timer_init();
void timer_load();
void timer_save();
bool timer_get(int index, timer_config_t *out);
bool timer_set(int index, const timer_config_t *cfg);
int8_t timer_check(unsigned long current_epoch, int timezone_offset);
void timer_get_next(unsigned long current_epoch, int timezone_offset,
                    unsigned long *next_epoch, uint8_t *next_action);
void timer_to_json(JsonDocument &doc);
bool timer_from_json(JsonDocument &doc);

#endif
