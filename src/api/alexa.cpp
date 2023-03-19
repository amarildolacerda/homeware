
#include <api/alexa.h>
#include <protocol.h>
#include "ArduinoJson.h"
#include "homeware.h"

Espalexa alexa;
Espalexa getAlexa()
{
    return alexa;
}

int localSensorId = 0;

void Alexa::init()
{
    getInstanceOfProtocol()->resources += "alexa,";
    alexa.setFriendlyName(getInstanceOfProtocol()->config["label"]);
    alexa.begin(homeware.server);
#ifdef DEBUG_ON
    Serial.println("Alexa pronta");
#endif
}

void Alexa::beforeSetup()
{
    productName = "Alexa"; // dont change - nao alterar, usado para checar qual drive na busca do pin
}
void Alexa::loop()
{
    if (sensorId == 0)
        alexa.loop();
}

int Alexa::findAlexaPin(EspalexaDevice *d)
{
    if (d == nullptr)
        return -1;       // this is good practice, but not required
    int id = d->getId(); // * base 0
    int n = 0;
    for (ApiDriver *p : getApiDrivers().items)
    {
        if (p->productName == "Alexa")
        {
            if (id == n)
                return p->pin;
            n++;
        }
    }
    return -1;
}

String Alexa::getName()
{
    String sName = getInstanceOfProtocol()->config["label"];
    sName += "-";
    sName += String(pin);
    return sName;
}

void AlexaLight::beforeSetup()
{
    Alexa::beforeSetup();
    sensorId = localSensorId++;
    alexa.addDevice(getName(), onoffChanged, EspalexaDeviceType::onoff); // non-dimmable device
}
void AlexaLight::changed(const int pin, const long value)
{
    EspalexaDevice *d = alexa.getDevice(sensorId);
    d->setState(true); // indica se esta ligado
    d->setValue(value);
    d->setPercent((value > 0) ? 100 : 0);
}

void AlexaDimmable::beforeSetup()
{
    Alexa::beforeSetup();
    sensorId = localSensorId++;
    alexa.addDevice(getName(), dimmableChanged, EspalexaDeviceType::dimmable); // non-dimmable device
}

void AlexaDimmable::changed(const int pin, const long value)
{
    EspalexaDevice *d = alexa.getDevice(sensorId);
    d->setState(true); // indica se esta ligado
    d->setValue(value);
    d->setPercent((value / 1024) * 100);
}
