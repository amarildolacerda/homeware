#pragma once

#include <protocol.h>

class ADCDriver : public Driver
{
private:
    int eventState = 0;

    unsigned long ultimoLoop = 0;
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
#ifdef DEBUG_API
        Serial.println("ADCDriver creating");
#endif
        return new ADCDriver();
    }
    void setup() override
    {
        Driver::setup();
        interval = 500;
        Protocol *prot = getProtocol();
        if (prot->containsKey(sMin))
            min = prot->getKey(sMin).toInt();
        if (prot->containsKey(sMax))
            max = prot->getKey(sMax).toInt();
        triggerEnabled = true;
        active = true;
#ifdef DEBUG_API
        Serial.println("END: ADC setup()");
#endif
    }

    int readPin() override
    {
        getAdcState();
        if (min == 0 && max == 0)
            return tmpAdc;
        return genStatus();
    }
    int writePin(const int value) override
    {
        analogWrite(_pin, value);
        return value;
    }
#ifndef BASIC
    int internalRead() override
    {
        return tmpAdc;
    }
#endif

    void loop() override
    {
#ifdef DEBUG_API
        Serial.println("BEGIN: ADC loop()");
#endif

        int st = getAdcState();

        if (eventState != st)
        {
            eventState = st;
#ifdef DEBUG_API
            getProtocol()->debugf("ADC value: %i status: %i Min: %i Max: %i \r\n", tmpAdc, st, min, max);
#endif
            actionEvent(tmpAdc);
            if (triggerCallback)
                triggerCallback(_mode, _pin, eventState);
            trgOkState = true;
        }
        if (trgOkState)
        {
            if (triggerOkState)
                triggerOkState("ok", _pin, eventState == 0 ? HIGH : LOW);
            trgOkState = false;
        }
#ifdef DEBUG_API
        Serial.println("END: ADC loop()");
#endif
    }
    int genStatus()
    {
#ifdef DEBUG_API
        Serial.println("BEGIN: ADC genStatus()");
#endif
        int rt = eventState;
        if (max == 0 && min == 0)
            rt = tmpAdc;
        else if (max > 0 && tmpAdc >= max)
            rt = HIGH;
        else if (min > 0 && tmpAdc < min)
            rt = HIGH;
        else
            rt = LOW;
#ifdef DEBUG_API
        Serial.printf("END: genStatus()->%i\r\n", rt);
#endif
        return rt;
    }
    int getAdcState()
    {
#ifdef DEBUG_API
        Serial.println("BEGIN: getAdcState()");
#endif
        if (millis() - ultimoLoop > interval)
        {
            tmpAdc = analogRead(_pin);
            ultimoLoop = millis();
        }
#ifdef DEBUG_API
        Serial.printf("END: getAdcState() (%i)\r\n", tmpAdc);
#endif
        return genStatus();
    }
};
