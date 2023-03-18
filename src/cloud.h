#pragma once

#include "Arduino.h"
#include "protocol.h"

class CloudDriver
{
private:
public:
    String sensorType;
    int pin;
    virtual void setup() {}
    virtual void loop() {}
    virtual bool isLoop() { return false; }
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

struct CloudDriverPair
{
    String sensorType;
    CloudDriver *(*create)();
};

void registerCloudDriver(String sensorType, CloudDriver *(*create)());
CloudDriver *createCloudDriver(const String sensorType, const int pin);

class CloudDrivers
{
private:
    CloudDriver *items[32];

public:
    void add(CloudDriver *driver);

    CloudDriver *initPinSensor(const int pin, const String sensorType)
    {
        CloudDriver *drv = createCloudDriver(sensorType, pin);
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
    CloudDriver *findByPin(const int pin)
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
    CloudDriver *findByType(const String sensorType)
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

CloudDrivers getCloudDrivers();
