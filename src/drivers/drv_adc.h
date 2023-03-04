#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
public:
    ADCDriver()
    {
        mode = "adc";
        getProtocol()->resources += "adc,";
    }
    virtual int readPin(const int pin)
    {
        return analogRead(pin);
    }
    virtual int writePin(const int pin, const int value)
    {
        analogWrite(pin, value);
        return value;
    }
    virtual bool isGet() { return true; }
    virtual bool isSet() { return true; }
};
