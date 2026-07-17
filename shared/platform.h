#ifndef PLATFORM_H
#define PLATFORM_H

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <esp_now.h>
  #define PLATFORM_PREFIX "esp32"
  typedef WebServer MyWebServer;

  static inline uint32_t chip_id() {
      return (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  }

  static inline bool espnow_add_peer_wrapper(const uint8_t *mac, int channel) {
      esp_now_del_peer((uint8_t*)mac);
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, mac, 6);
      peer.channel = channel;
      peer.ifidx = WIFI_IF_STA;
      peer.encrypt = false;
      return esp_now_add_peer(&peer) == ESP_OK;
  }
#else
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <espnow.h>
  #define PLATFORM_PREFIX "esp8266"
  typedef ESP8266WebServer MyWebServer;

  static inline uint32_t chip_id() {
      return ESP.getChipId();
  }

  static inline bool espnow_add_peer_wrapper(const uint8_t *mac, int channel) {
      esp_now_del_peer((uint8_t*)mac);
      int ret = esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, channel, NULL, 0);
      return ret == 0;
  }
#endif

#endif
