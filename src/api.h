#pragma once

#include "Arduino.h"
#include "protocol.h"

class ApiDriver
{
private:
public:
    String productName = "local";
    String sensorType;
    int sensorIndex = -1;
    int pin;
    virtual void setup() {}
    virtual void loop() {}
    virtual bool isLoop() { return true; }
    virtual void beforeSetup() {}
    virtual void afterSetup() {}
    virtual void changed(const int pin, const long value) {}
    static Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }

#ifdef DEBUG_API
    String toString()
    {
        char buf[100];
        sprintf(buf, "{ 'pin': %i, 'sensorType': %s }", pin, sensorType.c_str());
        return String(buf);
    }
#endif
};

struct ApiDriverPair
{
    bool autoInit;
    String sensorType;
    ApiDriver *(*create)();
};

void registerApiDriver(const String sensorType, ApiDriver *(*create)(),const bool autoInit = false);
ApiDriver *createApiDriver(const String sensorType, const int pin);

class ApiDrivers
{
private:
public:
    void add(ApiDriver *driver);
    ApiDriver *initPinSensor(const int pin, const String sensorType);

    void loop();
    ApiDriver *findByPin(const int pin);
    ApiDriver *findByType(const String sensorType);
    void changed(const int pin, const long value);
    void afterSetup();
    int count();
    ApiDriver *getItem(int index);
};

ApiDrivers getApiDrivers();
