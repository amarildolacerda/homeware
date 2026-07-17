#ifndef HW_SHARED_OTA_H
#define HW_SHARED_OTA_H

#include <Arduino.h>

// OTA via ArduinoOTA (cross-platform ESP8266/ESP32).
// Loga no 'console' (de common_console.h) — inclua common_console.h antes.
void ota_setup(const char *hostname);
void ota_handle();   // chame em loop()

#endif
