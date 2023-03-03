#pragma once

// #ifndef protocol_def
// #define protocol_def

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
    int ledPin = 255;
    bool inDebug = false;
    bool inTelnet = false;
    String resources = "";

    void setup();

#ifdef TELNET
    ESPTelnet telnet;
#endif

    void reset();
    JsonObject getTrigger();
    JsonObject getStable();
    JsonObject getMode();
    JsonObject getDevices();
    JsonObject getSensors();
    JsonObject getDefaults();
    void initPinMode(int pin, const String m);
    int writePin(const int pin, const int value);
    int writePWM(const int pin, const int value, const int timeout = 0);
    int readPin(const int pin, const String mode = "");
    bool pinValueChanged(const int pin, const int value);
    int pinValue(const int pin);
    void afterChanged(const int pin, const int value, const String mode);
    void debug(String txt);
    int ledLoop(const int pin);
    int findPinByMode(String mode);
    String print(String msg);
    int getAdcState(int pin);
    String getPinMode(const int pin);
    void checkTrigger(int pin, int value);
    int switchPin(const int pin);
    void setLedMode(const int mode);
    String getStatus();
    String showGpio();
    void setupPins();
    void doSleep(const int tempo);
    void resetDeepSleep(const unsigned int t = 60000);
    String help();
    bool readFile(String filename, char *buffer, size_t maxLen);
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    String doCommand(String command);
    void errorMsg(String msg);
    String localIP();
    void resetWiFi();
    void printConfig();
    void setupTelnet();
    void loop();
    JsonObject getValues();

#ifdef DHT_SENSOR
    JsonObject readDht(const int pin);
#endif

private:
    DynamicJsonDocument baseConfig();
#ifdef TELNET
#endif
};
// #endif
