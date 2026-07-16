#ifndef HW_SHARED_MY_WIFI_MANAGER_H
#define HW_SHARED_MY_WIFI_MANAGER_H

#include <Arduino.h>
#include "shared_config.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiManager.h>
#endif

// Wrapper cross-platform de WiFi: conecta (STA), aplica IP estatico se
// configurado, sobe portal cativo (ESP8266 via WiFiManager; ESP32 via
// captive_portal do projecto) e persiste credenciais em EEPROM.
// Objetivo: uma unica API para gateway e clients ESP8266/ESP32.

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_PORTAL,
} wifi_state_t;

// Deve ser chamado uma vez no setup. 'force_portal' abre o portal mesmo com
// credenciais salvas. Retorna true se conectou a um AP.
bool mywifi_begin(bool force_portal);

// Abre portal cativo (WiFiManager no ESP8266) com campo "Device Name".
// name_buf: buffer que recebe o nome digitado (ja pre-preenchido).
// on_name: callback chamado com o nome validado ao salvar.
// Retorna true se conectou/portal concluido.
bool mywifi_portal(char *name_buf, size_t name_size, void (*on_name)(const char*));

// Loop nao-bloqueante: mantem reconexao e expira o portal. Chame em loop().
void mywifi_loop();

wifi_state_t mywifi_state();

const char* mywifi_ssid();

// Deriva o MAC alt do ESP-NOW a partir do MAC WiFi (bit 1 do byte 0 invertido).
void mywifi_espnow_mac(uint8_t *out);

#endif
