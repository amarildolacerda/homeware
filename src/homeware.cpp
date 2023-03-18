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

#include <cloud.h>

#ifdef ALEXA
void alexaTrigger(int pin, int value);
#endif

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

void Homeware::prepare()
{
    Protocol::prepare();
    defaultConfig();
    restoreConfig();
}

#ifdef ESP32
void Homeware::setup(WebServer *externalServer)
#else

void Homeware::setup(ESP8266WebServer *externalServer)
#endif
{
    setServer(externalServer);

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
    }

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

#ifdef ALEXA
    alexaTrigger(pin, value);
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