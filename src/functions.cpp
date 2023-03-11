#include "Arduino.h"
#include "options.h"

#ifdef LITTLEFS
#include "LittleFS.h"
#endif

#ifdef ESP32
const char *getChipId() { return ESP.getChipModel(); }

#else
#ifdef ARDUINO_AVR
#else
uint32_t getChipId() { return ESP.getChipId(); }
#endif
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
    if (dig.substring(0, 1) == "A")
        return PIN_A0;
    else if (dig.substring(0, 1) == "D")
    {
        String p = dig.substring(1);
        if (v[p.toInt()])
            return v[p.toInt()];
        else
            return p.toInt();
    }
#else
    if (dig.substring(0, 1) == "A")
        return PIN_A0;
    else if (dig.substring(0, 1) == "D")
    {
        String p = dig.substring(1);
        return p.toInt();
    }
#endif
    return dig.toInt();
}