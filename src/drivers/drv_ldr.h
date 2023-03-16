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
    unsigned int tmpAdc = 0;
    unsigned int ultimoLoop = 0;
    unsigned interval = 60000;

public:
    void setup() override
    {
        Driver::setMode("ldr");
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
    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return getAdcState(pin);
    }
    int genStatus()
    {
        int rt = eventState;
        if (tmpAdc >= max)
            rt = LOW;
        if (tmpAdc < min)
            rt = HIGH;
        return rt;
    }
    int getAdcState(int pin)
    {
        if (millis() - ultimoLoop > interval)
        {
            Driver::setPin(pin);
            tmpAdc = analogRead(pin);
            ultimoLoop = millis();
        }
        return genStatus();
    }
    bool isLoop() override
    {
        return active;
    }
    void loop() override
    {
        getAdcState(getPin());

        if (eventState != genStatus())
        {
            eventState = genStatus();
            triggerCallback(getMode(), getPin(), eventState);
        }
    }
};