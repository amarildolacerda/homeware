#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <Arduino.h>

void log_buffer_init();
void log_add(const char *level, const char *fmt, ...);
const char* log_get_json();

#endif
