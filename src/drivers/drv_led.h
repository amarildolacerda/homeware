#pragma once

#include <protocol.h>

class AlternateDriver : public Driver
{
protected:
    int curPin = -1;
    bool curStatus = false;
    unsigned long ultimoChanged = 0;
    unsigned long interval = 1000;
    bool stateOn = true;

public:
    AlternateDriver()
    {
        v1 = 5000;
    }
    void setV1(long v) override
    {
        if (v < 100)
            return;
        v1 = v;
    }
    virtual void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
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
        if (stateOn)
        {
            curPin = pin;
            if (pin == 255 || pin < 0)
                curPin = getProtocol()->findPinByMode(getMode());
            if (pin < 0)
            {
                curStatus = false;
                digitalWrite(curPin, curStatus);
                ultimoChanged = 0;
            }
            else
            {
                if ((!curStatus && (millis() - ultimoChanged > v1)) || (curStatus && (millis() - ultimoChanged > interval)))
                {
                    curStatus = !curStatus;
                    digitalWrite(curPin, curStatus);
                    ultimoChanged = millis();
                }
            }
        }
        return curStatus;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
    bool isLoop() override { return true; }
};

class LedDriver : public AlternateDriver
{

public:
    static void registerMode()
    {
        registerDriverMode("led", create);
        registerDriverMode("ok", create);
    }
    static Driver *create()
    {
        return new LedDriver();
    }
    LedDriver()
    {
        Driver::setV1(5000);
    }
    int writePin(const int value) override
    {
        if (getMode() == "ok")
        {
            stateOn = value > 0;
#ifdef DEBUG_ON
            Serial.printf("WritePin %s: %i v: %i", getMode(), _pin, stateOn);
#endif
            digitalWrite(_pin, stateOn);
        }
        return stateOn;
    }
};
