
#include <api/alexa.h>
#include <protocol.h>
#include "ArduinoJson.h"
#include "homeware.h"
#include <Espalexa.h>

Espalexa localalexa;

Espalexa getAlexa()
{
    return localalexa;
}

int localSensorId = 0;
bool localAlexaInited = false;

void Alexa::begin(Espalexa server)
{
    if (localAlexaInited)
        return;
    localalexa = server;
    getInstanceOfProtocol()
        ->resources += "alexa,";
    localalexa.setFriendlyName(getInstanceOfProtocol()->config["label"]);
    localAlexaInited = true;

#ifdef DEBUG_ALEXA
    Serial.println("Alexa::init()");
#endif
}

void Alexa::afterSetup()
{
}
void Alexa::beforeSetup()
{
    productName = "Alexa"; // dont change - nao alterar, usado para checar qual drive na busca do pin
}
void Alexa::loop()
{
#ifdef DEBUG_ALEXA
    Serial.printf("Alexa sensorId: %i \r\n ", sensorId);
#endif
    if (sensorId == 0)
    {
#ifdef DEBUG_ALEXA
        Serial.println("AlexaLight loop()");
#endif
       // alexa.loop();  // passado para o main
    }
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
    localalexa.addDevice(getName(), onoffChanged, EspalexaDeviceType::onoff); // non-dimmable device
#ifdef DEBUG_ALEXA
    Serial.println("AlexaLight.addDevice(..)");
#endif
}
void AlexaLight::changed(const int pin, const long value)
{
    EspalexaDevice *d = localalexa.getDevice(sensorId);
    d->setState(true); // indica se esta ligado
    d->setValue(value);
    d->setPercent((value > 0) ? 100 : 0);
#ifdef DEBUG_ON
    Serial.printf("AlexaLigh.changed(%i)\r\n", value);
#endif
}

void AlexaDimmable::beforeSetup()
{
    Alexa::beforeSetup();
    sensorId = localSensorId++;
    localalexa.addDevice(getName(), dimmableChanged, EspalexaDeviceType::dimmable); // non-dimmable device
#ifdef DEBUG_ALEXA
    Serial.println("AlexaDimmable.addDevice(..)");
#endif
}

void AlexaDimmable::changed(const int pin, const long value)
{
    EspalexaDevice *d = localalexa.getDevice(sensorId);
    d->setState(true); // indica se esta ligado
    d->setValue(value);
    d->setPercent((value / 1024) * 100);
}
