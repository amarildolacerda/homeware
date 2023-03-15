#pragma once

#ifndef protocol_h
#define protocol_h

/*
  Protocol deve ficar isolado para ser utilizado um modelos diferentes de arduino,
  não pode conter coisas exclusivas de alguma placa em particular;

*/

#include <ArduinoJson.h>
#include <options.h>

#ifdef TELEGRAM
#include "telegram.h"
#endif

#ifdef TELNET
#include "ESPTelnet.h"
#endif

#ifndef ARDUINO_AVR
typedef void (*callbackDebugFunction)(String texto);
#endif
class Protocol
{
private:
public:
#ifndef ARDUINO_AVR
    bool connected = false;
    int ledPin = 255;
    unsigned int ledTimeChanged = 5000;
    String resources = "";
    bool inDebug = false;
    bool inTelnet = false;
#endif
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);
    String hostname = "AutoConnect";
    DynamicJsonDocument docPinValues = DynamicJsonDocument(256);
    unsigned int eventLoopMillis = millis();
    bool inited = false;

    void setup();
    void prepare();
#ifdef TELNET
    ESPTelnet telnet;
#endif

    JsonObject getTrigger();
    JsonObject getStable();
    JsonObject getMode();
#ifndef ARDUINO_AVR
    JsonObject getDevices();
    JsonObject getSensors();
    JsonObject getDefaults();
    virtual void resetDeepSleep(const unsigned int t = 10);
#endif

    virtual String doCommand(String command);
    virtual String print(String msg);
    virtual int writePin(const int pin, const int value);
    virtual int writePWM(const int pin, const int value, const int timeout = 0);
    virtual int readPin(const int pin, const String mode = "");
    virtual int findPinByMode(String mode);
    virtual void debug(String txt);
    void debugf(const char *format, ...);

    void debugln(String txt);

    virtual int switchPin(const int pin);
    virtual String getPinMode(const int pin);
    virtual void loop();
    virtual bool pinValueChanged(const int pin, const int value, bool exectrigger = true);
    virtual void setupPins();
#ifndef ARDUINO_AVR
    virtual void setLedMode(const int mode);
    virtual void reset();
    callbackDebugFunction debugCallback;
    virtual void driverCallbackEvent(String mode, int pin, int value);
#endif
    String getKey(String name)
    {
        return config[name];
    }
    void setKey(String name, String value)
    {
        config[name] = value;
    }
    bool containsKey(String name)
    {
        return config.containsKey(name);
    }

protected:
    // processos
    void initPinMode(int pin, const String m);
    int pinValue(const int pin);
    void checkTrigger(int pin, int value);
    void errorMsg(String msg);
#ifndef ARDUINO_AVR
    // eventos
    virtual void afterConfigChanged();
    virtual void afterChanged(const int pin, const int value, const String mode);
    virtual void afterLoop();
    virtual void afterBegin();
    virtual void afterSetup();
    virtual String localIP();
    void doSleep(const int tempo);
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    virtual String help();
    virtual void resetWiFi();
    void printConfig();
    bool readFile(String filename, char *buffer, size_t maxLen);
    String showGpio();
    JsonObject getValues();
#endif

private:
    DynamicJsonDocument baseConfig();
    void begin();
#ifdef TELNET
    void setupTelnet();
#endif
    void eventLoop();
    String getStatus();
};

Protocol *getInstanceOfProtocol();

//==================== end
#endif