#include <homeware.h>
#include <functions.h>
#include "options.h"

#include <ArduinoJson.h>

#ifdef ESP8266
#define USE_LittleFS
#endif

#include <FS.h>
#ifdef USE_LittleFS
#define SPIFFS LITTLEFS
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

#ifdef DHT_SENSOR
#include "DHTesp.h"
#endif

#ifdef MQTT
#include <mqtt.h>
#endif

#ifdef GROOVE_ULTRASONIC
#include <Ultrasonic.h>
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


String resources = "";
unsigned int sinric_count = 0;
bool inTelnet = false;

unsigned long loopEventMillis = millis();



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
    resources += "servidor comandos,";
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


#ifdef DHT_SENSOR
#define DHTTYPE DHT11
DHTesp dht;
bool dht_inited = false;
StaticJsonDocument<200> docDHT;

JsonObject readDht(const int pin)
{
    if (!dht_inited)
    {
        dht.setup(pin, DHTesp::AUTO_DETECT);
        dht_inited = true;
    }
    delay(dht.getMinimumSamplingPeriod());
    homeware.ledLoop(255); //(-1) desliga
    docDHT["temperature"] = dht.getTemperature();
    docDHT["humidity"] = dht.getHumidity();
    docDHT["fahrenheit"] = dht.toFahrenheit(dht.getTemperature());
    Serial.print("Temperatura: ");
    Serial.println(dht.getTemperature());
    return docDHT.as<JsonObject>();
}
#endif

void Homeware::begin()
{
    if (inited)
        return;

#ifdef ALEXA
    resources += "alexa,";
    setupSensores();
#endif
    setupServer();
#ifdef TELNET
    resources += "telnet,";
    setupTelnet();
#endif

#ifdef OTA

    resources += "OTA,";
    ElegantOTA.begin(server);
#endif
    server->begin();
#ifdef MQTT
    resources += "MQTT,";
    mqtt.setup(config["mqtt_host"], config["mqtt_port"], config["mqtt_prefix"], (config["mqtt_name"] != NULL) ? config["mqtt_name"] : config["label"]);
    mqtt.setUser(config["mqtt_user"], config["mqtt_password"]);
#endif
    resetDeepSleep();
    inited = true;
    Serial.println(resources);
}

#ifdef ESP32
void Homeware::setup(WebServer *externalServer)
#else
void Homeware::setup(ESP8266WebServer *externalServer)
#endif
{
#ifdef DHT_SENSOR
    resources += "dht,";
#endif
#ifdef ESP8266
    analogWriteRange(256);
#endif
    setServer(externalServer);

#ifdef ESP32
    if (!SPIFFS.begin())
#else
    if (!LittleFS.begin())
#endif
    {
        Serial.println("LittleFS mount failed");
    }

    defaultConfig();
    restoreConfig();
    if (config["label"])
    {
        hostname = config["label"].as<String>() + ".local";
    }
    setupPins();
}

void lerSerial()
{
    if (Serial.available() > 0)
    {
        char term = '\n';
        String cmd = Serial.readStringUntil(term);
        cmd.replace("\r", "");
        String rsp = homeware.doCommand(cmd);
        Serial.println(rsp);
#ifdef TELNET
        homeware.telnet.println("SERIAL " + cmd);
        homeware.telnet.println("RSP " + rsp);
#endif
    }
}

bool inLooping = false;
void Homeware::loop()
{
    if (inLooping)
        return;
    try
    {
        lerSerial();
        inLooping = true;
        if (!inited)
            begin();

        loopEvent();

#ifdef TELNET

        telnet.loop(); // se estive AP, pode conectar por telnet ou pelo browser.
#endif

        //=========================== usado somente quando conectado
        if (connected)
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
        }

        const int sleep = config["sleep"].as<String>().toInt();
        if (sleep > 0)
            doSleep(sleep);

        yield();
    }
    catch (int &e)
    {
    }
    inLooping = false;
}



#ifdef GROOVE_ULTRASONIC
unsigned long ultimo_ultrasonic = 0;
int grooveUltrasonic(int pin)
{
    if (millis() - ultimo_ultrasonic > (int(homeware.config["interval"]) * 5))
    {
        Ultrasonic ultrasonic(pin);
        long RangeInCentimeters;
        RangeInCentimeters = ultrasonic.MeasureInCentimeters(); // two measurements should keep an interval
        Serial.print(RangeInCentimeters);                       // 0~400cm
        Serial.println(" cm");
        ultimo_ultrasonic = millis();
        return roundf(RangeInCentimeters);
    }
    else
    {
        return int(docPinValues[String(pin)]);
    }
}
#endif

JsonObject Homeware::getValues()
{
    return docPinValues.as<JsonObject>();
}




unsigned int ultimaTemperatura = 0;
void Homeware::loopEvent()
{
    ledLoop(ledPin);
    try
    {
        unsigned long interval;
        try
        {
            interval = config["interval"].as<String>().toInt();
        }
        catch (char e)
        {
            interval = 500;
        }
        if (millis() - loopEventMillis > interval)
        {
            JsonObject mode = config["mode"];
            for (JsonPair k : mode)
            {
                readPin(String(k.key().c_str()).toInt(), k.value().as<String>());
            }
            loopEventMillis = millis();
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
        }
    }
    catch (const char *e)
    {
        print(String(e));
    }
    /*if (int(config["sleep"]) > 0)
    {
        unsigned tempo = int(config["sleep"]) * 1000000;
        ESP.deepSleep(tempo);
    }
    */
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
#ifdef ESP8266
    ESP.reset();
#else
    ESP.restart();
#endif
    delay(1000);
}


String Homeware::doCommand(String command)
{
    if (command.startsWith("SERIAL "))
    {
        command.replace("SERIAL ", "");
        Serial.print(command);
        return "OK";
    }
    try
    {
        resetDeepSleep();
        String *cmd = split(command, ' ');
        Serial.print("CMD: ");
        Serial.println(command);
#ifdef ESP8266
        if (cmd[0] == "format")
        {
            LittleFS.format();
            return "formated";
        }
        else
#endif
            if (cmd[0] == "open")
        {
            char json[1024];
            readFile(cmd[1], json, 1024);
            return String(json);
        }
        else if (cmd[0] == "help")
            return help();
        else if (cmd[0] == "show")
        {
            if (cmd[1] == "resources")
                return resources;
            else if (cmd[1] == "status")
            {
                return getStatus();
            }
            else if (cmd[1] == "config")
                return config.as<String>();
            else if (cmd[1] == "gpio")
                return showGpio();
            char buffer[128];
            char ip[20];
            IPAddress x = localIP();
            sprintf(ip, "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);
            // FSInfo fs_info;
            // LittleFS.info(fs_info);
            //  ADC_MODE(ADC_VCC);
            //'total': %d, 'free': %s
            //, fs_info.totalBytes, String(fs_info.totalBytes - fs_info.usedBytes)
            sprintf(buffer, "{ 'host':'%s' ,'version':'%s', 'name': '%s', 'ip': '%s'  }", hostname.c_str(), VERSION, config["label"].as<String>().c_str(), ip);
            return buffer;
        }
        else if (cmd[0] == "reset")
        {
            if (cmd[1] == "wifi")
            {
                homeware.resetWiFi();
                return "OK";
            }
            else if (cmd[1] == "factory")
            {
                defaultConfig();
                return "OK";
            }
            print("reiniciando...");
            delay(1000);
            // telnet.stop();
#ifdef ESP8266
            ESP.reset();
#else
            ESP.restart();
#endif
            return "OK";
        }
        else if (cmd[0] == "save")
        {
            return saveConfig();
        }
        else if (cmd[0] == "restore")
        {
            return restoreConfig();
        }
        else if (cmd[0] == "set")
        {
            if (cmd[2] == "none")
            {
                config.remove(cmd[1]);
            }
            else
            {
                config[cmd[1]] = cmd[2];
                printConfig();
            }
            return "OK";
        }
        else if (cmd[0] == "get")
        {
            return config[cmd[1]];
        }
        else if (cmd[0] == "pwm")
        {
            int pin = cmd[1].toInt();
            int timeout = 0;
            if (cmd[4] == "until" || cmd[4] == "timeout")
                timeout = cmd[5].toInt();
            if (cmd[2] == "set")
            {
                int value = cmd[3].toInt();
                int rsp = writePWM(pin, value, timeout);
                return String(rsp);
            }
            if (cmd[2] == "get")
            {
                int rsp = readPin(pin, "pwm");
                return String(rsp);
            }
        }
        else if (cmd[0] == "gpio")
        {
            int pin = cmd[1].toInt();
            String spin = cmd[1];
            if (cmd[2] == "none")
            {
                config["mode"].remove(cmd[1]);
                return "OK";
            }
#ifdef DHT_SENSOR
            else if (cmd[2] == "get" && getMode()[cmd[1]] == "dht")
            {
                JsonObject j = readDht(String(cmd[1]).toInt());
                String result;
                serializeJson(j, result);
                return result;
            }
#endif
            else if (cmd[2] == "get")
            {
                int v = readPin(pin, "");
                return String(v);
            }
            else if (cmd[2] == "set")
            {
                int v = cmd[3].toInt();
                writePin(pin, v);
                return String(v);
            }
            else if (cmd[2] == "mode")
            {
                initPinMode(pin, cmd[3]);
                return "OK";
            }
            else if (cmd[2] == "device")
            {
                JsonObject devices = getDevices();
                devices[spin] = cmd[3];
                if (String("motion,doorbell").indexOf(cmd[3]))
                    getMode()[spin] = "in";
                if (String("ldr,dht").indexOf(cmd[3]))
                    getMode()[spin] = cmd[3];
                return "OK";
            }
            else if (cmd[2] == "sensor")
            {
                JsonObject devices = getSensors();
                devices[spin] = cmd[3];
                return "OK";
            }
            else if (cmd[2] == "default")
            {
                JsonObject d = getDefaults();
                d[spin] = cmd[3];
                return "OK";
            }
            else if (cmd[2] == "trigger")
            {
                if (!cmd[4])
                    return "cmd incompleto";
                JsonObject trigger = getTrigger();
                trigger[spin] = cmd[3];

                // 0-monostable 1-monostableNC 2-bistable 3-bistableNC
                getStable()[spin] = (cmd[4] == "bistable" ? 2 : 0) + (cmd[4].endsWith("NC") ? 1 : 0);

                return "OK";
            }
        }
        return "invalido";
    }
    catch (const char *e)
    {
        return String(e);
    }
}


void Homeware::printConfig()
{

    serializeJson(config, Serial);
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
    int pin = homeware.findPinByMode("dht");
    if (pin < 0)
        return;
    JsonObject r = readDht(pin);
    String id = homeware.getSensors()[String(pin)];
    if (!id)
        return;
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
    debug("ativando sensores");
    JsonObject devices = getDevices();

    Serial.println("\r\nDevices\r\n============================\r\n");
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
            debug("ativando Doorbell");
            SinricProDoorbell &myDoorbell = SinricPro[getSensors()[k.key().c_str()]];
            myDoorbell.onPowerState(onSinricPowerState);
            sinric_count += 1;
        }
        else if (sValue.startsWith("motion") && getSensors()[k.key().c_str()])
        {
            debug("ativando PIR");
            SinricProMotionsensor &myMotionsensor = SinricPro[getSensors()[k.key().c_str()]];
            myMotionsensor.onPowerState(onSinricPowerState);
            sinric_count += 1;
        }
        else if (sValue.startsWith("dht") && getSensors()[k.key().c_str()])
        {
            debug("ativando DHT11");
            SinricProTemperaturesensor &mySensor = SinricPro[getSensors()[k.key().c_str()]];
            mySensor.onPowerState(onSinricDHTPowerState);
            sinric_count += 1;
        }
#endif
    }
    Serial.println("============================");

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
}
#endif

void Homeware::setupTelnet()
{
    Serial.println("carregando TELNET");
    telnet.onConnect([](String ip)
                     {
        inTelnet = true;
        homeware. resetDeepSleep();
        Serial.print("- Telnet: ");
        Serial.print(ip);
        Serial.println(" conectou");
        homeware.telnet.println("\nhello " + homeware.telnet.getIP());
        homeware.telnet.println("(Use ^] + q  para desligar.)"); });
    telnet.onInputReceived([](String str)
                           { 
                            Serial.println("TEL: "+str);
                            if (str=="exit"){
                                inTelnet = false;
                                homeware.telnet.disconnectClient();
                            }
                            else {
                                    homeware.resetDeepSleep();
                                    homeware.print(homeware.doCommand(str));} });

    Serial.print("- Telnet ");
    if (telnet.begin())
    {
        Serial.println("running");
    }
    else
    {
        Serial.println("error.");
        errorMsg("Will reboot...");
    }
}
void Homeware::errorMsg(String msg)
{
    Serial.println(msg);
    telnet.println(msg);
}

IPAddress Homeware::localIP()
{
    return WiFi.localIP();
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
    checkTrigger(pin, value);
#ifdef ALEXA
    alexaTrigger(pin, value);
#endif

#ifdef SINRIC
    if (sinricActive)
        sinricTrigger(pin, value);
#endif
}

int Homeware::readPin(const int pin, const String mode)
{
    String md = mode;
    int newValue = 0;
    if (!md || md == "")
        md = getMode()[String(pin)].as<String>();
    if (md == "out")
    {
        newValue = Protocol::readPin(pin, md);
    }
#ifdef DHT_SENSOR
    if (md == "dht")
    {
      newValue =readDht(pin)["temperature"];
    }
#endif
#ifdef GROOVE_ULTRASONIC
    else if (md == "gus")
    {
      // groove ultrasonic
      newValue = grooveUltrasonic(pin);
    }
#endif
    else
    {
      newValue = Protocol::readPin(pin, md);
    }

    pinValueChanged(pin, newValue);

    return newValue;
}

Homeware homeware;
#ifdef ESP32
WebServer server;
#else
ESP8266WebServer server;
#endif
