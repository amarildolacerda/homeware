
#pragma once

#include <api.h>

#ifdef SINRICPRO
#include <api/sinric.h>
#endif
#ifdef ALEXA
#include <api/alexa.h>
#endif

void register_Api_setup()
{
#ifdef SINRICPRO
    registerSinricApi();
#endif
#ifdef ALEXA
    AlexaLight::registerApi();
    AlexaDimmable::registerApi();
    Alexa::init();// last - deixar por último - need to be last one
#endif
}