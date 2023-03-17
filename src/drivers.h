
/*

Sim, é possível modificar o código para permitir que as classes filhas de Driver se auto-registrem na lista `modeDriverList`. Aqui está um exemplo de como isso pode ser feito:

```c++
class Driver {
  public:
    virtual void run() = 0;
};


class DriverA : public Driver {
  public:
    static void registerMode() {
      ::registerMode("A", create);
    }

    static Driver* create() {
      return new DriverA();
    }

    void run() {
      // código para executar o driver A
    }
};

class DriverB : public Driver {
  public:
    static void registerMode() {
      ::registerMode("B", create);
    }

    static Driver* create() {
      return new DriverB();
    }

    void run() {
      // código para executar o driver B
    }
};

Driver* drivers[10];
int numDrivers = 0;

void createDriver(String mode) {
  for (int i = 0; i < numModes; i++) {
    if (mode == modeDriverList[i].mode) {
      drivers[numDrivers] = modeDriverList[i].create();
      numDrivers++;
      break;
    }
  }
}

void setup() {
  DriverA::registerMode();
  DriverB::registerMode();

  createDriver("A");
  createDriver("B");
}

void loop() {
  for (int i = 0; i < numDrivers; i++) {
    drivers[i]->run();
  }
}
```

Neste exemplo, cada classe filha de `Driver` possui um método estático `registerMode` que chama a função global `registerMode` para registrar a classe na lista `modeDriverList`. O método `setup` chama os métodos `registerMode` de cada classe filha para registrar todas as classes disponíveis.

Espero que isso ajude! 😊

*/

#ifndef drivers_h
#define drivers_h

#include <Arduino.h>
#include <ArduinoJson.h>

#include "protocol.h"
#include "functions.h"
class Driver
{
private:
    String _mode;
    int _pin = -1;

protected:
    typedef void (*callbackFunction)(String mode, int pin, int value);
    long v1 = 0;
    callbackFunction triggerCallback;
    callbackFunction triggerOkState;

public:
    /// @brief triggerEnabled true indica que o driver define o momento para dispara trigger
    bool triggerEnabled = false;

    bool active = false;
    virtual void setV1(long x) { v1 = x; }
    virtual void setPinMode(int pin)
    {
        setPin(pin);
        active = true;
    }

    virtual void setup()
    {
#ifndef ARDUINO_AVR
        if (getProtocol()->resources.indexOf(_mode) < 0)
            getProtocol()->resources += _mode + ",";
#endif
    };
    virtual void loop(){};

    virtual int readPin(const int pin)
    {
        return -1;
    }
    void setMode(String md)
    {
        _mode = md;
    }
    String getMode()
    {
        return _mode;
    }
    Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }
    void setPin(const int pin)
    {
        _pin = pin;
    }
    virtual int writePin(const int pin, const int value)
    {
        return value;
    }
    virtual bool isGet() { return true; }
    virtual bool isSet() { return false; }
    virtual String doCommand(const String command)
    {
        return "NAK";
    }
    virtual JsonObject readStatus(const int pin)
    {
        int rsp = readPin(pin);
        DynamicJsonDocument json = DynamicJsonDocument(128);
        json["result"] = rsp;
        return json.as<JsonObject>();
    }
    virtual bool isStatus() { return false; }
    virtual bool isCommand() { return false; }
    virtual bool isLoop() { return false; }

    virtual void changed(const int pin, const int value){};

    int getPin()
    {
        return _pin;
    };

    void setTriggerEvent(callbackFunction callback)
    {
        triggerCallback = callback;
    }
    void setTriggerOkState(callbackFunction callback)
    {
        triggerOkState = callback;
    }
};

/// -----------------------------------------------------
/// Registra os tipos de drivers disponiveis para uso
/// -----------------------------------------------------
struct ModeDriverPair
{
    String mode;
    Driver *(*create)();
};

void registerDriverMode(String mode, Driver *(*create)());
Driver *createByDriverMode(const String mode, const int pin);

// -----------------------------------------------
//  Coleção de Drivers
// -----------------------------------------------
class Drivers
{

private:
    int count = 0;
    int next = 0;

public:
    Driver *items[32];
    Driver *initPinMode(const String mode, const int pin)
    {
        Driver *old = findByPin(pin);
        if (old && old->getMode() == mode)
            return old;
        Driver *nwer = createByDriverMode(mode, pin);
        if (nwer)
        {
            deleteByPin(pin);

            nwer->setMode(mode);
            nwer->setPin(pin);
            items[count++] = nwer;
#ifdef DEBUG_ON
            Serial.printf("Criou %s em %i\r\n", mode, pin);
#endif
            return nwer;
            // add(nwer);
        }
        return nullptr;
    }
    template <class T>
    void add(T *driver)
    {
        items[count++] = driver;
    }
    Protocol *getProtocol()
    {
        return getInstanceOfProtocol();
    }

    void setup()
    {
        for (auto *drv : items)
        {
            if (drv)
                drv->setup();
        }
    }
    void deleteByPin(const int pin)
    {
        int idx = indexOf(pin);
        if (idx > -1)
        {
            items[idx] = nullptr;
            for (int i = idx; i < count - 1; i++)
            {
                items[i] = items[i + 1];
            }
            count--;
        }
    }
    int indexOf(const int pin)
    {
        int idx = 0;
        for (auto *drv : items)
        {
            if (drv)
            {
                if (drv->getPin() == pin)
                {
                    return idx;
                }
            }
            idx++;
        }
        return -1;
    }

    Driver *findByMode(String mode)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->getMode().equals(mode))
                {
                    return drv;
                }
            }
        return nullptr;
    }
    Driver *findByPin(const int pin)
    {
        for (auto *drv : items)
            if (drv)
            {
                if (drv->getPin() == pin)
                {
                    return drv;
                }
            }
        return nullptr;
    }
    void changed(const int pin, const int value)
    {
        auto *drv = findByPin(pin);
        if (drv && drv->active)
            drv->changed(pin, value);
    }
    void loop()
    {
        for (auto *drv : items)
            if (drv && drv->active && drv->isLoop())
            {
                drv->loop();
            }
    }
};

Drivers *getDrivers();
#endif