#include <Arduino.h>
#include "options.h"

#ifndef ARDUINO_AVR

#ifdef LITTLEFS
#include <LittleFS.h>
#endif

#ifdef ESP32
const char *getChipId() { return ESP.getChipModel(); }

#else
uint32_t getChipId() { return ESP.getChipId(); }
#endif

char *stringf(const char *format, ...)
{
    static char buffer[512];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    return buffer;
}

#endif

String *split(String s, const char delimiter)
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < s.length(); i++)
    {
        if (s[i] == delimiter)
        {
            count++;
        }
    }

    String *words = new String[count + 1];
    unsigned int wordCount = 0;

    for (unsigned int i = 0; i < s.length(); i++)
    {
        if (s[i] == delimiter)
        {
            wordCount++;
            continue;
        }
        words[wordCount] += s[i];
    }
    // words[count+1] = 0;

    return words;
}

int getPinByName(String dig)
{
#ifdef USE_PIN_NAMES
    uint8_t v[16] = {

        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        D10,
        D11,
        D12,
        D13,
        D14,
        D15};

    dig.toUpperCase();
    if (dig == nullptr)
        return -1;
#ifdef PIN_WIRE_SDA
    else if (dig == "SDA")
        return SDA;
#endif
#ifdef PIN_WIRE_SCL
    else if (dig == "SCL")
        return SCL;
#endif
#ifdef LED_BUILTIN
    else if (dig == "LED")
        return LED_BUILTIN;
#endif
#ifdef PIN_A0
    else if (dig == "A0")
        return A0;
#endif
#ifdef PIN_A1
    else if (dig == "A1")
        return A1;
#endif
#ifdef PIN_A2
    else if (dig == "A2")
        return A2;
#endif
    else if (dig.substring(0, 1) == "D")
    {
        String p = dig.substring(1);
        if (v[p.toInt()])
            return v[p.toInt()];
        else
            return p.toInt();
    }
#else
    if (dig.substring(0, 2) == "A0")
        return A0;
#ifdef ESP32
    else if (dig.substring(0, 2) == "A3")
        return A3;
    else if (dig.substring(0, 2) == "A4")
        return A4;
#endif
    else if (dig.substring(0, 1) == "D")
    {
        String p = dig.substring(1);
        return p.toInt();
    }
#endif
    return dig.toInt();
}