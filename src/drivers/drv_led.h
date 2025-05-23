#pragma once

#include <protocol.h>

class AlternateDriver : public Driver
{
protected:
    int curPin = -1;
    bool curStatus = false;
    unsigned long ultimoChanged = 0;
    bool stateOn = true;
    unsigned long v1 = 0;

public:
    AlternateDriver()
    {
        v1 = 5000;
        timeout = 60000;
        interval = 5000;
    }

    virtual void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
#ifdef DEBUG_API
        Serial.println("AlternateDriver set OUTPUT mode");
#endif
        Driver::setPinMode(pin);
    }
    virtual int readPin() override
    {
        return internalLoop(_pin);
    }
    virtual void loop() override
    {
        internalLoop(curPin);
    }
    virtual int internalLoop(const int pin = 255)
    {
        // #ifdef DEBUG_API
        // Serial.printf(" %s internalLoop %i stateOn %i status %i v1 %i\r\n", _mode, pin, stateOn, curStatus, v1);
        // #endif
        if (stateOn)
        {
            curPin = pin;
            if (pin == 255 || pin < 0)
                curPin = getProtocol()->findPinByMode(_mode);
            if (pin < 0)
            {
                curStatus = false;
                digitalWrite(curPin, curStatus);
                ultimoChanged = 0;
            }
            else
            {
                if (((!curStatus && millis() - ultimoChanged > v1)) || (curStatus && (millis() - ultimoChanged > interval)))
                {
                    curStatus = !curStatus;
                    digitalWrite(curPin, curStatus);
#ifdef DEBUG_API
                    Serial.printf("write %s status %i set %i\r\n", _mode.c_str(), curPin, curStatus);
#endif
                    ultimoChanged = millis();
                }
            }
        }
        return curStatus;
    }
};

class OkDriver : public AlternateDriver
{
public:
    static void registerMode()
    {
        registerDriverMode("ok", create);
    }
    static Driver *create()
    {
        return new OkDriver();
    }
    OkDriver()
    {
        interval = 5000;
    }
    int writePin(const int value) override
    {
        stateOn = value > 0;
#ifdef DEBUG_API
        Serial.printf("write %i set %i", _pin, stateOn);
#endif
        digitalWrite(_pin, stateOn);
        return stateOn;
    }
};

class LedDriver : public AlternateDriver
{

public:
    static void registerMode()
    {
        registerDriverMode("led", create);
    }
    static Driver *create()
    {
        return new LedDriver();
    }
    virtual void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
#ifdef DEBUG_API
        Serial.println("AlternateDriver set OUTPUT mode");
#endif
        Driver::setPinMode(pin);
#ifdef LED_INVERT
        interval = 5000;
        v1 = 500;
#else
        interval = 1000;
#endif
    }
    int writePin(const int value) override
    {
        return stateOn;
    }
};
