#pragma once

#ifdef ALEXA
#include <api.h>
#include <Espalexa.h>
#include <ESP8266WebServer.h>

/// @brief Alexa é classe base para o driver de comunicação com a alexa, é um driver virtual a ser herdado;
class Alexa : public ApiDriver
{
private:
public:

    static Espalexa *getAlexa();
    int sensorId = -1;
    void beforeSetup() override;
    void afterSetup() override;
    virtual void loop() override;
    static int findAlexaPin(EspalexaDevice *d);

    static void init(Espalexa *alx = nullptr);
    static void handle();
    String getName();
    bool isLoop() override { return true; }
};

/// @brief AlexaLight liga um Sensor onoff controlado pela alexa
class AlexaLight : public Alexa
{

public:
    static void registerApi()
    {
        registerApiDriver("onoff", create);
    }
    static ApiDriver *create()
    {
#ifdef DEBUG_ALEXA
        Serial.println("AlexaLight create");
#endif
        return new AlexaLight();
    }
    void beforeSetup() override;

    static void onoffChanged(EspalexaDevice *d);
    void changed(const int pin, const long value) override;
};

/*
/// @brief Sensor dimmable para controle pela alexa
class AlexaDimmable : public Alexa
{
public:
    void beforeSetup() override;
    static void registerApi()
    {
        registerApiDriver("dim", create);
    }
    static ApiDriver *create()
    {
        return new AlexaDimmable();
    }
    static void dimmableChanged(EspalexaDevice *d)
    {
        int pin = Alexa::findAlexaPin(d);
        getInstanceOfProtocol()->writePin(pin, d->getValue());
    }
    void changed(const int pin, const long value) override;
};
*/

#endif