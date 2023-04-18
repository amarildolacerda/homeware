
#pragma once

#include "api.h"

#ifndef NO_MDNS
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

#ifdef MYSENSORS
#include "api/my_sensors.h"
#endif

void register_Api_setup()
{
#ifndef NO_MDNS
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

#ifdef MYSENSORS
    MySensorsDriver::registerApi();
#endif
}