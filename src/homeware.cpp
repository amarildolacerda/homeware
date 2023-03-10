#include "options.h"
#ifdef ARDUINO_AVR

#else

#include <homeware.h>
#include <functions.h>

#include "drivers.h"

#include <ArduinoJson.h>

#include <FS.h>
#ifdef LITTLEFs
#include <LittleFS.h>
#else
#include <SPIFFS.h>
#endif

#include <Arduino.h>

#ifdef OTA
#include <ElegantOTA.h>
#endif

#ifdef ESP32
#include "WiFi.h"
#else
#include <ESP8266WiFi.h>
#endif

#ifdef MQTT
#include <mqtt.h>
#endif

#ifdef SINRIC
#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#include "SinricProTemperaturesensor.h"
#include "SinricProDoorbell.h"

#endif

#ifdef ALEXA
void alexaTrigger(int pin, int value);
#endif

#ifdef SINRIC
bool sinricActive = false;
void sinricTrigger(int pin, int value);
void sinricTemperaturesensor();

#endif

unsigned int sinric_count = 0;

void linha()
{
    Serial.println("-------------------------------");
}

#ifdef ESP32
void Homeware::setServer(WebServer *externalServer)
#else
void Homeware::setServer(ESP8266WebServer *externalServer)
#endif
{
    server = externalServer;
}

void Homeware::setupServer()
{
    resources += "cmd,";
    server->on("/cmd", []()
               {
        if (homeware.server->hasArg("q"))
        {
            String cmd = homeware.server->arg("q");
            String rt = homeware.doCommand(cmd);
            if (!rt.startsWith("{"))
               rt = "\""+rt+"\"";
            homeware.server->send(200, "application/json", "{\"result\":" + rt + "}");
            return;
        } });
}

void Homeware::afterBegin()
{
#ifdef ALEXA
    resources += "alexa,";
    setupSensores();
#endif
    setupServer();

#ifdef OTA
    resources += "OTA,";
    ElegantOTA.setID(hostname.c_str());
    ElegantOTA.begin(server);
#endif
    server->begin();
#ifdef MQTT
    resources += "MQTT,";
    mqtt.setup(config["mqtt_host"], config["mqtt_port"], config["mqtt_prefix"], (config["mqtt_name"] != NULL) ? config["mqtt_name"] : config["label"]);
    mqtt.setUser(config["mqtt_user"], config["mqtt_password"]);
#endif
    resetDeepSleep();
    Protocol::afterBegin();
}

#ifdef ESP32
void Homeware::setup(WebServer *externalServer)
#else
void Homeware::setup(ESP8266WebServer *externalServer)
#endif
{
    prepare();
    setServer(externalServer);
    defaultConfig();
    restoreConfig();


    Protocol::setup();

    if (config["label"])
    {
        hostname = config["label"].as<String>() + ".local";
    }
    setupPins();
}

unsigned int ultimaTemperatura = 0;
void Homeware::afterLoop()
{
#ifdef MQTT
    mqtt.loop();
#endif
#ifdef ALEXA
    alexa.loop();
#endif

#ifdef SINRIC
    if (sinric_count > 0 && sinricActive)
        SinricPro.handle();
    if (sinric_count > 0 && !sinricActive)
        setLedMode(4);

#endif

#ifdef SINRIC
    if (millis() - ultimaTemperatura > 60000)
    {
        if (sinric_count > 0 && sinricActive && !inTelnet)
        {
            sinricTemperaturesensor();
            ultimaTemperatura = millis();
        }
    }
#endif
    Protocol::afterLoop();
}

void Homeware::resetWiFi()
{
    HomewareWiFiManager wifiManager;
    WiFi.mode(WIFI_STA);
    WiFi.persistent(true);
    WiFi.disconnect(true);
    WiFi.persistent(false);
    // wifiManager.resetSettings();
    config.remove("ssid");
    config.remove("password");
    saveConfig();
    reset();
    delay(1000);
}

void Homeware::reset()
{
    Serial.println("Reset call");
#ifdef ESP8266
    ESP.reset();
#else
    ESP.restart();
#endif
}


#ifdef ESP32
const char *Homeware::getChipId()
{
    return ESP.getChipModel();
}
#else
uint32_t Homeware::getChipId()
{
    return ESP.getChipId();
}
#endif

#ifdef SINRIC

void sinricTrigger(int pin, int value)
{
    String id = homeware.getSensors()[String(pin)];
    if (id)
    {
        String sType = homeware.getDevices()[String(pin)];
        if (sType.startsWith("motion"))
        {
            bool bValue = value > 0;
            Serial.printf("Motion %s\r\n", bValue ? "detected" : "not detected");
            SinricProMotionsensor &myMotionsensor = SinricPro[id]; // get motion sensor device
            myMotionsensor.sendPowerStateEvent(bValue);
            myMotionsensor.sendMotionEvent(bValue);
        }
        if (sType.startsWith("doorbell"))
        {
            bool bValue = value > 0;
            Serial.printf("Doorbell %s\r\n", bValue ? "detected" : "not detected");
            SinricProDoorbell &myDoorbell = SinricPro[id];
            myDoorbell.sendPowerStateEvent(bValue);
            if (bValue)
                myDoorbell.sendDoorbellEvent();
        }
    }
}
#endif

#ifdef ALEXA
void alexaTrigger(int pin, int value)
{
    for (int i = 0; i < ESPALEXA_MAXDEVICES; i++)
    {
        EspalexaDevice *d = homeware.alexa.getDevice(i);
        if (d != nullptr && d->getValue() != value)
        {
            int id = d->getId();
            int index = 0;
            for (JsonPair k : homeware.getDevices())
            {
                String sType = k.value().as<String>();
                if (sType != "onoff" && sType != "dimmable")
                    continue;
                if (index == id)
                {
                    if (String(pin) == k.key().c_str())
                    {
                        d->setState(value != 0);
                        if (value > 0 && sType == "onoff")
                        {
                            d->setValue(value);
                            d->setPercent((value > 0) ? 100 : 0);
                        }
                        else if (sType.startsWith("dimmable"))
                        {
                            d->setValue(value);
                            d->setPercent((value / 1024) * 100);
                        }
                        homeware.debug(stringf("Alexa Pin %d setValue(%d)", pin, value));
                    }
                }
                index += 1;
            }
        }
    }
}
int findAlexaPin(EspalexaDevice *d)
{

    if (d == nullptr)
        return -1;       // this is good practice, but not required
    int id = d->getId(); // * base 0
    Serial.printf("\r\nId: %d \r\n", id);
    JsonObject devices = homeware.getDevices();
    int index = 0;
    int pin = -1;
    for (JsonPair k : devices)
    {
        homeware.debug(stringf("Alexa: %s %s %d", k.key().c_str(), k.value(), d->getValue()));
        if (index == (id))
        {
            pin = String(k.key().c_str()).toInt();
            break;
        }
        index += 1;
    }
    return pin;
}

void onoffChanged(EspalexaDevice *d)
{
    int pin = findAlexaPin(d);
    bool value = d->getState();
    if (pin > -1)
    {
        homeware.writePin(pin, (value) ? HIGH : LOW);
    }
}

void dimmableChanged(EspalexaDevice *d)
{
    int pin = findAlexaPin(d);
    if (pin > -1)
    {
        homeware.writePin(pin, d->getValue());
    }
}

#ifdef SINRIC
int findSinricPin(String id)
{
    for (JsonPair k : homeware.getSensors())
    {
        if (k.value().as<String>() == id)
            return String(k.key().c_str()).toInt();
    }
    return -1;
}

int ultimaTemperaturaAferida = 0;
void sinricTemperaturesensor()
{
#if defined(DRIVERS_ENABLED) && defined(DHT_SENSOR)
    int pin = homeware.findPinByMode("dht");
    if (pin < 0)
        return;
    String id = homeware.getSensors()[String(pin)];
    if (!id)
        return;
    Driver *drv = getDrivers()->findByMode("dht");
    if (!drv)
        return;
    JsonObject r = drv->readStatus(pin);
    float t = r["temperature"];
    float h = r["humidity"];
    if (ultimaTemperaturaAferida != t)
    {
        ultimaTemperaturaAferida = t;
        SinricProTemperaturesensor &mySensor = SinricPro[id]; // get temperaturesensor device
        mySensor.sendTemperatureEvent(t, h);                  // send event
        String result;
        serializeJson(r, result);
        Serial.println(result);
    }
#endif
}

bool onSinricDHTPowerState(const String &deviceId, bool &state)
{
    Serial.printf("PowerState turned %s  \r\n", state ? "on" : "off");
    sinricTemperaturesensor();
    return true; // request handled properly
}

bool onSinricPowerState(const String &deviceId, bool &state)
{
    Serial.printf("Device %s turned %s (via SinricPro) \r\n", deviceId.c_str(), state ? "on" : "off");
    int pin = findSinricPin(deviceId.c_str());
    if (pin > -1)
        homeware.writePin(pin, state ? HIGH : LOW);
    return true; // req
}

#endif

void Homeware::setupSensores()
{
    Serial.print("Ativando sensores: ");
    JsonObject devices = getDevices();
    alexa.setFriendlyName(config["label"]);
    for (JsonPair k : devices)
    {
        debug(stringf("%s is %s\r\n", k.key().c_str(), k.value().as<String>()));
        String sName = config["label"];
        sName += "-";
        sName += k.key().c_str();
        String sValue = k.value().as<String>();
        if (sValue == "onoff")
        {
            alexa.addDevice(sName, onoffChanged, EspalexaDeviceType::onoff); // non-dimmable device
        }
        else if (sValue.startsWith("dim"))
        {
            alexa.addDevice(sName, dimmableChanged, EspalexaDeviceType::dimmable); // non-dimmable device
        }
#ifdef SINRIC
        else if (sValue.startsWith("doorbell") && getSensors()[k.key().c_str()])
        {
            debug("Ativando Doorbell");
            SinricProDoorbell &myDoorbell = SinricPro[getSensors()[k.key().c_str()]];
            myDoorbell.onPowerState(onSinricPowerState);
            sinric_count += 1;
        }
        else if (sValue.startsWith("motion") && getSensors()[k.key().c_str()])
        {
            debug("Ativando PIR");
            SinricProMotionsensor &myMotionsensor = SinricPro[getSensors()[k.key().c_str()]];
            myMotionsensor.onPowerState(onSinricPowerState);
            sinric_count += 1;
        }
        else if (sValue.startsWith("dht") && getSensors()[k.key().c_str()])
        {
            debug("Ativando DHT11");
            SinricProTemperaturesensor &mySensor = SinricPro[getSensors()[k.key().c_str()]];
            mySensor.onPowerState(onSinricDHTPowerState);
            sinric_count += 1;
        }
#endif
    }

#ifdef SINRIC
    resources += "SINRIC,";
    if (sinric_count > 0)
    {
        SinricPro.onConnected([]()
                              {            
                                sinricActive = true;               
                                homeware.resetDeepSleep();
                                Serial.printf("Connected to SinricPro\r\n"); });
        SinricPro.onDisconnected([]()
                                 { 
                                    sinricActive = false; 
                                    Serial.printf("Disconnected from SinricPro\r\n"); });
        SinricPro.begin(config["app_key"], config["app_secret"]);
        sinricActive = true;
    }
#endif
    alexa.begin(server);
    Serial.println("OK");
}
#endif

String IPAddressToString(const IPAddress x)
{
    char ip[20];
    sprintf(ip, "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);
    return ip;
}

String Homeware::localIP()
{
    return IPAddressToString(WiFi.localIP());
}

// ============= revisados para ficar aqui mesmo
void Homeware::afterChanged(const int pin, const int value, const String mode)
{

#ifdef DHT_SENSOR
#ifdef SINRIC
    if (mode == "dht")
        sinricTemperaturesensor();
#endif
#endif
#ifdef ALEXA
    alexaTrigger(pin, value);
#endif

#ifdef SINRIC
    if (sinricActive)
        sinricTrigger(pin, value);
#endif
    Protocol::afterChanged(pin, value, mode);
}


Homeware homeware;
#ifdef ESP32
WebServer server;
#else
ESP8266WebServer server;
#endif
#endif