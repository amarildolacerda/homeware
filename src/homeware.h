#pragma once

#ifdef ESP32
#include "WiFi.h"
#include "WebServer.h"
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include "homewareWiFiManager.h"
#include <options.h>

#include <ArduinoJson.h>

#ifdef ALEXA
#include <Espalexa.h>
#endif

#include <functions.h>

#include <protocol.h>


void linha();

class Homeware : public Protocol
{
public:
#ifdef ESP32
    void setServer(WebServer *externalServer = nullptr);
#else
    void setServer(ESP8266WebServer *externalServer = nullptr);
#endif
    virtual String localIP();
#ifdef ALEXA
    Espalexa alexa = Espalexa();
#endif
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
    virtual int readPin(const int pin, const String mode = "");
    virtual void afterChanged(const int pin, const int value, const String mode);
    virtual String doCommand(String command);
    virtual void resetWiFi();
    virtual void afterLoop();
    virtual void afterBegin();
    virtual void reset();

    //============================= potencial para mudar para Protocol

#ifdef ESP32
    const char *getChipId();
#else
    uint32_t getChipId();
#endif

private:
    void setupServer();
#ifdef ALEXA
    void setupSensores();
#endif
};

extern Homeware homeware;
#ifdef ESP32
extern WebServer server;
#else
extern ESP8266WebServer server;
#endif
