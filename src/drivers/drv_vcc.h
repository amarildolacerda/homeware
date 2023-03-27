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
    bool isSet() override { return false; }
    bool isLoop() override { return true; }
    int readPin()
    {
        return (int)getVcc();
    }
    float getVcc()
    {
        if (millis() - lastOne > 60000 || power == 0)
        {
            float vcc = ESP.getVcc();
            float v_min = 1024 * 30;
            float v_max = 65536.0;
            power = ((vcc - v_min) / (v_max - v_min)) * 1024;
            lastOne = millis();
        }
        return power;
    }
    void loop() override
    {
        Serial.printf("vcc: %i", (int)getVcc());
    }
};
