#pragma once

#include <ArduinoJson.h>
#include "drivers.h"


class ProtocolBase
{
public:
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);


JsonObject getMode()
{
    return config["mode"].as<JsonObject>();
}

/// @brief Escreve uma valores para o Pin
/// @param pin - Pin alvo
/// @param value  - valor a escrever
/// @return valor atribuido
int writePin(const int pin, const int value)
{
    String mode = getMode()[String(pin)];

    if (mode != NULL)
    {
#ifdef DEBUG_ON
        debugf("write %s pin: %i set %i\r\n", mode, pin, value);
#endif
        Driver *drv = getDrivers()->findByPin(pin);
        if (drv)
        {
            drv->writePin(value);
        }
    }
    return value;
}
    
};