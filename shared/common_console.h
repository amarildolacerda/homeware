#ifndef HW_SHARED_CONSOLE_H
#define HW_SHARED_CONSOLE_H

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

// Console telnet (cross-platform ESP8266/ESP32).
// Espelha saida Serial para um cliente telnet na porta 23.
class ConsoleOutput : public Print {
public:
    void begin();
    void loop();
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int telnet_available();
    int telnet_read();
    using Print::printf;
    void set_banner(const char *s) { strncpy(m_banner, s, sizeof(m_banner) - 1); m_banner[sizeof(m_banner)-1] = 0; }
private:
    WiFiServer m_server{23};
    WiFiClient m_client;
    char m_banner[48];
};

extern ConsoleOutput console;

#endif
