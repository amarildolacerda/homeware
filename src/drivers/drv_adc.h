#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
public:
    ADCDriver()
    {
        Driver::setMode("adc");
        getProtocol()->resources += "adc,";
        Serial.println("Init ADCDriver");
    }
    void setup() override
    {
        Serial.println("ADC.setup()");
        Driver::setup();
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
};
