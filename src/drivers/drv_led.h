#pragma once

#include <protocol.h>

class AlternateDriver : public Driver
{
protected:
    int curPin = -1;
    bool curStatus = false;
    unsigned long ultimoChanged = 0;
    bool stateOn = true;

public:
    AlternateDriver()
    {
        v1 = 5000;
        timeout = 60000;
        interval = 5000;
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
#ifdef DEBUG_DRV
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
#ifdef DEBUG_DRV
        Serial.printf(" %s internalLoop %i stateOn %i status %i v1 %i\r\n", _mode, pin, stateOn, curStatus, v1);
#endif
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
                if (((!curStatus && millis() - ultimoChanged > v1)) || (curStatus && (millis() - ultimoChanged > interval)))
                {
                    curStatus = !curStatus;
                    digitalWrite(curPin, curStatus);
#ifdef DEBUG_DRV
                    Serial.printf("write %s status %i set %i\r\n", _mode, curPin, curStatus);
#endif
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
        Driver::setV1(5000);
    }
    int writePin(const int value) override
    {
        stateOn = value > 0;
#ifdef DEBUG_DRV
        Serial.printf("write %s: %i set %i", getMode(), _pin, stateOn);
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
    LedDriver()
    {
        Driver::setV1(5000);
    }
    int writePin(const int value) override
    {
        return stateOn;
    }
};
