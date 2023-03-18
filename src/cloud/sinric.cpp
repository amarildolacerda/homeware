#include <cloud/sinric.h>
#include "ArduinoJson.h"

#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "SinricProTemperaturesensor.h"
#include "SinricProDoorbell.h"

static bool sinricLoaded = false;
static int sinricInstanceCount = 0;

void registerSinricApi()
{
    MotionSinricCloud::registerCloud();

    getInstanceOfProtocol()->resources += "SINRIC,";
}

int SinricCloud::findSinricPin(String id)
{
    for (JsonPair k : getProtocol()->getSensors())
    {
        if (k.value().as<String>() == id)
            return String(k.key().c_str()).toInt();
    }
    return -1;
}
bool SinricCloud::onSinricPowerState(const String &deviceId, bool &state)
{
    Serial.printf("Device %s turned %s (via SinricPro) \r\n", deviceId.c_str(), state ? "on" : "off");
    int pin = findSinricPin(deviceId.c_str());
    if (pin > -1)
        getProtocol()->writePin(pin, state ? HIGH : LOW);
    return true; // req
}
void SinricCloud::setup()
{
    Protocol *prot = getProtocol();
    if (prot)
    {
        sensorId = prot->getSensors()[String(pin)].as<String>();

#ifdef DEBUG_ON
        String sJson;
        serializeJson(json, sJson);
        Serial.printf("Sensores: %i - %s \r\n", pin, sJson.c_str());
#endif
    }
#ifdef DEBUG_ON
    else
    {
        Serial.println("a instância protocol não esta completa sua inicialização");
    }
    Serial.printf("Sensor: %s \r\n", sensorId.c_str());
#endif
}

void SinricCloud::initSinric()
{
    if (!sinricLoaded)
    {

        Protocol *prot = getInstanceOfProtocol();
        SinricPro.onConnected([]()
                              {
                                Protocol *prot = getInstanceOfProtocol();
                                prot->resetDeepSleep();
                                Serial.printf("Connected to SinricPro\r\n"); });
        SinricPro.onDisconnected([]()
                                 { Serial.printf("Disconnected from SinricPro\r\n"); });
        SinricPro.begin(prot->config["app_key"], prot->config["app_secret"]);

        sinricLoaded = true;
        Serial.println("Sinric pronto");
#ifdef DEBUG_ON
        Serial.printf("Key: %s token: %s", prot->config["app_key"], prot->config["app_secret"]);
#endif
    }
    sinricInstanceCount++;
}

void MotionSinricCloud::changed(const int pin, const long value)
{
    bool bValue = value > 0;
    Serial.printf("Motion %s\r\n", bValue ? "detected" : "not detected");
    SinricProMotionsensor &myMotionsensor = SinricPro[sensorId]; // get motion sensor device
    myMotionsensor.sendPowerStateEvent(bValue);
    myMotionsensor.sendMotionEvent(bValue);
}
void MotionSinricCloud::setup()
{
    SinricCloud::setup();
    SinricProMotionsensor &myMotionsensor = SinricPro[sensorId];
    myMotionsensor.onPowerState(onSinricPowerState);
#ifdef DEBUG_ON
    Serial.println("Sinric Motion OK");
#endif
}

void SinricCloud::loop()
{
   SinricPro.handle();
}
