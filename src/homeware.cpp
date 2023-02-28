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

#ifdef MQTT
#include <mqtt.h>
#endif

#ifdef GROOVE_ULTRASONIC
#include <Ultrasonic.h>
#endif

#ifdef SINRIC
#include "SinricPro.h"
#include "SinricProMotionsensor.h"
#endif

#ifdef ALEXA
void alexaTrigger(int pin, int value);
#endif

#ifdef SINRIC
void sinricTrigger(int pin, int value);
#endif

StaticJsonDocument<256> docPinValues;
String resources = "";
int sinric_count = 0;

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

unsigned long sleeptmp = millis() + timeoutDeepSleep;
void Homeware::resetDeepSleep(const unsigned int t)
{
    unsigned int v = millis() + t;
    if (v > sleeptmp)
    {
        sleeptmp = v;
        Serial.println(sleeptmp);
    }
}

void doSleep(const int tempo)
{
    if (millis() > sleeptmp)
    {
        Serial.print("Sleeping: ");
        Serial.println(tempo);
        ESP.deepSleep(tempo * 1000 * 1000);
    }
}

String Homeware::restoreConfig()
{
    String rt = "nao restaurou config";
    Serial.println("");
    // linha();
    try
    {
        String old = config.as<String>();
#ifdef ESP32
        File file = SPIFFS.open("/config.json", "r");
#else
        File file = LittleFS.open("/config.json", "r");
#endif
        if (!file)
            return "erro ao abrir /config.json";
        String novo = file.readString();
        DynamicJsonDocument doc = DynamicJsonDocument(1024);
        auto error = deserializeJson(doc, novo);
        if (error)
        {
            config.clear();
            deserializeJson(config, old);
            return "Error: " + String(novo);
        }
        for (JsonPair k : doc.as<JsonObject>())
        {
            String key = k.key().c_str();
            config[key] = doc[key]; // pode ter variaveis gravadas que nao tem no default inicial; ex: ssid
        }
        serializeJson(config, Serial);
        Serial.println("");
        rt = "OK";
        // linha();
    }
    catch (const char *e)
    {
        return String(e);
    }
    Serial.println("");
    return rt;
}

void Homeware::begin()
{
    if (inited)
        return;

#ifdef ALEXA
    resources += "alexa,";
    setupAlexa();
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
void Homeware::loop()
{
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
        if (sinric_count > 0)
            SinricPro.handle();
#endif
    }

    const int sleep = config["sleep"].as<String>().toInt();
    if (sleep > 0)
        doSleep(sleep);

    yield();
}

DynamicJsonDocument baseConfig()
{
    DynamicJsonDocument config = DynamicJsonDocument(1024);
    config["label"] = LABEL;
    config["board"] = "esp8266";
    config.createNestedObject("mode");
    config.createNestedObject("trigger");
    config.createNestedObject("stable");
    config.createNestedObject("device");
    config.createNestedObject("sensor");
    config.createNestedObject("default");
    config["debug"] = homeware.inDebug ? "on" : "off";
    config["interval"] = "500";
    config["adc_min"] = "125";
    config["adc_max"] = "126";
    config["sleep"] = "0";

#ifdef MQTT
    config["mqtt_host"] = "none"; //"test.mosquitto.org";
    config["mqtt_port"] = 1883;
    config["mqtt_user"] = "homeware";
    config["mqtt_password"] = "123456780";
    config["mqtt_interval"] = 1;
    config["mqtt_prefix"] = "mesh";
#endif
    config["ap_ssid"] = "none";
    config["ap_password"] = "123456780";
#ifdef SINRIC
    config["app_key"] = "";
    config["app_secret"] = "";
#endif
    return config;
}
void Homeware::defaultConfig()
{
    DynamicJsonDocument doc = baseConfig();
    for (JsonPair k : doc.as<JsonObject>())
    {
        String key = k.key().c_str();
        config[key] = doc[key];
    }
}

String Homeware::saveConfig()
{ // TODO: ajustar para gravar somente os alterado e reduzir uso de espaço.
    String rsp = "OK";
    DynamicJsonDocument doc = baseConfig();
    DynamicJsonDocument base = DynamicJsonDocument(SIZE_BUFFER);

    for (JsonPair k : config.as<JsonObject>())
    {
        String key = k.key().c_str();
        if (doc[key] != config[key])
            base[key] = config[key];
    }

    base["debug"] = inDebug ? "on" : "off"; // volta para o default para sempre ligar com debug desabilitado
    serializeJson(base, Serial);
#ifdef ESP32
    File file = SPIFFS.open("/config.json", "w");
#else
    File file = LittleFS.open("/config.json", "w");
#endif
    if (serializeJson(base, file) == 0)
        rsp = "não gravou /config.json";
    file.close();
    return rsp;
}

void Homeware::initPinMode(int pin, const String m)
{
    if (m == "in")
        pinMode(pin, INPUT);
    else if (m == "out")
        pinMode(pin, OUTPUT);
    JsonObject mode = getMode();
    mode[String(pin)] = m;
}

void Homeware::setupPins()
{
    Serial.println("configurando os pinos");
    JsonObject mode = config["mode"];
    for (JsonPair k : mode)
    {
        int pin = String(k.key().c_str()).toInt();
        initPinMode(pin, k.value().as<String>());
        int trPin = getTrigger()[String(pin)];
        if (trPin)
        {
            initPinMode(trPin, "out");
        }
    }
    for (JsonPair k : getDefaults())
    {
        writePin(String(k.key().c_str()).toInt(), k.value().as<String>().toInt());
    }
}

JsonObject Homeware::getTrigger()
{
    return config["trigger"].as<JsonObject>();
}
JsonObject Homeware::getDevices()
{
    return config["device"].as<JsonObject>();
}
JsonObject Homeware::getSensors()
{
    return config["sensor"].as<JsonObject>();
}

JsonObject Homeware::getMode()
{
    return config["mode"].as<JsonObject>();
}

JsonObject Homeware::getStable()
{
    return config["stable"].as<JsonObject>();
}

int Homeware::switchPin(const int pin)
{
    String mode = getMode()[String(pin)];
    if (mode == "out" || mode == "lc")
    {
        int r = readPin(pin, mode);
        return writePin(pin, 1 - r);
    }
    else
    {
        int r = readPin(pin, mode);
        return writePin(pin, (r > 0) ? 0 : r);
    }
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

int Homeware::writePin(const int pin, const int value)
{
    String mode = getMode()[String(pin)];
    int v = value;
    if (mode != NULL)
        if (mode == "pwm")
            analogWrite(pin, value);
#ifdef GROOVE_ULTRASONIC
        else if (mode == "gus")
        {
            int v = grooveUltrasonic(pin);
        }
#endif
        else if (mode == "adc")
            analogWrite(pin, value);
        else if (mode == "lc")
        {
            byte relON[] = {0xA0, 0x01, 0x01, 0xA2}; // Hex command to send to serial for open relay
            byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1};
            if (value == 0)
            {
                Serial.write(relOFF, sizeof(relOFF));
            }
            else
                Serial.write(relON, sizeof(relON));
        }

        else
        {
            digitalWrite(pin, value);
        }
    else
    {
        // initPinMode(pin, "out");
        digitalWrite(pin, value);
    }
    Serial.println(stringf("writePin: %d value: %d", pin, value));
    docPinValues[String(pin)] = v;
    return value;
}

JsonObject Homeware::getValues()
{
    return docPinValues.as<JsonObject>();
}

int Homeware::readPin(const int pin, const String mode)
{
    String md = mode;
    if (!md)
        md = getMode()[String(pin)].as<String>();
    int oldValue = docPinValues[String(pin)];
    int newValue = 0;
    if (md == "pwm")
    {
        newValue = analogRead(pin);
    }
#ifdef GROOVE_ULTRASONIC
    else if (md == "gus")
    {
        // groove ultrasonic
        newValue = grooveUltrasonic(pin);
    }
#endif
    else if (md == "adc")
    {
        newValue = analogRead(pin);
    }
    else if (md == "lc")
    {
        newValue = docPinValues[String(pin)];
    }
    else if (md == "ldr")
    {
        newValue = getAdcState(pin);
    }
    else
        newValue = digitalRead(pin);

    debug(stringf("readPin %d from %d to %d \r\n", pin, oldValue, newValue));

    if (oldValue != newValue)
    {
        char buffer[32];
        sprintf(buffer, "pin %d : %d ", pin, newValue);
        debug(buffer);
        docPinValues[String(pin)] = newValue;
        checkTrigger(pin, newValue);
#ifdef ALEXA
        alexaTrigger(pin, newValue);
#endif

#ifdef SINRIC
        sinricTrigger(pin, newValue);
#endif
    }

    return newValue;
}

void Homeware::checkTrigger(int pin, int value)
{
    String p = String(pin);
    JsonObject trig = getTrigger();
    if (trig.containsKey(p))
    {
        String pinTo = trig[p];
        if (!getStable().containsKey(p))
            return;

        int bistable = getStable()[p];
        int v = value;

        if ((bistable == 2 || bistable == 3))
        {
            if (v == 0)
                return; // so aciona quando v for 1
            switchPin(pinTo.toInt());
            return;
        }

        Serial.println(stringf("pin %s trigger %s to %d stable %d \r\n", p, pinTo, v, bistable));
        if (pinTo.toInt() != pin)
            writePin(pinTo.toInt(), v);
    }
}

const char HELP[] =
    "set board <esp8266>\r\n"
    "show config\r\n"
    "gpio <pin> mode <in,out,adc,lc,ldr>\r\n"
    "gpio <pin> defult <n>(usado no setup)\r\n"
    "gpio <pin> mode gus (groove ultrasonic)\r\n"
    "gpio <pin> trigger <pin> [monostable,bistable]\r\n"
    "gpio <pin> device <onoff,dimmable> (usado na alexa)\r\n"
    "set app_key <x> (SINRIC)\r\n"
    "set app_secret <x> (SINRIC)\r\n"
    "gpio <pin> sensor <deviceId> (SINRIC)\r\n"
    "gpio <pin> get\r\n"
    "gpio <pin> set <n>\r\n"
    "set interval 50\r\n"
    "set adc_min 511\r\n"
    "set adc_max 512\r\n";

String Homeware::help()
{
    String s = FPSTR(HELP);
    return s;
}

bool Homeware::readFile(String filename, char *buffer, size_t maxLen)
{
#ifdef ESP32

    File file = SPIFFS.open(filename, "r");
#else
    File file = LittleFS.open(filename, "r");
#endif
    if (!file)
    {
        return false;
    }
    size_t len = file.size();
    if (len > maxLen)
    {
        len = maxLen;
    }
    file.readBytes(buffer, len); //(buffer, len);
    buffer[len] = 0;
    file.close();
    return true;
}

unsigned long loopEventMillis = millis();
void Homeware::loopEvent()
{
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

String *split(String s, const char delimiter)
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < s.length(); i++)
    {
        if (s[i] == delimiter)
        {
            count++;
        }
    }

    String *words = new String[count + 1];
    unsigned int wordCount = 0;

    for (unsigned int i = 0; i < s.length(); i++)
    {
        if (s[i] == delimiter)
        {
            wordCount++;
            continue;
        }
        words[wordCount] += s[i];
    }
    // words[count+1] = 0;

    return words;
}

String Homeware::print(String msg)
{
    Serial.print("RSP: ");
    Serial.println(msg);
#ifdef TELNET
    telnet.println(msg);
#endif
    return msg;
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
            else if (cmd[1] == "config")
                return config.as<String>();
            else if (cmd[1] == "gpio")
                return showGpio();
            char buffer[32];
            char ip[20];
            IPAddress x = localIP();
            sprintf(ip, "%d.%d.%d.%d", x[0], x[1], x[2], x[3]);
            // FSInfo fs_info;
            // LittleFS.info(fs_info);
            //  ADC_MODE(ADC_VCC);
            //'total': %d, 'free': %s
            //, fs_info.totalBytes, String(fs_info.totalBytes - fs_info.usedBytes)
            sprintf(buffer, "{ 'host':'%s' ,'version':'%s', 'name': '%s', 'ip': '%s'  }", hostname.c_str(), VERSION, config["label"].as<String>(), ip);
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
        else if (cmd[0] == "gpio")
        {
            int pin = cmd[1].toInt();
            String spin = cmd[1];
            if (cmd[2] == "none")
            {
                config["mode"].remove(cmd[1]);
                return "OK";
            }
            else if (cmd[2] == "get")
            {
                int v = digitalRead(pin);
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

JsonObject Homeware::getDefaults()
{
    return config["default"];
}

const String optionStable[] = {"monostable", "monostableNC", "bistable", "bistableNC"};
String Homeware::showGpio()
{
    String r = "[";
    for (JsonPair k : getMode())
    {
        String sPin = String(k.key().c_str());
        String s = "'gpio': ";
        s += sPin;
        s += ", 'mode': '";
        s += k.value().as<String>();
        s += "'";

        JsonObject trig = getTrigger();
        if (trig.containsKey(sPin))
        {
            s += ", 'trigger': ";
            s += trig[sPin].as<String>();
            s += ", 'trigmode':'";
            JsonObject stab = getStable();
            String st = stab[sPin].as<String>();
            s += optionStable[st.toInt()];
            s += "'";
            s += " ";
            s += st;
        }
        int value = readPin(sPin.toInt(), k.value().as<String>());
        s += ", 'value': ";
        s += String(value);

        s = "{" + s + "}";
        if (r.length() > 1)
            r += ",";
        r += s;
    }
    r += "]";
    return r;
}
void Homeware::printConfig()
{

    serializeJson(config, Serial);
}

void Homeware::debug(String txt)
{
    if (config["debug"] == "on")
    {
        print(txt);
    }
    else if (config["debug"] == "term")
        Serial.println(txt);
    else if (txt.indexOf("ERRO") > -1)
        Serial.println(txt);
}

int Homeware::getAdcState(int pin)
{

    unsigned int tmpAdc = analogRead(pin);
    int rt = currentAdcState;
    const int v_min = config["adc_min"].as<int>();
    const int v_max = config["adc_max"].as<int>();
    if (tmpAdc >= v_max)
        rt = HIGH;
    if (tmpAdc < v_min)
        rt = LOW;
    if (rt != currentAdcState)
    {
        char buffer[64];
        sprintf(buffer, "adc %d,currentAdcState %d, adcState %d  (%i,%i) ", tmpAdc, currentAdcState, rt, v_max, v_min);
        debug(buffer);
    }
    return rt;
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
    if (homeware.getSensors()[String(pin)])
    {
        String sType = homeware.getDevices()[String(pin)];
        if (sType.startsWith("motion"))
        {
            bool bValue = value > 0;
            Serial.printf("Motion %s\r\n", bValue ? "detected" : "not detected");
            SinricProMotionsensor &myMotionsensor = SinricPro[homeware.getSensors()[String(pin)]]; // get motion sensor device
            myMotionsensor.sendMotionEvent(bValue);
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
bool onSinricMotionState(const String &deviceId, bool &state)
{
    Serial.printf("Device %s turned %s (via SinricPro) \r\n", deviceId.c_str(), state ? "on" : "off");
    int pin = findSinricPin(deviceId.c_str());
    if (pin > -1)
        homeware.writePin(pin, state ? HIGH : LOW);
    return true; // req
}
#endif

void Homeware::setupAlexa()
{
    Serial.println("preaparando alexa");
    JsonObject devices = getDevices();

    Serial.println("\r\nDevices\r\n============================\r\n");
    for (JsonPair k : devices)
    {
        Serial.println(stringf("%s is %s\r\n", k.key().c_str(), k.value().as<String>()));
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
        else if (sValue.startsWith("motion") && getSensors()[k.key().c_str()])
        {
            SinricProMotionsensor &myMotionsensor = SinricPro[getSensors()[k.key().c_str()]];
            myMotionsensor.onPowerState(onSinricMotionState);
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
                                homeware.resetDeepSleep();
 Serial.printf("Connected to SinricPro\r\n"); });
        SinricPro.onDisconnected([]()
                                 { Serial.printf("Disconnected from SinricPro\r\n"); });
        SinricPro.begin(config["app_key"], config["app_secret"]);
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
        homeware. resetDeepSleep();
        Serial.print("- Telnet: ");
        Serial.print(ip);
        Serial.println(" conectou");
        homeware.telnet.println("\nhello " + homeware.telnet.getIP());
        homeware.telnet.println("(Use ^] + q  para desligar.)"); });
    telnet.onInputReceived([](String str)
                           { 
                            if (str=="exit")
                               homeware.telnet.disconnectClient();
                            else {   
                            homeware.resetDeepSleep();
                            homeware.print(homeware.doCommand(str));} });

    Serial.print("- Telnet: ");
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

Homeware homeware;
#ifdef ESP32
WebServer server;
#else
ESP8266WebServer server;
#endif
