#pragma once
#include "drivers.h"

class VccDriver : public Driver
{
protected:
private:
    long lastOne = 0;
    float power = 0;

public:
    static void
    registerMode()
    {
        registerDriverMode("vcc", create);
    }
    static Driver *create()
    {
        return new VccDriver();
    }
    void setPinMode(int pin) override
    {
        // pinMode(pin, OUTPUT);
    }
    int readPin()
    {
        return (int)getVcc();
    }
    float getVcc()
    {
        if (millis() - lastOne > 60000 || power == 0)
        {
#ifdef ESP32
            power = getVcc32();
#else
            float vcc = ESP.getVcc();
            float v_min = 1024 * 30;
            float v_max = 65536.0;
            power = ((vcc - v_min) / (v_max - v_min)) * 1024;
#endif
            lastOne = millis();
            actionEvent(power);
        }
        return power;
    }

#ifdef ESP32
    float getVcc32()
    {  // TODO: chacar o valor de retonro para manter compatibilidade com ESP8266 (1024)
        // Ler o valor da entrada analógica A13 (ADC2_CH1)
      /*  int adcValue = analogRead(ADC_6db );

        // Calcular a tensão de alimentação (Vcc)
        float vRef = 1100.0; // Valor fixo da tensão de referência interna (1.1V)
        float vcc = (vRef * 4095.0) / adcValue;

        return vcc;
        */
       return 0;
    }
#endif
    void loop() override
    {
#ifdef DEBUG_ON
        Serial.printf("vcc: %i", (int)getVcc());
#endif
    }
};
