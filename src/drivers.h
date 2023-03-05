
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
        Serial.print(getMode());
        Serial.println(" - abstract Driver setup()");
    };
    virtual void loop()
    {
        Serial.print(getMode());
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
    Driver *items[32];
    int count = 0;
    int next = 0;

public:
    void add(Driver *driver)
    {
        items[count++] = driver;
    }

    void setup()
    {
        for (int i = 0; i < count; i++)
        {
            Driver *drv = items[i];
            if (drv)
                drv->setup();
        }
    }
    Driver *findByMode(String mode)
    {
        for (size_t i = 0; i < count; i++)
            if (items[i])
            {

                Driver *drv = items[i];
                if (drv->getMode().equals(mode))
                {
                    Serial.println("achou: " + mode);
                    return drv;
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
        /* for (size_t i = 0; i < count; i++)
             if (items[i])
             {
                 Driver *drv = items[i];
                 drv->loop();
             }
             */
    }
};

Drivers *getDrivers();
#endif