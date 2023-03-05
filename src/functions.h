
#ifndef functions_def
#define functions_def

#include <Arduino.h>

char *stringf(const char *format, ...);
uint32_t getChipId() ;
String *split(String s, const char delimiter);

template <class T>
String type_name(const T &)
{
    String s = __PRETTY_FUNCTION__;
    int start = s.indexOf("[with T = ") + 10;
    int stop = s.lastIndexOf(']');
    return s.substring(start, stop);
}

#endif