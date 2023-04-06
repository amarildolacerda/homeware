#pragma once

#include <Arduino.h>
#include "protocol.h"

class ApiDriver
{
private:
public:
    String sensorType;
    String productName = "none";
    int sensorIndex = -1;
    int pin;
    virtual void setup() {}
    virtual void loop() {}
    virtual void afterCreate() {}
    virtual void afterSetup() {}
    virtual void changed(const int pin, const long value) {}
    virtual void changed(const String value){};

    static Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }
    virtual void reload(){};

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
    bool autoCreate;
    String sensorType;
    ApiDriver *(*create)();
};

void registerApiDriver(const String sensorType, ApiDriver *(*create)(), const bool autoInit = false);
ApiDriver *createApiDriver(const String sensorType, const int pin, const bool autoCreate = false);

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
    void afterCreate();
    int count();
    ApiDriver *getItem(int index);
    void reload();
};

ApiDrivers getApiDrivers();
