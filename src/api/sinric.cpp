#include <api/sinric.h>
#include "ArduinoJson.h"
#include "drivers.h"

#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "SinricProTemperaturesensor.h"
#include "SinricProDoorbell.h"

bool sinricLoaded = false;
int sinricInstanceCount = 0;

void registerSinricApi()
{
    MotionSinricCloud::registerApi();
    DoorbellSinricCloud::registerApi();
    TemperatureSinricCloud::registerApi();

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

    }
#ifdef DEBUG_ON
    else
    {
        Serial.println("a instância protocol não esta completa na sua inicialização");
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
    sensorIndex = sinricInstanceCount++;

    SinricCloud::setup();
    SinricProMotionsensor &myMotionsensor = SinricPro[sensorId];
    myMotionsensor.onPowerState(onSinricPowerState);
#ifdef DEBUG_ON
    Serial.println("Sinric Motion OK");
#endif
}

bool TemperatureSinricCloud::onSinricDHTPowerState(const String &deviceId, bool &state)
{
    Serial.printf("PowerState turned %s  \r\n", state ? "on" : "off");
    return true; // request handled properly
}

void TemperatureSinricCloud::setup()
{
    sensorIndex = sinricInstanceCount++;
    getInstanceOfProtocol()->debug("Ativando DHT11");
    SinricProTemperaturesensor &mySensor = SinricPro[sensorId];
    mySensor.onPowerState(onSinricDHTPowerState);
}

void DoorbellSinricCloud::setup()
{
    sensorIndex = sinricInstanceCount++;
    getInstanceOfProtocol()->debug("Ativando Doorbell");
    SinricProDoorbell &myDoorbell = SinricPro[sensorId];
    myDoorbell.onPowerState(onSinricPowerState);
}

float ultimaTemperaturaAferida = 0;
void TemperatureSinricCloud::changed(const int pin, const long value)
{
    auto *drv = getDrivers()->findByPin(pin);
    if (!drv)
        return;

    JsonObject r = drv->readStatus();
    float t = r["temperature"];
    float h = r["humidity"];
    if (ultimaTemperaturaAferida != t)
    {
        ultimaTemperaturaAferida = t;
        SinricProTemperaturesensor &mySensor = SinricPro[sensorId]; // get temperaturesensor device
        mySensor.sendTemperatureEvent(t, h);                        // send event
        String result;
        serializeJson(r, result);
        Serial.println(result);
    }
}

void DoorbellSinricCloud::changed(const int pin, const long value)
{
    bool bValue = value > 0;
    Serial.printf("Doorbell %s\r\n", bValue ? "detected" : "not detected");
    SinricProDoorbell &myDoorbell = SinricPro[sensorId];
    myDoorbell.sendPowerStateEvent(bValue);
    if (bValue)
        myDoorbell.sendDoorbellEvent();
}

void SinricCloud::loop()
{
    if (sensorIndex == 0)
      SinricPro.handle();
}
