
#include <options.h>
#ifdef ALEXA

#include <api/alexa.h>
#include <protocol.h>
#include "ArduinoJson.h"
#include "homeware.h"
#include <Espalexa.h>
//#include "ESP8266HTTPClient.h"

Espalexa *localalexa; //= new Espalexa();

Espalexa *Alexa::getAlexa()
{
    return localalexa;
}

int localSensorId = 0;
bool localAlexaInited = false;

void Alexa::init(Espalexa *alx)
{
    if (localAlexaInited)
        return;
    localalexa = alx;
    //  Alexa::server = externalServer;
    getInstanceOfProtocol()
        ->apis += "alexa,";
    localalexa->setFriendlyName(getInstanceOfProtocol()->config["label"]);

#ifdef DEBUG_ALEXA
    Serial.println("Alexa::init()");
#endif
}

void Alexa::afterSetup()
{
    // if (localAlexaInited)
    //     return;
    // localalexa->begin(&server);
    localAlexaInited = true;
}
void Alexa::beforeSetup()
{
    productName = "Alexa"; // dont change - nao alterar, usado para checar qual drive na busca do pin
}

void Alexa::handle()
{
    localalexa->loop(); // passado para o main
}

void Alexa::loop()
{
#ifdef DEBUG_ALEXA
    Serial.printf("Alexa sensorId: %i \r\n ", sensorId);
#endif
    if (sensorId == 0)
        Alexa::handle();
#ifdef DEBUG_ALEXA
    Serial.println("AlexaLight loop()");
#endif
}

int Alexa::findAlexaPin(EspalexaDevice *d)
{
    if (d == nullptr)
        return -1;       // this is good practice, but not required
    int id = d->getId(); // * base 0
    int n = 0;
#ifdef DEBUG_ALEXA
    Serial.printf("procurando alexa: %i\r\n", id);
#endif
    for (int i = 0; i < getApiDrivers().count(); i++)
    {
        ApiDriver *p = getApiDrivers().getItem(i);
        if (p)
        {
            if (p->productName == "Alexa")
            {
                if (id == n)
                    return p->pin;
                n++;
            }
        }
    }
    return -1;
}

String Alexa::getName()
{
    String sName = getInstanceOfProtocol()->config["label"];
    sName += "-";
    sName += String(pin);
#ifdef DEBUG_ALEXA
    Serial.printf("getName()->%s\r\n", sName);
#endif
    return sName;
}

void AlexaLight::beforeSetup()
{
    Alexa::beforeSetup();
    sensorId = localSensorId++;
    getAlexa()->addDevice(getName(), AlexaLight::onoffChanged, EspalexaDeviceType::onoff); // non-dimmable device
#ifdef DEBUG_ALEXA
    Serial.println("AlexaLight.addDevice(..)");
#endif
}
void AlexaLight::onoffChanged(EspalexaDevice *d)
{
    bool value = d->getState();
    int pin = findAlexaPin(d);
    getInstanceOfProtocol()->writePin(pin, (value) ? HIGH : LOW);
}
void AlexaLight::changed(const int pin, const long value)
{
#ifdef DEBUG_API
    Serial.printf("BEGIN: AlexaLigh.changed(%i)\r\n", value);
#endif
    EspalexaDevice *d = getAlexa()->getDevice(sensorId);
    if (d)
    {
        d->setState(true); // indica se esta ligado
        d->setValue(value);
        d->setPercent((value > 0) ? 100 : 0);
    }
#ifdef DEBUG_API
    Serial.printf("END: AlexaLigh.changed(%i)\r\n", value);
#endif
}

/*
void AlexaDimmable::beforeSetup()
{
    Alexa::beforeSetup();
    sensorId = localSensorId++;
    getAlexa()->addDevice(getName(), dimmableChanged, EspalexaDeviceType::dimmable); // non-dimmable device
#ifdef DEBUG_ALEXA
    Serial.println("AlexaDimmable.addDevice(..)");
#endif
}

void AlexaDimmable::changed(const int pin, const long value)
{
    EspalexaDevice *d = getAlexa()->getDevice(sensorId);
    d->setState(true); // indica se esta ligado
    d->setValue(value);
    d->setPercent((value / 1024) * 100);
}
*/
#endif