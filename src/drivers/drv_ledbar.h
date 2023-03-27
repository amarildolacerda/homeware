#pragma once
#include "drivers.h"
#include <Grove_LED_Bar.h>
class LedBarDriver : public Driver
{
protected:
private:
    int barValue = 0;
    int pinClock = 0;
    int pinData = 0;
    Grove_LED_Bar *bar;
    int oldNumBars = 0;

public:
    static void
    registerMode()
    {
        registerDriverMode("ledbar", create);
    }
    static Driver *create()
    {
        return new LedBarDriver();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
        pinMode(pin + 1, OUTPUT);
        pinClock = pin;
        pinData = pin + 1;
        bar = new Grove_LED_Bar(pinClock, pinData, 1, LED_BAR_10);
    }
    bool isSet() override { return true; }
    bool isLoop() override { return false; }
    int writePin(const int value)
    {
        barValue = map(value, 0, 1024, 0, 10);
        if (oldNumBars != barValue)
        {
            Serial.println(barValue);
            bar->setLevel(barValue);
            oldNumBars = barValue;
            debug(barValue);
        }
        return barValue;
    }
    int readPin()
    {
        return barValue;
    }
};
