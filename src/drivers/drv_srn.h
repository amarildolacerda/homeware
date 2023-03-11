#pragma once

#include "drv_led.h"

class SireneDriver : public AlternateDriver
{
public:
    bool onOffStatus = false;

public:
    SireneDriver()
    {
        Driver::setV1(1000);
        Driver::setMode("srn");
    }
    void loop() override
    {
        if (!onOffStatus)
            return; // Sirene não esta ligada
        AlternateDriver::loop();
    }
    int writePin(const int pin, const int value) override
    {
        onOffStatus = (value > 0) ? HIGH : LOW; // ativa o loop modo sirene
        return curStatus;
    }
};