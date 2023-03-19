#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
private:
    size_t interval = 500;
    int eventState = 0;

    long ultimoLoop = 0;
    const String adc_interval = "adc_interval";
    const String sMin = "adc_min";
    const String sMax = "adc_max";
    int tmpAdc = 0;
    int min = 0;
    int max = 0;
    bool trgOkState = true;

public:
    static void
    registerMode()
    {
        registerDriverMode("adc", create);
    }
    static Driver *create()
    {
        return new ADCDriver();
    }
    void setup() override
    {
        Driver::setMode("adc");
        Driver::setup();
        Protocol *prot = getProtocol();
        if (prot->containsKey(adc_interval))
            interval = prot->getKey(adc_interval).toInt();
        if (prot->containsKey(sMin))
            min = prot->getKey(sMin).toInt();
        if (prot->containsKey(sMax))
            max = prot->getKey(sMax).toInt();
        triggerEnabled = true;
        active = true;
    }

    int readPin() override
    {
        getAdcState();
        if (min == 0 && max == 0)
            return tmpAdc;
        return genStatus();
    }
    int writePin( const int value) override
    {
        analogWrite(_pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
    bool isLoop() override { return active; }
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
    int genStatus()
    {
        int rt = eventState;
        if (max > 0 && tmpAdc >= max)
            rt = HIGH;
        else if (min > 0 && tmpAdc < min)
            rt = HIGH;
        else
            rt = LOW;
        return rt;
    }
    int getAdcState()
    {
        if (millis() - ultimoLoop > interval)
        {
            tmpAdc = analogRead(_pin);
#ifdef DEBUG_FULL
            getProtocol()->debugf("ADC value: %i status: %i Min: %i Max: %i \r\n", tmpAdc, genStatus(), min, max);
#endif
            ultimoLoop = millis();
        }
        return genStatus();
    }
};
