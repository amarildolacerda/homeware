
#include <drivers.h>

class BuzzerDriver : public Driver
{
private:
    bool playing = false;
    int position = 1;
    long lastOne = 0;
    int count = 0;

public:
    static void registerMode()
    {
        registerDriverMode("bzz", create);
        registerDriverMode("bip", create);
    }
    static Driver *create()
    {
        return new BuzzerDriver();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
    }
    int writePin(const int value) override
    {
        playing = value > 0;
        if (playing)
        {
            position = 10;
            count = 0;
            //   playSirene();
        }
        debug(value);
        return playing;
    }
    int readPin() override
    {
        return playing;
    }
    void loop() override
    {
        if (playing)
        {
            playSirene();
            if (_mode == "bip")
            {
                if ((count++) > 5)
                    playing = false;
            }
        }
    }
    void playSirene()
    {
        if (millis() - lastOne > 20)
        {
            int frequencia = 1000 * sin(position++ * (3.1412 / 180)); // calcula a frequência
            int duracao = 10;                                         // define a duração
            tone(_pin, frequencia, duracao);                          // gera o som
            if (position >= 180)
                position = 1;
            // #ifdef DEBUG_DRV
            Serial.printf("tone %i %i, %i\r\n", position, frequencia, duracao);
            // #endif
            lastOne = millis();
        }
    }
    bool isSet() override { return true; }
    bool isLoop() override { return true; }
};