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
    virtual bool isLoop() { return false; }
    virtual void beforeSetup() {}
    virtual void afterSetup() {}
    virtual void changed(const int pin, const long value) {}
    static Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }

#ifdef DEBUG_ON
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
    String sensorType;
    ApiDriver *(*create)();
};

void registerApiDriver(String sensorType, ApiDriver *(*create)());
ApiDriver *createApiDriver(const String sensorType, const int pin);

class ApiDrivers
{

public:
    ApiDriver *items[32];
    void add(ApiDriver *driver);

    ApiDriver *initPinSensor(const int pin, const String sensorType)
    {
        ApiDriver *drv = createApiDriver(sensorType, pin);
        if (drv)
        {
            drv->setup();
#ifdef DEBUG_ON
            Serial.println(drv->toString());
#endif
            add(drv);
#ifdef DEBUG_ON
            Serial.printf("Criou link cloud: %s em %i \r\n", sensorType, pin);
#endif
            return drv;
        }
        return nullptr;
    }

    void loop()
    {
        for (auto *drv : items)
            if (drv && drv->isLoop())
            {
                drv->loop();
            }
    }
    ApiDriver *findByPin(const int pin)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->pin == pin)
                {
                    return drv;
                }
            }
        return nullptr;
    }
    ApiDriver *findByType(const String sensorType)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->sensorType == sensorType)
                {
                    return drv;
                }
            }
        return nullptr;
    }

    void changed(const int pin, const long value)
    {
        auto *drv = findByPin(pin);
        if (drv)
            drv->changed(pin, value);
    }

    void afterSetup()
    {
        for (auto *drv : items)
            if (drv)
            {
                drv->afterSetup();
            }
    }
};

ApiDrivers getApiDrivers();
