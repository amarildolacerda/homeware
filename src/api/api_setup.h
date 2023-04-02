
#pragma once

#include "api.h"

#ifdef SINRICPRO
#include "api/sinric.h"
#endif
#ifdef ALEXA
#include "api/alexa.h"
#endif

#ifdef MQTTBroker
#include "api/mqtt_broker.h"
#endif
#ifdef MQTTClient
#include "api/mqtt_client.h"
#endif

void register_Api_setup()
{
#ifdef SINRICPRO
    registerSinricApi();
#endif
#ifdef ALEXA
    AlexaLight::registerApi();
    // AlexaDimmable::registerApi();
#endif

#ifdef MQTTBroker
    MqttBrokerApi::registerApi();
#endif

#ifdef MQTTClient
    MqttClientDriver::registerApi();
#endif
}