#include "Arduino.h"

#include "protocol.h"
#include <functions.h>

#ifdef SPIFFs
#include <SPIFFS.h>
#endif
#ifdef LITTLEFs
#include <LittleFS.h>
#endif

#ifndef ARDUINO_AVR
unsigned int timeoutDeepSleep = 1000;
#endif

#include "drivers.h"
#include "drivers/drivers_setup.h"

#include "api/api_setup.h"
#include <api.h>

Protocol *protocol;

void Protocol::debugf(const char *format, ...)
{
    static char buffer[512];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    protocol->debug(buffer);
}

Protocol *getInstanceOfProtocol()
{
    return protocol;
}

size_t driversCount = 0;

#ifndef ARDUINO_AVR
void Protocol::reset()
{
    // noop
}
#endif

JsonObject Protocol::getTrigger()
{
    return config["trigger"].as<JsonObject>();
}
#ifndef ARDUINO_AVR
JsonObject Protocol::getDevices()
{
    return config["device"].as<JsonObject>();
}
JsonObject Protocol::getSensors()
{
    return config["sensor"].as<JsonObject>();
}
#endif

JsonObject Protocol::getMode()
{
    return config["mode"].as<JsonObject>();
}

JsonObject Protocol::getStable()
{
    return config["stable"].as<JsonObject>();
}

#ifndef ARDUINO_AVR
JsonObject Protocol::getDefaults()
{
    return config["default"];
}
#endif

void Protocol::initPinMode(int pin, const String m)
{
    Driver *drv = getDrivers()->findByMode(m);
    if (drv)
    {
        drv->setPinMode(pin);
    }
    else
        pinMode(pin, OUTPUT);
    JsonObject mode = getMode();
    mode[String(pin)] = m;
}

int Protocol::writePin(const int pin, const int value)
{
    String mode = getMode()[String(pin)];

    if (mode != NULL)
    {
        debugf("write %s pin: %i to: %i", mode, pin, value);
        Driver *drv = getDrivers()->findByMode(mode);
        if (drv && drv->isSet())
        {
            drv->writePin(pin, value);
        }

        else
        {
            digitalWrite(pin, value);
        }
    }
    return value;
}

int Protocol::writePWM(const int pin, const int value, const int timeout)
{
    Driver *drv = getDrivers()->findByMode("pwm");
    if (drv && drv->active)
    {
        drv->setV1(timeout);
        return drv->writePin(pin, value);
    }
    return 0;
}

int Protocol::pinValue(const int pin)
{
    return docPinValues[String(pin)];
}

#ifndef ARDUINO_AVR
void Protocol::afterChanged(const int pin, const int value, const String mode)
{
    // evento para ser herando
}
#endif

String Protocol::getPinMode(const int pin)
{
    return getMode()[String(pin)].as<String>();
}

bool Protocol::pinValueChanged(const int pin, const int newValue, bool exectrigger)
{
    if (pinValue(pin) != newValue)
    {
        docPinValues[String(pin)] = newValue;
        getDrivers()->changed(pin, newValue);
        getApiDrivers().changed(pin, newValue);
#ifndef ARDUINO_AVR
        afterChanged(pin, newValue, getPinMode(pin));
#endif
        if (exectrigger)
            checkTrigger(pin, newValue);
        return true;
    }
    return false;
}

int Protocol::readPin(const int pin, const String mode)
{
    Driver *drv;
    int newValue = 0;
    bool exectrigger = true;
    String m = mode;
    if (!m)
    {
        drv = getDrivers()->findByPin(pin);
        if (drv)
            m = drv->getMode();
    }
    else
        drv = getDrivers()->findByMode(m);
    if (!drv)
        debugf("Pin: %i. Não tem um drive especifico: %s \r\n", pin, m);

    if (drv && drv->isGet())
    {
        newValue = drv->readPin(pin);
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

void Protocol::debugln(String txt)
{
    debug(txt);
    Serial.println("");
}
void Protocol::debug(String txt)
{
    const bool erro = txt.indexOf("ERRO") > -1;
    if (config["debug"] == "on" || erro)
    {
        print(txt);
    }
#ifndef ARDUINO_AVR
    else

        if (config["debug"] == "term" || erro)
    {

        if (debugCallback)
            debugCallback("INF: " + txt);
        Serial.print(txt);
    }
#endif
    else
        Serial.print(txt);
}
int Protocol::findPinByMode(String mode)
{
    for (JsonPair k : getMode())
    {
        if (k.value().as<String>() == mode)
            return String(k.key().c_str()).toInt();
    }
    return -1;
}

String Protocol::print(String msg)
{
    Serial.print("INF: ");
    Serial.println(msg);
#ifdef TELNET
    telnet.println("INF: " + msg);
#endif
#ifdef WEBSOCKET
    if (debugCallback)
        debugCallback("INF: " + msg);
#endif
    return msg;
}

void Protocol::checkTrigger(int pin, int value)
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

            if ((bistable == 2 || bistable == 3))
            {
                if (v == 1)
                    switchPin(pinTo.toInt());
            }
            else if (pinTo.toInt() != pin)
            {
                debugf("{'trigger':%i, 'to':%i}", pin, value);
                writePin(pinTo.toInt(), v);
            }
        }
#ifndef ARDUINO_AVR
    }
    catch (char &e)
    {
    }
#endif
}

int Protocol::switchPin(const int pin)
{
    Driver *drv = getDrivers()->findByPin(pin);
    if (drv)
    {
        int r = drv->readPin(pin);
        debugf("{'switch':%i,'mode':'%s', 'actual':%i, 'to':%i}", pin, drv->getMode(), r, (r > 0) ? LOW : HIGH);
        return drv->writePin(pin, (r > 0) ? LOW : HIGH);
    }
    return -1;
}

#ifndef ARDUINO_AVR

void Protocol::setLedMode(const int mode)
{
    Driver *drv = getDrivers()->findByMode("led");
    if (drv && drv->active)
        drv->setV1(5 - ((mode <= 5) ? mode : 4) * 1000);
}
#endif
String Protocol::getStatus()
{
    StaticJsonDocument<256> doc;
    for (size_t i = 0; i < 18; i++)
    {
        doc[String(i)] = readPin(i, "");
    }
    String r;
    serializeJson(doc, r);
    return r;
}

#ifndef ARDUINO_AVR
const String optionStable[] = {"monostable", "monostableNC", "bistable", "bistableNC"};
String Protocol::showGpio()
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
#endif

void Protocol::setupPins()
{

    JsonObject mode = config["mode"];

    for (JsonPair k : mode)
    {
        int pin = String(k.key().c_str()).toInt();
        getDrivers()->initPinMode(k.value().as<String>(), pin);
    }

#ifndef ARDUINO_AVR
    debug("Registrando os drivers: ");
#ifdef ESP8266
    analogWriteRange(256);
#endif

    for (Driver *drv : getDrivers()->items)
        if (drv)
        {
            drv->setTriggerEvent(driverCallbackEvent);
            drv->setTriggerOkState(driverOkCallbackEvent);
        }
    debugln("OK");
#endif
    getDrivers()->setup();

#ifndef ARDUINO_AVR

    for (JsonPair k : getDefaults())
    {
        writePin(String(k.key().c_str()).toInt(), k.value().as<String>().toInt());
    }

    /// APIs externas
    register_Api_setup();
    JsonObject sensores = getDevices();
    for (JsonPair k : sensores)
    {
        // init sensor, set pins e setup();
        getApiDrivers().initPinSensor(String(k.key().c_str()).toInt(), k.value().as<String>());
    }
    getApiDrivers().afterSetup();

#endif

    debugln("OK");
}

#ifndef ARDUINO_AVR

unsigned long sleeptmp = millis() + timeoutDeepSleep;
void Protocol::resetDeepSleep(const unsigned int t)
{
    int x = timeoutDeepSleep * t;
    unsigned int v = millis() + (x);

#ifdef DEBUG_ON
    Serial.printf("next %i sleep: %i at: %i \r\n", t, x, v);
#endif

    if (v > sleeptmp)
    {
        sleeptmp = v;
    }
}
void Protocol::doSleep(const int tempo)
{

    print("sleeping");
#ifdef ESP32
    esp_sleep_enable_timer_wakeup(tempo * 1000000);
    esp_deep_sleep_start();
#else
#ifdef DEBUG_ON
    Serial.println("checar se RST e D0 conectados para weakup");
#endif
    ESP.deepSleep(tempo * 1000 * 1000);
#endif
}

const char HELP[] =
    "set board <esp8266>\r\n"
    "show config\r\n"
    "gpio <pin> mode <in,out,adc,lc,ldr,dht,rst>\r\n"
    "gpio <pin> defult <n>(usado no setup)\r\n"
#ifdef GOOVER_ULTRASONIC
    "gpio <pin> mode gus (groove ultrasonic)\r\n"
#endif
    "gpio <pin> trigger <pin> [monostable,bistable]\r\n"
#ifdef ALEXA
    "gpio <pin> device <onoff,dimmable> (usado na alexa)\r\n"
#endif
#ifdef SINRICPRO
    "set app_key <x> (SINRIC)\r\n"
    "set app_secret <x> (SINRIC)\r\n"
    "gpio <pin> sensor <deviceId> (SINRIC)\r\n"
#endif
    "gpio <pin> get\r\n"
    "gpio <pin> set <n>\r\n"
    "version\r\n"
    "switch <pin>\r\n"
    "show gpio\r\n"
#ifdef GROOVE_ULTRASONIC
    "set gus_min 50\r\n"
    "set gus_max 150\r\n"
#endif
    "set interval 50\r\n"
    "set adc_min 511\r\n"
    "set adc_max 512\r\n";

String Protocol::help()
{
    String s = HELP;
    return s;
}

bool Protocol::readFile(String filename, char *buffer, size_t maxLen)
{
#ifdef SPIFFs
    File file = SPIFFS.open(filename, "r");
#else
#ifdef ESP32
    File file = LITTLEFS.open(filename, "r", true);
#else
    File file = LittleFS.open(filename, "r");
#endif
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

void Protocol::driverCallbackEvent(String mode, int pin, int value)
{
    getInstanceOfProtocol()->debugf("callback: %s(%i,%i)", mode.c_str(), pin, value);
    getInstanceOfProtocol()->checkTrigger(pin, value);
}
void Protocol::driverOkCallbackEvent(String mode, int pin, int value)
{
    Driver *drv = getDrivers()->findByMode("ok");
    if (drv)
        drv->writePin(pin, value);
}

#endif

void Protocol::prepare()
{
#ifdef SPIFFs
    if (!SPIFFS.begin())
#else
#ifdef LITTLEFs
#ifdef ESP32
    if (!LITTLEFS.begin(true))
#else
    if (!LittleFS.begin())
#endif
#endif
#endif
    {
        Serial.println("FS mount failed");
    }

    protocol = this;
    drivers_register();
}

void Protocol::setup()
{
#ifndef ARDUINO_AVR
    afterSetup();
#endif
}

#ifndef ARDUINO_AVR

void Protocol::afterConfigChanged()
{
    getDrivers()->setup(); // mudou as configurações, recarregar os parametros;
}

String Protocol::restoreConfig()
{

    String rt = "nao restaurou config";
    Serial.println("");
    // linha();
    try
    {
        String old = config.as<String>();
#ifdef SPIFFs
        File file = SPIFFS.open("/config.json", "r");
#else
#ifdef ESP32
        File file = LITTLEFS.open("/config.json", "r", true);
#else
        File file = LittleFS.open("/config.json", "r");
#endif
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
        // serializeJson(config, Serial);
        // Serial.println("");
        rt = "OK";
        // linha();
    }
    catch (const char *e)
    {
        return String(e);
    }
    Serial.println("");
    afterConfigChanged();
    return rt;
    return "";
}
#endif

DynamicJsonDocument Protocol::baseConfig()
{
    DynamicJsonDocument config = DynamicJsonDocument(1024);
    config["label"] = LABEL;
    config.createNestedObject("mode");
    config.createNestedObject("trigger");
    config.createNestedObject("stable");
#ifdef BOARD_NAME
    config["board"] = BOARD_NAME;
#endif
#ifndef ARDUINO_AVR
    config.createNestedObject("device");
    config.createNestedObject("sensor");
    config.createNestedObject("default");
    config["adc_min"] = "125";
    config["adc_max"] = "126";
    config["sleep"] = "0";
#ifdef SSID_NAME
    config["ap_ssid"] = SSID_NAME;
    config["ap_password"] = SSID_PASWORD;
#else
    config["ap_ssid"] = "none";
    config["ap_password"] = "123456780";
#endif
#endif

#if defined(DEBUG_ON)
    config["debug"] = "on";
#elif defined(DEBUG_OFF)
    config["debug"] = "off";
#else
    config["debug"] =
#ifndef ARDUINO_AVR
        inDebug ? "on" : "off";
#else
        "off";
#endif
#endif
    config["interval"] = "500";

#ifdef MQTT
    config["mqtt_host"] = "none"; //"test.mosquitto.org";
    config["mqtt_port"] = 1883;
    config["mqtt_user"] = "homeware";
    config["mqtt_password"] = "123456780";
    config["mqtt_interval"] = 1;
    config["mqtt_prefix"] = "mesh";
#endif
#ifdef SINRICPRO
    config["app_key"] = "";
    config["app_secret"] = "";
#endif
    return config;
}

#ifndef ARDUINO_AVR
String Protocol::saveConfig()
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
    // serializeJson(base, Serial);
#ifdef SPIFFs
    File file = SPIFFS.open("/config.json", "w");
#else
#ifdef ESP32
    File file = LITTLEFS.open("/config.json", "w", true);
#else
    File file = LittleFS.open("/config.json", "w");
#endif
#endif
    if (serializeJson(base, file) == 0)
        rsp = "não gravou /config.json";
    file.close();
    afterConfigChanged();
    return rsp;
}

void Protocol::defaultConfig()
{
    DynamicJsonDocument doc = baseConfig();
    for (JsonPair k : doc.as<JsonObject>())
    {
        String key = k.key().c_str();
        config[key] = doc[key];
    }
}
#endif

void Protocol::begin()
{

#ifdef TELNET
    resources += "telnet,";
    setupTelnet();
#endif
#ifndef ARDUINO_AVR
    afterBegin();
    debug("Resources: ");
    debugln(resources);
#endif
    inited = true;
}

#ifndef ARDUINO_AVR
void Protocol::afterBegin()
{
}
void Protocol::afterSetup()
{
}
#endif

#ifdef TELNET
void telnetOnConnect(String ip)
{
    protocol->inTelnet = true;
    protocol->resetDeepSleep(60 * 5);
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" conectou");
    protocol->telnet.println("\nhello " + protocol->telnet.getIP());
    protocol->telnet.println("(Use ^] + q  para desligar.)");
}
void telnetOnInputReceive(String str)
{
    Serial.println("TEL: " + str);
    if (str == "exit")
    {
        protocol->inTelnet = false;
        protocol->telnet.disconnectClient();
    }
    else
    {
        protocol->resetDeepSleep();
        protocol->print(protocol->doCommand(str));
    }
    yield();
}

void Protocol::setupTelnet()
{

    debug("Telnet: ");
    telnet.onConnect(telnetOnConnect);
    telnet.onInputReceived(telnetOnInputReceive);
    if (telnet.begin())
    {
        debugln("OK");
    }
    else
    {
        Serial.println("Error");
        errorMsg("Will reboot...");
    }
}

#endif

void Protocol::errorMsg(String msg)
{
    Serial.println(msg);
#ifdef TELNET
    telnet.println(msg);
#endif
}

#ifndef ARDUINO_AVR
String Protocol::localIP()
{
    return "";
}
#endif

int convertOnOff(String pin)
{
#ifndef ARDUINO_AVR
    pin.toLowerCase();
    if (pin == "on" || pin == "high" || pin == "1")
    {
        return 1;
    }
    else if (pin == "off" || pin == "low" || pin == "0")
    {
        return 0;
    }
#endif
    return pin.toInt();
}

String Protocol::doCommand(String command)
{
#ifdef DEBUG_ON
    Serial.print("Command: ");
    Serial.println(command);
#endif
    if (command.startsWith("SERIAL "))
    {
        command.replace("SERIAL ", "");
        Serial.print(command);
        return "OK";
    }
#ifndef ARDUINO_AVR
    try
    {
        resetDeepSleep(60 * 5);
#endif
        String *cmd = split(command, ' ');
#ifdef ESP8266
        if (cmd[0] == "format")
        {
            LittleFS.format();
            return "formated";
        }
        else
#endif
            if (cmd[0] == "sleep" && String(cmd[1]).toInt() > 0)
        {
            doSleep(String(cmd[1]).toInt());
            return "OK";
        }
#ifndef ARDUINO_AVR
        if (cmd[0] == "open")
        {
            char json[1024];
            readFile(cmd[1], json, 1024);
            return String(json);
        }
        else if (cmd[0] == "test")
        {
            config["debug"] = "on";
            int pin = getPinByName(cmd[1]);
            if (cmd[1].substring(0, 1) == "A")
            {
                for (int i = 0; i < 10; i++)
                {
                    debugf("%i. %i: %i", i, pin, analogRead(pin));
                    yield();
                    delay(1000);
                }
            }
            else
            {
                pinMode(pin, OUTPUT);
                for (int i = 0; i < 10; i++)
                {
                    digitalWrite(pin, i % 2);
                    debugf("%i. %i: %i", i, pin, i % 2);
                    yield();
                    delay(1000);
                    yield();
                }
                digitalWrite(pin, LOW);
            }
            return "OK";
        }
        else

            if (cmd[0] == "version")
        {
            return VERSION;
        }
        else

            if (cmd[0] == "help")
            return help();
        else
#endif
            if (cmd[0] == "show")
        {
#ifndef ARDUINO_AVR

            Serial.println("show: " + command);

            if (cmd[1] == "resources")
                return resources;
            else if (cmd[1] == "status")
            {
                return getStatus();
            }
            else
#endif
                if (cmd[1] == "config")
                return config.as<String>();
#ifndef ARDUINO_AVR
            else if (cmd[1] == "gpio")
                return showGpio();
            char buffer[128];
            String ip = localIP();
            sprintf(buffer, "{ 'host':'%s' ,'version':'%s', 'name': '%s', 'ip': '%s'  }", hostname.c_str(), VERSION, config["label"].as<String>().c_str(), ip.c_str());
            return buffer;
#endif
        }
#ifndef ARDUINO_AVR

        else

            if (cmd[0] == "reset")
        {
            if (cmd[1] == "wifi")
            {
                resetWiFi();
                return "OK";
            }
            else if (cmd[1] == "factory")
            {
                defaultConfig();
                return "OK";
            }
            print("reiniciando...");
            delay(1000);
            reset();
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
        else if (cmd[0] == "debug")
        {
            config["debug"] = cmd[1];
            return "OK";
        }
#endif
        else if (cmd[0] == "set")
        {
            if (cmd[2] == "none")
            {
                config.remove(cmd[1]);
            }
            else
            {
                config[cmd[1]] = cmd[2];
                getDrivers()->setup(); // mudou as configurações, recarregar os parametros;
            }
            return "OK";
        }
        else if (cmd[0] == "get")
        {
            return config[cmd[1]];
        }
        else if (cmd[0] == "pwm")
        {
            int pin = getPinByName(cmd[1]);
            int timeout = 0;
            if (cmd[4] == "until" || cmd[4] == "timeout")
                timeout = cmd[5].toInt();
            if (cmd[2] == "set")
            {
                int value = convertOnOff(cmd[3]);
                int rsp = writePWM(pin, value, timeout);
                return String(rsp);
            }
            if (cmd[2] == "get")
            {
                int rsp = readPin(pin, "pwm");
                return String(rsp);
            }
        }
        else if (cmd[0] == "switch")
        {
            int pin = getPinByName(cmd[1]);
            return String(switchPin(pin));
        }
        else if (cmd[0] == "gpio")
        {
            int pin = getPinByName(cmd[1]);
            String spin = String(pin);

            if (cmd[2] == "get" || cmd[2] == "set")
            {
                Driver *drv = getDrivers()->findByPin(pin);
                if (drv)
                {
                    if (cmd[2] == "status")
                    {
                        JsonObject j = drv->readStatus(pin);
                        String rsp;
                        serializeJson(j, rsp);
                        return rsp;
                    }
                    else if (cmd[2] == "get" && drv->isGet())
                    {
                        return String(drv->readPin(pin));
                    }
                    else if (cmd[2] == "set" && drv->isSet())
                    {
                        return String(drv->writePin(pin, convertOnOff(cmd[3])));
                    }
                    else if (cmd[2] == "get" && drv->isStatus())
                    {
                        JsonObject sts = drv->readStatus(pin);
                        String rsp;
                        serializeJson(sts, rsp);
                        return rsp;
                    }
                    else if (drv->isCommand())
                    {
                        String rst = drv->doCommand(command);
                        if (rst != "NAK")
                            return rst;
                    }
                }
            }

            if (cmd[2] == "none")
            {
                getDrivers()->deleteByPin(pin);
                config["mode"].remove(spin);
                return "OK";
            }
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
#ifndef ARDUINO_AVR

                if (resources.indexOf(cmd[3]) > -1)
                {
                    getDrivers()->initPinMode(cmd[3], pin);
                    initPinMode(pin, cmd[3]);
                    return "OK";
                }
                else
                    return "driver indisponivel";
#else
            initPinMode(pin, cmd[3]);
            return "OK";

#endif
            }
#ifndef ARDUINO_AVR
            else if (cmd[2] == "device")
            {

                JsonObject devices = getDevices();
                if (cmd[3] == "none")
                {
                    devices.remove(spin);
                    return "OK";
                }
                devices[spin] = cmd[3];
                /*if (String("motion,doorbell").indexOf(cmd[3]) > -1)
                    getMode()[spin] = "in";
                if (String("ldr,dht").indexOf(cmd[3]) > -1)
                    getMode()[spin] = cmd[3];*/
                return "OK";
            }
            else if (cmd[2] == "sensor")
            {
                JsonObject sensors = getSensors();
                if (cmd[3] == "none")
                {
                    sensors.remove(spin);
                    return "OK";
                }
                sensors[spin] = cmd[3];
                return "OK";
            }
            else if (cmd[2] == "default")
            {
                JsonObject d = getDefaults();
                if (cmd[3] == "none")
                {
                    d.remove(spin);
                    return "OK";
                }
                d[spin] = cmd[3];
                return "OK";
            }
#endif
            else if (cmd[2] == "trigger")
            {
                if (!cmd[4])
                    return "cmd incompleto";
                JsonObject trigger = getTrigger();
                if (cmd[3] == "none")
                {
                    trigger.remove(spin);
                    return "OK";
                }
                trigger[spin] = getPinByName(cmd[3]);

                // 0-monostable 1-monostableNC 2-bistable 3-bistableNC
                getStable()[spin] = (cmd[4] == "bistable" ? 2 : 0) + (cmd[4].endsWith("NC") ? 1 : 0);

                return "OK";
            }
        }
        return "invalido";
#ifndef ARDUINO_AVR
    }
    catch (const char *e)
    {
        return String(e);
    }
#endif
}

#ifndef ARDUINO_AVR
void Protocol::resetWiFi()
{
    // noop
}
void Protocol::printConfig()
{

    serializeJson(config, Serial);
}
#endif

void lerSerial()
{
    if (Serial.available() > 0)
    {
        char term = '\n';
        String cmd = Serial.readStringUntil(term);
        cmd.replace("\r", "");
        cmd.replace("\n", "");
        if (cmd.length() == 0)
            return;
        String validos = "gpio,get,show,switch";
#ifdef ARDUINO_AVR
        validos += ",set";
#endif
        String *r = split(cmd, ' ');
        if (validos.indexOf(r[0]) > -1)
        {
            String rsp = protocol->doCommand(cmd);
            if (!rsp.startsWith("invalid"))
                protocol->debug("SER: " + rsp);
        }
#ifndef ARDUINO_AVR

        else if (protocol->debugCallback)
            protocol->debugCallback(cmd);
#endif
    }
}

bool inLooping = false;
void Protocol::loop()
{

    if (inLooping)
        return;
    inLooping = true;
    if (!inited)
        begin();
    lerSerial();
#ifdef TELNET
    telnet.loop(); // se estive AP, pode conectar por telnet ou pelo browser.
#endif
    eventLoop();

    getDrivers()->loop();
    getApiDrivers().loop();

#ifndef ARDUINO_AVR
    const int sleep = config["sleep"].as<String>().toInt();
    if (sleep > 0 && millis() > sleeptmp)
    {
        doSleep(sleep);
    }
#endif
    yield();

    inLooping = false;
    //===========================
}

#ifndef ARDUINO_AVR
JsonObject Protocol::getValues()
{
    return docPinValues.as<JsonObject>();
}

void Protocol::afterLoop()
{
    // usado na herança
}
#endif

void Protocol::eventLoop()
{

    unsigned long interval = 500;
#ifndef ARDUINO_AVR
    try
    {
#endif
        if (containsKey("interval"))
        {
            interval = getKey("interval").toInt();
        }
        if (interval < 50)
            interval = 50;
        if (millis() - eventLoopMillis > interval)
        {
            JsonObject mode = config["mode"];
            for (JsonPair k : mode)
            {
                // Serial.printf("readPin(%s, %s)\r\n", k.key().c_str(), k.value().as<String>());
                readPin(String(k.key().c_str()).toInt(), k.value().as<String>());
            }
            eventLoopMillis = millis();
#ifndef ARDUINO_AVR
            afterLoop();
#endif
        }

#ifndef ARDUINO_AVR
    }
    catch (const char *e)
    {
        print(String(e));
    }
#endif
}
