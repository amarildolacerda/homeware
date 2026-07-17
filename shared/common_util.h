#ifndef HW_SHARED_UTIL_H
#define HW_SHARED_UTIL_H

#include <Arduino.h>

// Converte uptime (ms) para string "Xd Yh Zm Ws" (usado em dashboards/console).
void uptime_to_str(unsigned long ms, char *buf, size_t len);

// Versao String (Arduino).
void uptime_to_str(unsigned long ms, String &out);

#endif
