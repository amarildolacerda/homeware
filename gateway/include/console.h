#pragma once
#include <Arduino.h>
#include "platform.h"

class ConsoleOutput : public Print {
public:
    void begin();
    void loop();
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int telnet_available();
    int telnet_read();
    using Print::printf;
private:
    WiFiServer m_server{23};
    WiFiClient m_client;
};

extern ConsoleOutput console;
