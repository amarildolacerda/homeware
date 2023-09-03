#pragma once

/*
ProtocolBase -> simplificar para usar em dispositivos mais enxutos (AVR)
Objetivo: implementar a base para ler escrever e disparar as trigger combinados com os drivers.

EM CONSTRUCAO - Não usado ainda.

*/



#include <ArduinoJson.h>
#include "drivers.h"


class ProtocolBase
{
public:
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);


JsonObject getMode()
{
    return config["mode"].as<JsonObject>();
}

/// @brief comutador de estado; se esta em HIGH mudar para LOW; se estiver em LOW mudar para HIGH
/// @param pin
/// @return
int switchPin(const int pin)
{
    Driver *drv = getDrivers()->findByPin(pin);
    if (drv)
    {
        int r = drv->readPin();
#ifdef DEBUG_ON
        debugf("{'switch':%i,'mode':'%s', 'actual':%i, 'to':%i}", pin, drv->_mode, r, (r > 0) ? LOW : HIGH);
#endif
        return drv->writePin((r > 0) ? LOW : HIGH);
    }
    return -1;
}

void checkTrigger(int pin, int value)
{
#ifndef ARDUINO_AVR
    try
    {
#endif
        String p = String(pin);
        JsonObject trig = getTrigger();
        if (trig.containsKey(p))
        {
            String pinTo = trig[p];
            if (!getStable().containsKey(p))
                return;

            int bistable = getStable()[p];
            int v = value;

#ifdef DEBUG_ON
            debug("bistable: ");
            debug((String)bistable);
            debug(" value: ");
            debug((String)v);
            debug(" pinTo: ");
            debugln(pinTo);
#endif
            if ((bistable == 2))
            {
                if (v == 1)
                    switchPin(pinTo.toInt());
                return;
            }
            else if ((bistable == 3))
            {
                if (v == 0)
                    switchPin(pinTo.toInt());
                return;
            }
            else if (pinTo.toInt() != pin)
            {
#ifdef DEBUG_ON
                debugf("{'trigger':%i, 'to':%i}", pin, value);
#endif
                writePin(pinTo.toInt(), v);
                return;
            }
        }
#ifndef ARDUINO_AVR
    }
    catch (char &e)
    {
    }
#endif
}


/// @brief gerar evento trigger caso o valor atribuido ao Pin foi alterado
/// @param pin
/// @param newValue
/// @param exectrigger
/// @return
bool initedValues = false;
bool pinValueChanged(const int pin, const int newValue, bool exectrigger)
{
    if (pinValue(pin) != newValue)
    {
        docPinValues[String(pin)] = newValue;
        getDrivers()->changed(pin, newValue);
#ifndef NO_API
        getApiDrivers().changed(pin, newValue);
#endif
        if (initedValues)
        { // na primeira carga, não dispara trigger - so pega o estado do PIN
#ifndef ARDUINO_AVR
            afterChanged(pin, newValue, getPinMode(pin));
#endif
            if (exectrigger)
                checkTrigger(pin, newValue);

#ifdef SCENE
            checkTriggerScene(pin, newValue);
#endif

            return true;
        }
        initedValues = true;
    }
    return false;
}


/// @brief Escreve uma valores para o Pin
/// @param pin - Pin alvo
/// @param value  - valor a escrever
/// @return valor atribuido
int writePin(const int pin, const int value)
{
    String mode = getMode()[String(pin)];

    if (mode != NULL)
    {
#ifdef DEBUG_ON
        debugf("write %s pin: %i set %i\r\n", mode, pin, value);
#endif
        Driver *drv = getDrivers()->findByPin(pin);
        if (drv)
        {
            drv->writePin(value);
        }
    }
    return value;
}
    


/// @brief faz a leitura do valor atribuido ao Pin - leitura lógica no Pin
/// @param pin
/// @param mode
/// @return
int readPin(const int pin, const String mode)
{
    Driver *drv;
    int newValue = 0;
    bool exectrigger = true;
    String m = mode;
    if (!m)
    {
        drv = getDrivers()->findByPin(pin);
        if (drv)
            m = drv->_mode;
    }
    else
        drv = getDrivers()->findByPin(pin);
    if (!drv)
        debugf("Pin: %i. Não tem um drive especifico: %s \r\n", pin, m);

    if (drv)
    {
        newValue = drv->readPin();
        // checa quem dispara trigger de alteração do estado
        // quando o driver esta  triggerEnabled=true significa que o driver dispara os eventos
        // por conta de regras especificas do driver;
        // se o driver for triggerEnabled=false, então quem dispara trigger é local, ja que o driver
        // nao tem esta funcionalidade
        exectrigger = !drv->triggerEnabled;
    }
    else
        newValue = digitalRead(pin);

    pinValueChanged(pin, newValue, exectrigger);

    return newValue;
}

};