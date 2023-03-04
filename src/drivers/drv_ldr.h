#pragma once

#include <protocol.h>

class LDRDriver : public Driver
{
public:
    int currentAdcState = 0;
    LDRDriver()
    {
        mode = "ldr";
        getProtocol()->resources += "ldr,";
    }
    virtual int readPin(const int xpin)
    {
        pin = xpin;
        return getAdcState(pin);
    }
    int getAdcState(int pin)
    {
        Protocol *prot = getProtocol();
        unsigned int tmpAdc = analogRead(pin);
        int rt = currentAdcState;
        const unsigned int v_min = prot->config["adc_min"].as<int>();
        const unsigned int v_max = prot->config["adc_max"].as<int>();
        if (tmpAdc >= v_max)
            rt = LOW;
        if (tmpAdc < v_min)
            rt = HIGH;
        char buffer[64];
        sprintf(buffer, "adc %d,currentAdcState %d, adcState %s  (%i,%i) ", tmpAdc, currentAdcState, (rt > 0) ? "ON" : "OFF", v_min, v_max);
        prot->debug(buffer);
        currentAdcState = rt;
        return rt;
    }
};