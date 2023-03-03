

#ifndef protocol_def
#define protocol_def

#include <ArduinoJson.h>
#include <options.h>

class Protocol
{
public:
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);

    void reset();
};

#endif
