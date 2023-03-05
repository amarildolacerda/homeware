#pragma once

#ifndef protocol_h
#define protocol_h

/*
  Protocol deve ficar isolado para ser utilizado um modelos diferentes de arduino,
  não pode conter coisas exclusivas de alguma placa em particular;

*/

#include <ArduinoJson.h>
#include <options.h>
#include <ArduinoJson.h>

#ifdef TELNET
#include "ESPTelnet.h"
#endif

class Protocol
{
private:
public:
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);
    String hostname = "AutoConnect";
    unsigned int currentAdcState = 0;
    DynamicJsonDocument docPinValues = DynamicJsonDocument(256);
    unsigned int ledTimeChanged = 5000;
    unsigned int loopEventMillis = millis();
    bool inited = false;
    bool connected = false;

    int ledPin = 255;
    bool inDebug = false;
    bool inTelnet = false;
    String resources = "";

    void setup();
#ifdef TELNET
    ESPTelnet telnet;
#endif

    JsonObject getTrigger();
    JsonObject getStable();
    JsonObject getMode();
    JsonObject getDevices();
    JsonObject getSensors();
    JsonObject getDefaults();

    virtual void resetDeepSleep(const unsigned int t = 60000);
    virtual String doCommand(String command);
    virtual String print(String msg);
    virtual void setLedMode(const int mode);
    virtual int writePin(const int pin, const int value);
    virtual int writePWM(const int pin, const int value, const int timeout = 0);
    virtual int readPin(const int pin, const String mode = "");
    virtual int findPinByMode(String mode);
    virtual void debug(String txt);
    virtual int switchPin(const int pin);
    virtual String getPinMode(const int pin);
    virtual void loop();
    virtual bool pinValueChanged(const int pin, const int value);

protected:
    // eventos
    virtual void afterChanged(const int pin, const int value, const String mode);
    virtual void afterLoop();
    virtual void afterBegin();
    virtual void afterSetup();
    virtual void afterConfigChanged();
    // processos
    void initPinMode(int pin, const String m);
    int pinValue(const int pin);
    void checkTrigger(int pin, int value);
    void doSleep(const int tempo);
    virtual String help();
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    void errorMsg(String msg);
    virtual String localIP();
    virtual void resetWiFi();
    void printConfig();

    JsonObject getValues();
    virtual void reset();
    virtual void setupPins();
    bool readFile(String filename, char *buffer, size_t maxLen);

private:
    DynamicJsonDocument baseConfig();
    void begin();
#ifdef TELNET
    void setupTelnet();
#endif
    void loopEvent();
    String getStatus();
    String showGpio();
};

Protocol *getInstanceOfProtocol();

//==================== end
#endif