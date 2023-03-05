
#ifndef drivers_h
#define drivers_h

#include <Arduino.h>
#include <ArduinoJson.h>

#include "protocol.h"
#include "functions.h"
class Driver
{
private:
    String _mode;
    int _pin;

protected:
    typedef void (*callbackFunction)(String mode, int pin, int value);
    callbackFunction triggerCallback;

public:
    bool triggerEnabled = false ;
    bool active = false;
    long v1 = 0;
    void setV1(long x) { v1 = x; }
    virtual void setPinMode(int pin)
    {
        active = true;
    }

    virtual void setup()
    {
        getProtocol()->resources += _mode + ",";
    };
    virtual void loop(){};

    virtual int readPin(const int pin)
    {
        return -1;
    }
    void setMode(String md)
    {
        _mode = md;
    }
    String getMode()
    {
        return _mode;
    }
    Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }
    void setPin(const int pin)
    {
        _pin = pin;
    }
    virtual int writePin(const int pin, const int value)
    {
        return value;
    }
    virtual bool isGet() { return true; }
    virtual bool isSet() { return false; }
    virtual String doCommand(const String command)
    {
        return "NAK";
    }
    virtual JsonObject readStatus(const int pin)
    {
        int rsp = readPin(pin);
        DynamicJsonDocument json = DynamicJsonDocument(128);
        json["result"] = rsp;
        return json.as<JsonObject>();
    }
    virtual bool isStatus() { return false; }
    virtual bool isCommand() { return false; }
    virtual bool isLoop() { return false; }

    virtual void changed(const int pin, const int value){};

    int getPin()
    {
        return _pin;
    };

    void setTriggerEvent(callbackFunction callback)
    {
        triggerCallback = callback;
    }
};

class Drivers
{

private:
    int count = 0;
    int next = 0;

public:
    Driver *items[32];
    template <class T>
    void add(T *driver)
    {
        items[count++] = driver;
    }

    void setup()
    {
        for (auto *drv : items)
        {
            if (drv)
                drv->setup();
        }
    }
    Driver *findByMode(String mode)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->getMode().equals(mode))
                {
                    return drv;
                }
            }
        return nullptr;
    }
    void changed(const int pin, const int value)
    {

        for (auto *drv : items)
            if (drv && drv->active)
            {
                if (drv->getPin() == pin)
                    drv->changed(pin, value);
            }
    }
    void loop()
    {
        for (auto *drv : items)
            if (drv && drv->active && drv->isLoop())
            {
                drv->loop();
            }
    }
};

Drivers *getDrivers();
#endif