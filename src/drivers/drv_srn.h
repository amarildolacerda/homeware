#pragma once

#include "drv_led.h"

class SireneDriver : public AlternateDriver
{
public:
    bool onOffStatus = false;

public:
    SireneDriver()
    {
        setV1(1000);
        setMode("srn");
    }
    void loop() override
    {
        if (!onOffStatus)
            return; // Sirene não esta ligada
        internalLoop(curPin);
    }
    int readPin(const int pin) override
    {
        setPin(pin);
        return onOffStatus;
    }
    int writePin(const int pin, const int value) override
    {
        onOffStatus = (value > 0) ? HIGH : LOW; // ativa o loop modo sirene
        return curStatus;
    }
};