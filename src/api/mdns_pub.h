#pragma once

#ifndef ESP32
#include "api.h"
#include "functions.h"

class mDNSPubDriver : public ApiDriver
{
public:
    static void
    registerApi()
    {
        registerDriver("mDNS", create, true);
    }
    static ApiDriver *create()
    {
        return new mDNSPubDriver();
    }
    void setup() override;
    void loop() override;
};
#endif