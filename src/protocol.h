

#ifndef protocol_def
#define protocol_def

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
    unsigned int ledPin = 255;

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
};
#endif
