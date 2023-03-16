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

public:
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
    }
    void setPinMode(int pin) override
    {
        active = true;
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        getAdcState(pin);
        if (min == 0 && max == 0)
            return tmpAdc;
        return genStatus();
    }
    int writePin(const int pin, const int value) override
    {
        analogWrite(pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
    bool isLoop() override { return active; }
    void loop() override
    {
        getAdcState(getPin());

        if (eventState != genStatus())
        {
            eventState = genStatus();
            triggerCallback(getMode(), getPin(), eventState);
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
    int getAdcState(int pin)
    {
        if (millis() - ultimoLoop > interval)
        {
            Driver::setPin(pin);
            tmpAdc = analogRead(pin);
            getProtocol()->debugf("ADC value: %i status: %i Min: %i Max: %i \r\n", tmpAdc, genStatus(),min,max);
                ultimoLoop = millis();
        }
        return genStatus();
    }
};
