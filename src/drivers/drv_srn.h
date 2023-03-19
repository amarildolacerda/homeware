#pragma once

#include "drv_led.h"

class SireneDriver : public AlternateDriver
{
public:
    bool onOffStatus = false;
    unsigned long desligarDepoisDe = 60000;
    unsigned long iniciadoEm = 0;

public:
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
        if (!onOffStatus)
            return; // Sirene não esta ligada
        if (desligarDepoisDe > 0 && (millis() - iniciadoEm > desligarDepoisDe))
        {
            onOffStatus = false;
            internalLoop(-1);
            return;
        }
        internalLoop(curPin);
    }
    int readPin() override
    {
        return onOffStatus;
    }
    int writePin(const int value) override
    {
        Serial.printf("srn %i para %i", _pin, value);
        onOffStatus = (value > 0); // ativa o loop modo sirene
        if (onOffStatus)
            iniciadoEm = millis();
        digitalWrite(_pin, onOffStatus);
        ultimoChanged = millis();
        curStatus = onOffStatus;
        return onOffStatus;
    }
};

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
