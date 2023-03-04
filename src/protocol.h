#pragma once

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

#ifdef DHT_SENSOR
    JsonObject readDht(const int pin);
#endif
    void resetDeepSleep(const unsigned int t = 60000);
    String doCommand(String command);
    String print(String msg);
    void setLedMode(const int mode);
    int writePin(const int pin, const int value);
    int writePWM(const int pin, const int value, const int timeout = 0);
    int readPin(const int pin, const String mode = "");
    int findPinByMode(String mode);
    void debug(String txt);
    int switchPin(const int pin);
    String getPinMode(const int pin);
    void loop();
    bool pinValueChanged(const int pin, const int value);

protected:
    // eventos
    void afterChanged(const int pin, const int value, const String mode);
    void afterLoop();
    void afterBegin();
    void afterSetup();
    void afterConfigChanged();
    // processos
    void initPinMode(int pin, const String m);
    int pinValue(const int pin);
    int ledLoop(const int pin);
    int getAdcState(int pin);
    void checkTrigger(int pin, int value);
    void doSleep(const int tempo);
    String help();
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    void errorMsg(String msg);
    String localIP();
    void resetWiFi();
    void printConfig();

    JsonObject getValues();
    void reset();
    void setupPins();
    bool readFile(String filename, char *buffer, size_t maxLen);
    static Protocol *instance;

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

class Driver
{
public:
    String mode = "out";
    int pin = -1;
    Protocol *getProtocol();
    void setup();
    void loop();
    void changed(const int pin, const int value);
    int read(const int pin);
    int write(const int pin, const int value);

private:
};

class Drivers
{
public:
    Protocol *getProtocol();
    Driver items[32] ;
    void loop();
    void setup();
    int add(Driver item);
    void changed(const int pin, const int value);
    Driver *findByMode(String mode);
private:
    size_t length = 0;
};

Drivers getDrivers();
