
#ifndef drivers_h
#define drivers_h

#include <Arduino.h>
#include <ArduinoJson.h>

#include "protocol.h"

class Driver
{
private:
    String _mode;
    int _pin;

public:
    virtual void setup()
    {
        Serial.print(_mode);
        Serial.println(" - abstract Driver setup()");
    };
    virtual void loop()
    {
        Serial.print(_mode);
        Serial.println(" - abstract Driver loop()");
    };

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

        return DynamicJsonDocument(10).as<JsonObject>();
    }
    virtual bool isStatus() { return false; }
    virtual bool isCommand() { return false; }

    virtual void changed(const int pin, const int value)
    {
        Serial.print(_mode);
        Serial.println(" - abstract Driver changed()");
    };

    int getPin()
    {
        return _pin;
    };
};

class Drivers
{

private:
    static Drivers *instance;
    Driver *items[32];
    int count = 0;

public:
    Drivers()
    {
    }
    /* static Drivers *getInstance()
     {
         if (instance == nullptr)
         {
             instance = new Drivers();
         }
         return instance;
     }
     */
    void add(Driver *driver)
    {
        items[count++] = driver;
    }

    void setup()
    {
        for (int i = 0; i < count; i++)
        {
            items[i]->setup();
        }
    }
    Driver *findByMode(String mode)
    {
        for (size_t i = 0; i < count; i++)
            if (items[i])
            {

                Driver *drv = items[i];
                Serial.println(drv->getMode());
                if (drv->getMode().equals(mode))
                {
                    Serial.println("achou: " + mode);
                    return items[i];
                }
            }
            else
            {
                Serial.print(i);
                Serial.println(". Driver item is NULL");
            }
        return NULL;
    }
    void changed(const int pin, const int value)
    {
        for (size_t i = 0; i < count; i++)
        {
            Driver *drv = items[i];
            if (drv)
            {
                if (drv->getPin() == pin)
                    drv->changed(pin, value);
            }
        }
    }
    void loop()
    {
        for (size_t i = 0; i < count; i++)
            if (items[i])
            {
                Driver *drv = items[i];
                drv->loop();
            }
    }
};

Drivers *getDrivers();
#endif