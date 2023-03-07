#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
private:
    size_t interval = 500;
    long ultimoLoop = 0;
    const String adc_interval = "adc_interval";

public:
    void setup() override
    {
        Driver::setMode("adc");
        Driver::setup();
        Protocol *p = getProtocol();
        if (p->containsKey(adc_interval))
            interval = p->getKey(adc_interval).toInt();
        triggerEnabled = true;
    }
    void setPinMode(int pin) override
    {
        active = true;
    }

    int readPin(const int pin) override
    {
        Driver::setPin(pin);
        return analogRead(pin);
    }
    int writePin(const int pin, const int value) override
    {
        analogWrite(pin, value);
        return value;
    }
    bool isGet() override { return true; }
    bool isSet() override { return true; }
    bool isLoop() override { return active;}
    void loop()override{
        int pin = getPin();
        if (millis() - ultimoLoop > interval && pin > -1)
        {
            ultimoLoop = millis();
            triggerCallback(getMode(), pin, readPin(pin));
        }
    }
};
