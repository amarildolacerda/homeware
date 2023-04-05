#pragma once
#include "options.h"

#ifdef ARDUINO_AVR
// nada aqui para AVR
#else

#ifdef ESP32
#include <WiFi.h>
#include <WebServer.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#ifndef NO_WM
#include "homewareWiFiManager.h"
#endif

#include <ArduinoJson.h>

#include "functions.h"

#include "protocol.h"

void linha();
String IPAddressToString(const IPAddress x);

class Homeware : public Protocol
{
public:
#ifdef ESP32
    void setServer(WebServer *externalServer = nullptr);
#else
    void setServer(ESP8266WebServer *externalServer = nullptr);
#endif
    virtual String localIP();
#ifdef ESP32
    WebServer *server;
#else
    ESP8266WebServer *server;
#endif
#ifdef ESP32
    void setup(WebServer *externalServer = nullptr);
#else
    void setup(ESP8266WebServer *externalServer = nullptr);
#endif

    //===================== revisado para ficar aqui mesmo
    void afterChanged(const int pin, const int value, const String mode) override;
    void resetWiFi() override;
    void afterBegin() override;
    void reset() override;
    void prepare() override;

    //============================= potencial para mudar para Protocol

#ifdef ESP32
    const char *getChipId();
#else
    uint32_t getChipId();
#endif

private:
#ifndef BASIC
    void setupServer();
#endif
};

extern Homeware homeware;
#ifdef ESP32
extern WebServer server;
#else
extern ESP8266WebServer server;
#endif
#endif