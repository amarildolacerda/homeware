#pragma once

#ifndef protocol_h
#define protocol_h

/*
  Protocol deve ficar isolado para ser utilizado um modelos diferentes de arduino,
  não pode conter coisas exclusivas de alguma placa em particular;

*/

#include <ArduinoJson.h>
#include "options.h"

#ifdef TELEGRAM
#include <telegram.h>
#endif

#ifdef TELNET
#include <ESPTelnet.h>
#endif

#ifndef ARDUINO_AVR
typedef void (*callbackDebugFunction)(String texto);
#endif
class Protocol
{
private:
public:
    // size_t driversCount = 0;
#ifndef ARDUINO_AVR

#ifdef SEM_USO
    bool connected = false;
    int ledPin = 255;
    bool inDebug = false;
    bool inTelnet = false;
#endif
    unsigned int ledTimeChanged = 5000;
    String resources = "";
    String apis = "";
#endif
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);
    String hostname = "AutoConnect";
    DynamicJsonDocument docPinValues = DynamicJsonDocument(256);
    unsigned int eventLoopMillis = millis();
    bool inited = false;

    void setup();
    virtual void prepare();
#ifdef TELNET
    ESPTelnet telnet;
#endif

    JsonObject getTrigger();
    JsonObject getStable();
    JsonObject getMode();

#ifdef SCENE
    JsonObject getScenes();
    void checkTriggerScene(const int pin, const int value);
    JsonObject getTriggerScenes();
#endif

#ifndef ARDUINO_AVR

#ifdef SENSORS
    JsonObject getDevices();
    JsonObject getSensors();
    JsonObject getDefaults();
#endif

#ifdef PWM
    virtual int writePWM(const int pin, const int value, const int timeout = 0);
#endif

#ifdef DEEPSLEEP    
    virtual void resetDeepSleep(const unsigned int t = 10);
#endif
    JsonObject getTimers();
    JsonObject getIntervals();

#endif

#ifdef SEM_USO
#endif
    virtual int findPinByMode(String mode);

    virtual String doCommand(String command);
    virtual String print(String msg);
    virtual int writePin(const int pin, const int value);
    virtual int readPin(const int pin, const String mode = "");
    virtual void debug(String txt);
    void debugf(const char *format, ...);
    void debugln(String txt);

    virtual int switchPin(const int pin);
    virtual String getPinMode(const int pin);
    virtual void loop();
    virtual bool pinValueChanged(const int pin, const int value, bool exectrigger = true);
    virtual void setupPins();
    String show();

#ifndef ARDUINO_AVR
    virtual void setLedMode(const int mode);
    virtual void reset();
    callbackDebugFunction debugCallback;
    static void driverCallbackEvent(String mode, int pin, int value);
    static void driverOkCallbackEvent(String mode, int pin, int value);
    void actionEvent(const char *txt);

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
    void setKeyIfNull(String key, String value)
    {
        if (!containsKey(key))
            config[key] = value;
    }

protected:
    // processos
    void
    initPinMode(int pin, const String m);
    int pinValue(const int pin);
    void checkTrigger(int pin, int value);
    void errorMsg(String msg);
#ifndef ARDUINO_AVR
    // eventos
    virtual void afterConfigChanged();
    virtual void afterChanged(const int pin, const int value, const String mode);
    virtual void afterBegin();
    virtual void afterSetup();
    virtual String localIP();
#ifdef DEEPSLEEP
    void doSleep(const int tempo);
#endif
#ifndef BASIC
    virtual void afterLoop();
    String getStatus();

#endif
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
};

Protocol *getInstanceOfProtocol();

//==================== end
#endif