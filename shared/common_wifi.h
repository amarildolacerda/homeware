#ifndef HW_SHARED_WIFI_H
#define HW_SHARED_WIFI_H

#include <Arduino.h>
#include "myWiFiManager.h"

// Delegators para o myWiFiManager (fonte unica de WiFi/captive portal).
// Mantido separado para clareza de responsabilidade.
//   common_wifi_begin -> mywifi_begin
//   common_wifi_loop  -> mywifi_loop
// O portal cativo com campo "Device Name" fica em mywifi_portal().
bool common_wifi_begin(bool force_portal);
void common_wifi_loop();

#endif
