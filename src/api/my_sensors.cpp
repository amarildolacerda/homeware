#include "api/my_sensors.h"



#define MY_DEBUG
#define MY_RADIO_RF24
#define MY_GATEWAY_MAX_CLIENTS 2
#define MY_REPEATER_FEATURE
#define MY_GATEWAY_FEATURE

#include <core/MyGatewayTransport.h>
#include <core/MyMessage.h>
#include <core/MyMessage.cpp>

bool _initedGW = false;
static MyMessage _MQTT_msg;

bool gatewayTransportConnect(void)
{
    Serial.println("gatewayTransportConnect");
    return _initedGW;
}

bool gatewayTransportInit(void)
{
    Serial.println("gatewayTransportInit");
    gatewayTransportConnect();
    _initedGW = true;
    return _initedGW;
}

bool gatewayTransportAvailable(void)
{
    Serial.println("gatewayTransportAvailable");
    return _initedGW;
}

MyMessage &gatewayTransportReceive(void)
{
    Serial.println("gatewayTransportReceive; ");
    return _MQTT_msg;
}

bool gatewayTransportSend(MyMessage &message)
{
    Serial.print("gatewayTransportSend: ");
    Serial.println(message.data);
    return true;
}


#ifdef __cplusplus
#include <Arduino.h>
#endif
#include <stdint.h>

#include "hal/architecture/MyHwHAL.h"
#include "hal/crypto/MyCryptoHAL.h"

#include "MyConfig.h"
#include "core/MyHelperFunctions.cpp"

    typedef uint8_t unique_id_t[16];
#define MY_HWID_PADDING_BYTE (0xAAu)

#if defined(ARDUINO_ARCH_ESP8266)
#include "hal/architecture/ESP8266/MyHwESP8266.cpp"
#include "hal/crypto/generic/MyCryptoGeneric.cpp"
#elif defined(ARDUINO_ARCH_ESP32)
#include "hal/architecture/ESP32/MyHwESP32.cpp"
#include "hal/crypto/ESP32/MyCryptoESP32.cpp"
#endif

#include "hal/architecture/MyHwHAL.cpp"
#include "core/MyIndication.cpp"

#if !defined(MY_MQTT_PUBLISH_TOPIC_PREFIX)
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "mygateway1-out"
#endif

#if !defined(MY_MQTT_SUBSCRIBE_TOPIC_PREFIX)
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "mygateway1-in"
#endif

#if !defined(MY_MQTT_CLIENT_ID)
#define MY_MQTT_CLIENT_ID "mysensors-1"
#endif

// POWER PIN
#ifndef DOXYGEN
#if defined(MY_RF24_POWER_PIN) || defined(MY_RFM69_POWER_PIN) || defined(MY_RFM95_POWER_PIN) || defined(MY_RADIO_NRF5_ESB)
#define RADIO_CAN_POWER_OFF (true)
#else
#define RADIO_CAN_POWER_OFF (false)
#endif
#endif

#undef MY_SENSOR_NETWORK

#include "hal/transport/MyTransportHAL.h"
#include "core/MyGatewayTransport.cpp"
#include "core/MyProtocol.cpp"

#include "core/MySigning.cpp"

#include "core/MyTransport.h"
#include "core/MyTransport.cpp"

#include <hal/transport/RF24/driver/RF24.cpp>
#include <hal/transport/RF24/MyTransportRF24.cpp>


#include <core/MySensorsCore.h>
#include "core/MyCapabilities.h"
#include "core/MySplashScreen.cpp"
#include "core/MySensorsCore.cpp"


void MySensorsDriver::loop()
{
    _process();
}
void MySensorsDriver::setup()
{
    _begin();
}
