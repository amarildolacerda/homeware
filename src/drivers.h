
#ifndef drivers_h
#define drivers_h

#include <Arduino.h>
#include <ArduinoJson.h>

#if !(defined( BASIC) || defined(NO_TIMER))
#include "timer.h"
#endif

#ifdef AVR
#include "protocol_base.h"
#else
#include "protocol.h"
#endif

#include "functions.h"

typedef void (*callbackFunction)(String mode, int pin, int value);

class Driver
{
private:
protected:
    float oldValue = 0;

public:
    callbackFunction triggerCallback;
    callbackFunction triggerOkState;

    String _mode;
    int _pin = -1;
    unsigned long timeout = 0;
    unsigned long interval = 0;
    /// @brief triggerEnabled true indica que o driver define o momento para dispara trigger
    bool triggerEnabled = false;

    bool active = false;
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
    float actionEvent(const float value)
    {
        if (value != oldValue)
        {
            char buf[512];
#ifndef BASIC            
            sprintf(buf,
                    "{'action':'%s','pin':'%s','value':%g, 'at':'%s'}\r\n", _mode.c_str(), ((String)_pin).c_str(), value, timer.getNow().c_str());
#else
            sprintf(buf,
                    "{'action':'%s','pin':'%s','value':%g}\r\n", _mode.c_str(), ((String)_pin).c_str(), value);

#endif
            getProtocol()->actionEvent(buf);
            oldValue = value;
        }
        return value;
    }
    virtual void setPinMode(int pin)
    {
        _pin = pin;
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
#ifndef BASIC    
    virtual int internalRead()
    {
        return readPin();
    }
#endif    
    Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }
    virtual int writePin(const int value)
    {
        return value;
    }
#ifndef BASIC
    virtual JsonObject readStatus()
    {
        int rsp = readPin();
        DynamicJsonDocument json = DynamicJsonDocument(128);
        json["result"] = rsp;
        return json.as<JsonObject>();
    }
#endif
    virtual void changed(const int value){};
    virtual void beforeSetup() {}

    void setTriggerEvent(callbackFunction callback)
    {
        triggerCallback = callback;
    }
    void setTriggerOkState(callbackFunction callback)
    {
        triggerOkState = callback;
    }
    virtual bool isStatus(){ return false;}
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
    Driver *items[DRIVERS_SIZE];
    int driversCount = 0;

    Driver *initPinMode(const String mode, const int pin);
    void add(Driver *driver);
    void setup()
    {
        for (auto *drv : items)
        {
            if (drv)
                drv->setup();
        }
    }
    virtual bool isStatus() { return false; }
    int indexOf(const int pin)
    {
        int idx = 0;
        for (auto *drv : items)
        {
            if (drv)
            {
                if (drv->_pin == pin)
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
                if (drv->_mode.equals(mode))
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
                if (drv->_pin == pin)
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
            if (drv && drv->active)
            {
                drv->loop();
            }
    }
    void reload()
    {
        // TODO: separar um metodo para individualizar
        setup();
    }
};

Drivers *getDrivers();
#endif