#pragma once

#include <protocol.h>

class AlternateDriver : public Driver
{
protected:
    int curPin = -1;
    bool curStatus = false;
    unsigned long ultimoChanged = 0;
    int interval = 1000;

public:
    virtual int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return internalLoop(pin);
    }
    virtual void loop() override
    {
        internalLoop(curPin);
    }
    virtual int internalLoop(const int pin = 255)
    {
        curPin = pin;
        if (pin == 255 || pin < 0)
            curPin = getProtocol()->findPinByMode(getMode());
        if (curPin > -1)
        {
            if (pin < 0)
            {
                curStatus = false;
                digitalWrite(curPin, curStatus);
            }
            else
            {
                if (millis() - ultimoChanged > v1)
                {
                    curStatus = true;
                    digitalWrite(curPin, curStatus);
                    ultimoChanged = millis();
                }
                else if (curStatus && millis() - ultimoChanged > interval)
                {
                    curStatus = false;
                    digitalWrite(curPin, curStatus);
                    ultimoChanged = millis();
                }
            }
        }
        return curStatus;
    }
    bool isGet() override { return true; }
    bool isSet() override { return false; }
};

class LedDriver : public AlternateDriver
{
public:
    LedDriver()
    {
        Driver::setV1(5000);
        Driver::setMode("led");
    }
};
