
#include "LittleFS.h"
#ifdef ESP32
const char* getChipId() { return ESP.getChipModel(); }

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
