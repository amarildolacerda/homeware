#pragma once

#include <protocol.h>

#define sMin "ldr_min"
#define sMax "ldr_max"
#define sInterval "ldr_interval"

class LDRDriver : public Driver
{

private:
    int eventState = 0;
    unsigned int min = 300;
    unsigned int max = 800;
    unsigned int tmpValue = 0;
    unsigned int ultimoLoop = 0;
    unsigned interval = 60000;
    bool trgOkState = true;

public:
    static void
    registerMode()
    {
        registerDriverMode("ldr", create);
    }
    static Driver *create()
    {
        return new LDRDriver();
    }

    void setup() override
    {
        Driver::setup();
        Protocol *prot = getProtocol();
        if (prot->containsKey(sMin))
            min = prot->getKey(sMin).toInt();
        if (prot->containsKey(sMax))
            max = prot->getKey(sMax).toInt();
        if (prot->containsKey(sInterval))
            interval = prot->getKey(sInterval).toInt();
        triggerEnabled = true;
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, INPUT);
        active = true;
    }
    int readPin() override
    {
        return getAdcState();
    }
    int genStatus()
    {
        int rt = eventState;
        if (tmpValue >= max)
            rt = LOW;
        if (tmpValue < min)
            rt = HIGH;
        return rt;
    }
    int getAdcState()
    {
        if (millis() - ultimoLoop > interval)
        {
            tmpValue = analogRead(_pin);
            ultimoLoop = millis();
            debug(tmpValue);
        }
        return genStatus();
    }
    bool isLoop() override
    {
        return active;
    }
    void loop() override
    {
        getAdcState();

        if (eventState != genStatus())
        {
            eventState = genStatus();
            triggerCallback(_mode, _pin, eventState);
            trgOkState = true;
        }
        if (trgOkState)
        {
            triggerOkState("ok", _pin, eventState == 0 ? HIGH : LOW);
            trgOkState = false;
        }
    }
    #ifndef BASIC
    int internalRead() override
    {
        return tmpValue;
    }
    #endif
};