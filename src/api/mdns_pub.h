#pragma once

#include "api.h"
#include "functions.h"

class mDNSPubDriver : public ApiDriver
{
public:
    static void
    registerApi()
    {
        registerApiDriver("mDNS", create, true);
    }
    static ApiDriver *create()
    {
        return new mDNSPubDriver();
    }
    void setup() override;
    void loop() override;


    
};