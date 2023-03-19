#pragma once

#include "drv_led.h"

/// @brief Sirene
class SireneDriver : public OkDriver
{
public:
    unsigned long desligarDepoisDe = 60000;
    int iniciadoEm = 0;
    static void registerMode()
    {
        registerDriverMode("srn", create);
        registerDriverMode("alarm", create);
    }
    static Driver *create()
    {
        return new SireneDriver();
    }
    SireneDriver()
    {
        interval = 1000;
    }
    void setup() override
    {
        AlternateDriver::setup();
        Protocol *prot = getProtocol();
        if (prot->containsKey("srn_interval"))
            interval = prot->getKey("srn_interval").toInt();
        if (prot->containsKey("srn_duration"))
            desligarDepoisDe = prot->getKey("srn_duration").toInt();
        setV1(interval);
    }
    void loop() override
    {
        if (stateOn && desligarDepoisDe > 0 && (millis() - iniciadoEm > desligarDepoisDe))
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
        return stateOn;
    }
    bool isSet() override { return true; }
    bool isLoop() override { return true; }
};

/// @brief PulseDriver
class PulseDriver : public SireneDriver
{
public:
    static void registerMode()
    {
        registerDriverMode("pulse", create);
    }
    static Driver *create()
    {
        return new PulseDriver();
    }

    PulseDriver()
    {
        interval = 10000;
    }
    void setup() override
    {
        AlternateDriver::setup();
        Protocol *prot = getProtocol();
        if (prot->containsKey("pulse_duration"))
            interval = prot->getKey("pulse_duration").toInt();
        desligarDepoisDe = interval;
        setV1(interval);
    }
};
