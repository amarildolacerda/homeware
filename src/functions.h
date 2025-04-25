
#ifndef functions_def
#define functions_def

#include <Arduino.h>
#include "options.h"

#ifndef ARDUINO_AVR
char *stringf(const char *format, ...);
uint32_t getChipId();

template <class T>
String type_name(const T &)
{
    String s = __PRETTY_FUNCTION__;
    int start = s.indexOf("[with T = ") + 10;
    int stop = s.lastIndexOf(']');
    return s.substring(start, stop);
}
#endif
String *split(String s, const char delimiter);
int getPinByName(String dig);

#endif
