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

#include <api.h>


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
    Protocol::afterChanged(pin, value, mode);
}

Homeware homeware;
#ifdef ESP32
WebServer server;
#else
ESP8266WebServer server;
#endif
#endif