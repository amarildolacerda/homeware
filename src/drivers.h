
#ifndef drivers_h
#define drivers_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include "timer.h"

#include "protocol.h"
#include "functions.h"
class Driver
{
private:
protected:
    typedef void (*callbackFunction)(String mode, int pin, int value);
    long v1 = 0;
    callbackFunction triggerCallback;
    callbackFunction triggerOkState;
    String _mode;
    int _pin = -1;
    long timeout = 0;
    long interval = 0;
    float oldValue = 0;
public:
    /// @brief triggerEnabled true indica que o driver define o momento para dispara trigger
    bool triggerEnabled = false;

    bool active = false;
    virtual void setV1(long x) { v1 = x; }
    virtual void updateTimeout(const int pin)
    {
        // le timer count
        Protocol *prot = getInstanceOfProtocol();
        String sPin = (String)pin;
        if (prot->getTimers().containsKey(sPin))
        {
            timeout = (long)prot->getTimers()[sPin];
        }
        if (prot->getIntervals().containsKey(sPin))
        {
            interval = (long)prot->getIntervals()[sPin];
        }
    }
    float debug(const float value)
    {
        if (value != oldValue)
        {
            getProtocol()->debugf("{'action':'%s','pin':'%s','value':%g, 'at':'%s'}\r\n", _mode.c_str(), ((String)_pin).c_str(), value, timer.getNow().c_str());
            oldValue = value;
        }
        return value;
    }
    virtual void setPinMode(int pin)
    {
        active = true;
        updateTimeout(pin);
    }

    virtual void setup()
    {
#ifndef ARDUINO_AVR
        if (getProtocol()->resources.indexOf(_mode) < 0)
            getProtocol()->resources += _mode + ",";
#endif
    };
    virtual void loop(){};

    virtual int readPin()
    {
        return -1;
    }
    virtual int internalRead()
    {
        return readPin();
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
    virtual int writePin(const int value)
    {
        return value;
    }
    virtual bool isGet() { return true; }
    virtual bool isSet() { return false; }
    virtual String doCommand(const String command)
    {
        return "NAK";
    }
    virtual JsonObject readStatus()
    {
        int rsp = readPin();
        DynamicJsonDocument json = DynamicJsonDocument(128);
        json["result"] = rsp;
        return json.as<JsonObject>();
    }
    virtual bool isStatus() { return false; }
    virtual bool isCommand() { return false; }
    virtual bool isLoop() { return false; }

    virtual void changed(const int value){};
    virtual void beforeSetup() {}

    int getPin()
    {
        return _pin;
    };

    void setTriggerEvent(callbackFunction callback)
    {
        triggerCallback = callback;
    }
    void setTriggerOkState(callbackFunction callback)
    {
        triggerOkState = callback;
    }
};

/// -----------------------------------------------------
/// Registra os tipos de drivers disponiveis para uso
/// -----------------------------------------------------
struct ModeDriverPair
{
    String mode;
    Driver *(*create)();
};

void registerDriverMode(String mode, Driver *(*create)());
Driver *createByDriverMode(const String mode, const int pin);

// -----------------------------------------------
//  Coleção de Drivers
// -----------------------------------------------
class Drivers
{

public:
    Driver *items[32];
    int driversCount = 0;

    Driver *initPinMode(const String mode, const int pin);

    // template <class T>
    void add(Driver *driver);
    Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }

    void setup()
    {
        for (auto *drv : items)
        {
            if (drv)
                drv->setup();
        }
    }
    void deleteByPin(const int pin)
    {
        /*   int idx = indexOf(pin);
           if (idx > -1)
           {
               Driver *drv = items[idx];
               if (drv)
               {
   #ifdef DEBUG_ON
                   Serial.printf("removeu driver: %s em %i\r\n", drv->getMode(), pin);
   #endif
                   delete drv;
                   drv = NULL;
                   for (int i = idx; i < count - 1; i++)
                   {
                       items[i] = items[i + 1];
                   }
                   count--;
               }
           }
           */
    }
    int indexOf(const int pin)
    {
        int idx = 0;
        for (auto *drv : items)
        {
            if (drv)
            {
                if (drv->getPin() == pin)
                {
                    return idx;
                }
            }
            idx++;
        }
        return -1;
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
    Driver *findByPin(const int pin)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->getPin() == pin)
                {
                    return drv;
                }
            }
        return nullptr;
    }
    void changed(const int pin, const int value)
    {
        auto *drv = findByPin(pin);
        if (drv && drv->active)
            drv->changed(value);
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