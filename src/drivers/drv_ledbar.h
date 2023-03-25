#include "drivers.h"

class LedBarDriver : public Driver
{
protected:
    enum LedType
    {
        LED_TYPE_SHIFT = 16,
        LED_BAR_10 = 0 << LED_TYPE_SHIFT | 10,
        LED_CIRCULAR_24 = 0 << LED_TYPE_SHIFT | 24,
        LED_MAX_COUNT = 0 << LED_TYPE_SHIFT | 24,
        // LED_NEW_DEVICE = 1 << LED_TYPE_SHIFT | 10,
        LED_TYPE_MASK = (1 << LED_TYPE_SHIFT) - 1, // indicated there at most 65535 leds
    };

    enum LedState
    {
        LED_TURN_OFF,
        LED_FULL_BRIGHTNESS = 0xFF,
    };

private:
    int barValue = 0;
    int pinClock = 0;
    int pinData = 0;
    bool reverseShow = false;
    uint8_t led[LED_MAX_COUNT];
    uint32_t countOfShows = 10;
    LedType type = LedType::LED_BAR_10;

public:
    static void
    registerMode()
    {
        registerDriverMode("ledbar", create);
    }
    static Driver *create()
    {
        return new LedBarDriver();
    }
    void setPinMode(int pin) override
    {
        pinMode(pin, OUTPUT);
        pinMode(pin + 1, OUTPUT);
        pinClock = pin + 1;
        pinData = pin;
    }
    bool isSet() override { return true; }
    bool isLoop() override { return false; }
    int writePin(const int value)
    {
        countOfShows = countOfLed();
        barValue = value;
        setBits(value);
        return value;
    }
    int readPin()
    {
        return barValue;
    }
    void setBits(uint32_t value)
    {
        for (uint32_t i = 0; i < countOfLed(); i++, value >>= 1)
        {
            led[i] = value & 1 ? LED_FULL_BRIGHTNESS : LED_TURN_OFF;
            Serial.print(led[i]);
            Serial.print(" ");
        }
        Serial.println("");

        send();
    }
    uint32_t countOfLed()
    {
        return (uint32_t)type & LED_TYPE_MASK;
    }
    void send(uint16_t bits)
    {
        for (uint32_t i = 0, clk = 0; i < 16; i++)
        {
            digitalWrite(pinData, bits & 0x8000 ? HIGH : LOW);
            digitalWrite(pinClock, clk);
            clk = ~clk;
            bits <<= 1;
        }
    }
    void send()
    {
        if (reverseShow)
        {
            if (LED_BAR_10 != type)
            {
                send(0x00); // send cmd(0x00)

                for (uint32_t i = 24; i-- > 12;)
                {
                    send(led[i]);
                }

                send(0x00); // send cmd(0x00)

                for (uint32_t i = 12; i-- > 0;)
                {
                    send(led[i]);
                }
            }
            else
            {
                send(0x00); // send cmd(0x00)

                for (uint32_t i = countOfShows; i-- > 0;)
                {
                    send(led[i]);
                }
                for (uint32_t i = 0; i < 12 - countOfShows; i++)
                {
                    send(0x00);
                }
            }
        }
        else
        {
            send(0x00); // send cmd(0x00)

            for (uint32_t i = 0; i < 12; i++)
            {
                send(led[i]);
            }

            if (LED_BAR_10 == type)
            {
                latch();
                return;
            }

            send(0x00); // send cmd(0x00)

            for (uint32_t i = 12; i < 24; i++)
            {
                send(led[i]);
            }
        }
        latch();
    }
    void latch()
    {
        /*
        digitalWrite(pinData, LOW);
        digitalWrite(pinClock, HIGH);
        digitalWrite(pinClock, LOW);
        digitalWrite(pinClock, HIGH);
        digitalWrite(pinClock, LOW);
        delayMicroseconds(240);
        digitalWrite(pinData, HIGH);
        digitalWrite(pinData, LOW);
        digitalWrite(pinData, HIGH);
        digitalWrite(pinData, LOW);
        digitalWrite(pinData, HIGH);
        digitalWrite(pinData, LOW);
        digitalWrite(pinData, HIGH);
        digitalWrite(pinData, LOW);
        delayMicroseconds(1);
        digitalWrite(pinClock, HIGH);
        digitalWrite(pinClock, LOW);
        */
    }
};
