#include "Arduino.h"

#include "protocol.h"
#include <functions.h>
#include <LittleFS.h>
#include <list>

unsigned int timeoutDeepSleep = 10000;

#include "drivers.h"
#include "drivers/drivers_setup.h"

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

void Protocol::reset()
{
    // noop
}

JsonObject Protocol::getTrigger()
{
    return config["trigger"].as<JsonObject>();
}
JsonObject Protocol::getDevices()
{
    return config["device"].as<JsonObject>();
}
JsonObject Protocol::getSensors()
{
    return config["sensor"].as<JsonObject>();
}

JsonObject Protocol::getMode()
{
    return config["mode"].as<JsonObject>();
}

JsonObject Protocol::getStable()
{
    return config["stable"].as<JsonObject>();
}
JsonObject Protocol::getDefaults()
{
    return config["default"];
}

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
    int v = value;

    if (mode != NULL)
    {
        Driver *drv = getDrivers()->findByMode(mode);
        if (drv && drv->isSet())
        {
            v = drv->writePin(pin, value);
        }

        else
        {
            digitalWrite(pin, value);
        }
    }
    debug(stringf("{ 'writePin': %d, 'value': %d }", pin, value));
    // docPinValues[String(pin)] = v;
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
void Protocol::afterChanged(const int pin, const int value, const String mode)
{
    // evento para ser herando
}

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
        afterChanged(pin, newValue, getPinMode(pin));
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
        debug("Não tem um drive especifico: " + m);

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
    else if (config["debug"] == "term" || erro)
    {
        if (debugCallback)
            debugCallback("INF: " + txt);
        Serial.print(txt);
    }
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

bool inCheckTrigger = false;
void Protocol::checkTrigger(int pin, int value)
{
    if (inCheckTrigger)
        return;
    try
    {
        inCheckTrigger = true;
        String p = String(pin);
        JsonObject trig = getTrigger();
        if (trig.containsKey(p))
        {
            String pinTo = trig[p];
            if (!getStable().containsKey(p))
                return;

            debugf("{'pin':%i,'trigger': %s, 'set': %i }", pin, pinTo, value);

            int bistable = getStable()[p];
            int v = value;

            if ((bistable == 2 || bistable == 3))
            {
                if (v == 1)
                    switchPin(pinTo.toInt());
            }
            else if (pinTo.toInt() != pin)
                writePin(pinTo.toInt(), v);
        }
    }
    catch (char &e)
    {
    }
    inCheckTrigger = false;
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

void Protocol::setLedMode(const int mode)
{
    Driver *drv = getDrivers()->findByMode("led");
    if (drv && drv->active)
        drv->setV1(5 - ((mode <= 5) ? mode : 4) * 1000);
}

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

void Protocol::setupPins()
{
    debug("Configurando as portas: ");
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
    debugln("OK");
}

unsigned long sleeptmp = millis() + timeoutDeepSleep;
void Protocol::resetDeepSleep(const unsigned int t)
{
    unsigned int v = millis() + (timeoutDeepSleep * t);
    if (v > sleeptmp)
    {
        sleeptmp = v;
    }
}

void Protocol::doSleep(const int tempo)
{
    if (millis() > sleeptmp)
    {
        print("sleeping");
        ESP.deepSleep(tempo * 1000 * 1000);
    }
}
const char HELP[] =
    "set board <esp8266>\r\n"
    "show config\r\n"
    "gpio <pin> mode <in,out,adc,lc,ldr,dht,rst>\r\n"
    "gpio <pin> defult <n>(usado no setup)\r\n"
    "gpio <pin> mode gus (groove ultrasonic)\r\n"
    "gpio <pin> trigger <pin> [monostable,bistable]\r\n"
    "gpio <pin> device <onoff,dimmable> (usado na alexa)\r\n"
    "set app_key <x> (SINRIC)\r\n"
    "set app_secret <x> (SINRIC)\r\n"
    "gpio <pin> sensor <deviceId> (SINRIC)\r\n"
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
    String s = FPSTR(HELP);
    return s;
}

bool Protocol::readFile(String filename, char *buffer, size_t maxLen)
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

void driverCallbackEventFunc(String mode, int pin, int value)
{
    getInstanceOfProtocol()->driverCallbackEvent(mode, pin, value);
}

void Protocol::driverCallbackEvent(String mode, int pin, int value)
{
    debugf("callback: %s(%i,%i)", mode.c_str(), pin, value);
    checkTrigger(pin, value);
}

void Protocol::setup()
{
    protocol = this;

    debug("Registrando os drivers: ");
    drivers_register();

#ifdef ESP8266
    analogWriteRange(256);
#endif
    afterSetup();
    for (Driver *drv : getDrivers()->items)
        if (drv)
            drv->setTriggerEvent(driverCallbackEventFunc);

    debugln("OK");
}

void Protocol::afterConfigChanged()
{
}

String Protocol::restoreConfig()
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
}

DynamicJsonDocument Protocol::baseConfig()
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
    config["debug"] = inDebug ? "on" : "off";
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
#ifdef ESP32
    File file = SPIFFS.open("/config.json", "w");
#else
    File file = LittleFS.open("/config.json", "w");
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
    afterConfigChanged();
}

void Protocol::begin()
{

#ifdef TELNET
    resources += "telnet,";
    setupTelnet();
#endif
    afterBegin();
    debug("Resources: ");
    debugln(resources);
    inited = true;
}

void Protocol::afterBegin()
{
}
void Protocol::afterSetup()
{
    getDrivers()->setup();
}

#ifdef TELNET
void telnetOnConnect(String ip)
{
    protocol->inTelnet = true;
    protocol->resetDeepSleep();
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

String Protocol::localIP()
{
    return "";
}

String Protocol::doCommand(String command)
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
        else if (cmd[0] == "version")
        {
            return VERSION;
        }
        else if (cmd[0] == "help")
            return help();
        else if (cmd[0] == "show")
        {
            Serial.println("show: " + command);

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
            String ip = localIP();
            sprintf(buffer, "{ 'host':'%s' ,'version':'%s', 'name': '%s', 'ip': '%s'  }", hostname.c_str(), VERSION, config["label"].as<String>().c_str(), ip.c_str());
            return buffer;
        }
        else if (cmd[0] == "reset")
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
        else if (cmd[0] == "set")
        {
            Serial.println("set: " + command);

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
            Serial.println("get: " + command);

            return config[cmd[1]];
        }
        else if (cmd[0] == "pwm")
        {
            Serial.println("pwm: " + command);

            int pin = getPinByName(cmd[1]);
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
        else if (cmd[0] == "switch")
        {
            Serial.println("switch: " + command);
            int pin = getPinByName(cmd[1]);
            return String(switchPin(pin));
        }
        else if (cmd[0] == "gpio")
        {
            Serial.println("gpio: " + command);
            int pin = getPinByName(cmd[1]);
            String spin = String(pin);

            if (cmd[2] == "get" || cmd[2] == "set")
            {
                Serial.println("Busca de Drivers");
                Driver *drv = getDrivers()->findByPin(pin);
                if (drv)
                {
                    Serial.println("Executa Driver: " + command);
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
                        return String(drv->writePin(pin, cmd[3].toInt()));
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
                if (resources.indexOf(cmd[3]) > -1)
                {
                    initPinMode(pin, cmd[3]);
                    return "OK";
                }
                else
                    return "driver indisponivel";
            }
            else if (cmd[2] == "device")
            {

                JsonObject devices = getDevices();
                if (cmd[3] == "none")
                {
                    devices.remove(spin);
                    return "OK";
                }
                devices[spin] = cmd[3];
                if (String("motion,doorbell").indexOf(cmd[3]) > -1)
                    getMode()[spin] = "in";
                if (String("ldr,dht").indexOf(cmd[3]) > -1)
                    getMode()[spin] = cmd[3];
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
            else if (cmd[2] == "trigger")
            {
                if (!cmd[4])
                    return "cmd incompleto";
                JsonObject trigger = getTrigger();
                trigger[spin] = getPinByName(cmd[3]);

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

void Protocol::resetWiFi()
{
    // noop
}

void Protocol::printConfig()
{

    serializeJson(config, Serial);
}

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
        String *r = split(cmd, ' ');
        if (validos.indexOf(r[0]) > -1)
        {
            String rsp = protocol->doCommand(cmd);
            if (!rsp.startsWith("invalid"))
                protocol->debug("SER: " + rsp);
        }
        else if (protocol->debugCallback)
            protocol->debugCallback(cmd);
    }
}

bool inLooping = false;
void Protocol::loop()
{

    if (inLooping)
        return;
    try
    {
        inLooping = true;
        if (!inited)
            begin();
        lerSerial();
#ifdef TELNET
        telnet.loop(); // se estive AP, pode conectar por telnet ou pelo browser.
#endif
        loopEvent();

        //=========================== usado somente quando conectado
        if (connected)
        {
        }
        getDrivers()->loop();
    }
    catch (int &e)
    {
    }
    const int sleep = config["sleep"].as<String>().toInt();
    if (sleep > 0)
        doSleep(sleep);
    yield();

    inLooping = false;
    //===========================
}

JsonObject Protocol::getValues()
{
    return docPinValues.as<JsonObject>();
}

void Protocol::afterLoop()
{
    // usado na herança
}

void Protocol::loopEvent()
{

    unsigned long interval = 500;
    try
    {
        if (containsKey("interval"))
        {
            interval = getKey("interval").toInt();
        }
        if (interval < 50)
            interval = 50;
        if (millis() - loopEventMillis > interval)
        {
            JsonObject mode = config["mode"];
            for (JsonPair k : mode)
            {
                readPin(String(k.key().c_str()).toInt(), k.value().as<String>());
            }
            loopEventMillis = millis();
            afterLoop();
        }
    }
    catch (const char *e)
    {
        print(String(e));
    }
}
