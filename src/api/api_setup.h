
#pragma once

#include "api.h"

#ifdef MDNS
#include "mdns_pub.h"
#endif

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

void register_ApiSetup()
{
    DEBUGF("enter register_ApiSetup()\r\n");
#ifdef MDNS
    mDNSPubDriver::registerApi();
#endif
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

    DEBUGF("end register_ApiSetup()\r\n---------------------------------\r\n");
}