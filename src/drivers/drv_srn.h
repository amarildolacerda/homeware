#pragma once

#include "drv_led.h"

/// @brief Sirene
class SireneDriver : public OkDriver
{
public:
    int iniciadoEm = 0;
    static void registerMode()
    {
        registerDriverMode("srn", create);
        //        registerDriverMode("alarm", create);
    }
    static Driver *create()
    {
        return new SireneDriver();
    }
    SireneDriver()
    {
        interval = 1000;
        timeout = 60000;
    }
    void setup() override
    {
        AlternateDriver::setup();
    }
    void loop() override
    {
        if (stateOn && interval > 0 && (millis() - iniciadoEm > interval))
        {
            stateOn = false;
            internalLoop(-1);
            return;
        }
        else
            internalLoop(curPin);
    }
    int readPin() override
    {
        return stateOn;
    }
    int writePin(const int value) override
    {
        stateOn = (value > 0); // ativa o loop modo sirene
        iniciadoEm = millis();
        digitalWrite(_pin, stateOn);
        ultimoChanged = millis();
        curStatus = stateOn;
        updateTimeout(_pin);
        actionEvent(value);
        return stateOn;
    }
};
