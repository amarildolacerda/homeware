#pragma once

#include "api.h"
#include "functions.h"

class MySensorsDriver : public ApiDriver
{
protected:
public:
    static void registerApi()
    {
        registerApiDriver("mqtt", create, true);
    }
    static ApiDriver *create()
    {
        return new MySensorsDriver();
    }
    void setup() override;
    void loop() override;
};