#include <Arduino.h>

#include "protocol.h"
#include "functions.h"

#ifdef SPIFFs
#include <FS.h>
// #include <SPIFFS.h>
#endif

#ifndef ARDUINO_AVR
unsigned int timeoutDeepSleep = 1000;
#ifdef LITTLEFs
#include <LittleFS.h>
#endif
#endif

#include "drivers.h"
#include "drivers/drivers_setup.h"

#ifndef NO_API
#include "api/api_setup.h"
#include "api.h"
#endif

#ifdef MQTTClient
#include "api/mqtt_client.h"
#endif

Protocol *protocol;

void Protocol::actionEvent(const char *txt)
{
#ifdef MQTTClient
    ApiDriver *drv = getApiDrivers().findByType("mqtt");
    if (drv)
    {
        drv->changed(txt);
    }
#endif
    debug(txt);
}

void Protocol::debugf(const char *format, ...)
{
    static char buffer[512];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    debug(buffer);
}

Protocol *getInstanceOfProtocol()
{
    return protocol;
}

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
#ifndef BASIC
JsonObject Protocol::getSensors()
{
    return config["sensor"].as<JsonObject>();
}
#endif
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
#ifndef BASIC
JsonObject Protocol::getDefaults()
{
    return config["default"];
}
#endif
#endif
JsonObject Protocol::getTimers()
{
    return config["timers"];
}
JsonObject Protocol::getIntervals()
{
    return config["intervals"];
}
/// @brief lista as senhas que são alvo de disparo.
/// @return lista de cenas disponiveis para disparo.
#ifndef BASIC
JsonObject Protocol::getScenes()
{
    return config["scenes"];
}
/// @brief lista de triggers a ser disparadas com base em eventos de cenarios
/// @return lista objetos triggers
JsonObject Protocol::getTriggerScenes()
{
    return config["scene_triggers"];
}
#endif

/// @brief chamada inicial para criar a estrutura de Pins configurados
/// @param pin -> pin alvo para configuração
/// @param m  -> mode, indica o comportamento do Pin
void Protocol::initPinMode(int pin, const String m)
{

    Driver *drv = getDrivers()->initPinMode(m, pin);
    if (drv)
    {
#ifndef ARDUINO_AVR
        drv->setTriggerEvent(driverCallbackEvent);
        drv->setTriggerOkState(driverOkCallbackEvent);
#endif
    }
#ifdef DEBUG_DRV
    Serial.print("DRV: ");
    Serial.printf("%i is %s - ", pin, m);
#endif
    JsonObject mode = getMode();
#ifdef DEBUG_DRV
    if (!mode)
        Serial.println("não inicializou o config");
    else
#endif
        mode[String(pin)] = m;
#ifdef DEBUG_DRV
    Serial.println("OK");
#endif
}

/// @brief Escreve uma valores para o Pin
/// @param pin - Pin alvo
/// @param value  - valor a escrever
/// @return valor atribuido
int Protocol::writePin(const int pin, const int value)
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

#ifndef BASIC
/// @brief Escreve comportamento PWM para o pin;
/// @param pin
/// @param value
/// @param timeout quando tempo o valor deve ficar HIGH antes de voltar para LOW; 0 indica para não desligar
/// @return valor enviado
int Protocol::writePWM(const int pin, const int value, const int timeout)
{
    Driver *drv = getDrivers()->findByPin(pin);
    if (drv && drv->active)
    {
        drv->timeout = timeout;
        return drv->writePin(value);
    }
    return 0;
}
#endif

/// @brief retorna o ultimo valor atribuido ao Pin
/// @param pin
/// @return
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

/// @brief busca o Pin Mode para um Pin
/// @param pin
/// @return qual o mode de comportamento do pin
String Protocol::getPinMode(const int pin)
{
    return getMode()[String(pin)].as<String>();
}

/// @brief gerar evento trigger caso o valor atribuido ao Pin foi alterado
/// @param pin
/// @param newValue
/// @param exectrigger
/// @return
bool initedValues = false;
bool Protocol::pinValueChanged(const int pin, const int newValue, bool exectrigger)
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
#ifndef BASIC
            checkTriggerScene(pin, newValue);
#endif
            return true;
        }
        initedValues = true;
    }
    return false;
}

#ifndef BASIC
/// @brief gerar trigger de cenario para mudança de estado do pin
/// @param pin
/// @param value
void Protocol::checkTriggerScene(const int pin, const int value)
{
    for (JsonPair p : getScenes())
    {
        // getScenes -> lista as senhas que são alvo de disparo.
        if (p.value().as<int>() == pin)
        {
            /// monta scene a ser executada - executa quando existe uma scene_triggers
            String sceneName = String(p.key().c_str());
            String cmd = "scene " + sceneName + " set " + String(value);
            doCommand(cmd); // executa local se existir uma "scene_triggers" para  "scene" - funciona independente de ter
#ifdef MQTTClient
            ApiDriver *drv = getApiDrivers().findByType("mqtt");
            if (drv)
            {
                MqttClientDriver *mqtt = (MqttClientDriver *)drv;
                if (mqtt)
                    mqtt->sendScene(sceneName.c_str(), value);
            }
#endif
        }
    }
}
#endif

/// @brief faz a leitura do valor atribuido ao Pin - leitura lógica no Pin
/// @param pin
/// @param mode
/// @return
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

void Protocol::debugln(String txt)
{
    debug(txt);
    Serial.println("");
}
void Protocol::debug(String msg)
{
    const bool erro = msg.indexOf("ERRO") > -1;
    if (config["debug"] == "on" || erro)
    {
        print(msg);
    }
#ifndef ARDUINO_AVR
    else

        if (config["debug"] == "term" || erro)
    {

        if (debugCallback)
            debugCallback(msg.startsWith("{") ? msg : "INF: " + msg);
        Serial.print(msg);
    }
#endif
    else
        Serial.print(msg);
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
    String info = msg.startsWith("{") ? msg : "INF: " + msg;
    Serial.print(info);
#ifdef TELNET
    telnet.print(info);
#endif
#ifdef WEBSOCKET
    if (debugCallback)
        debugCallback(info);
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
/// @brief comutador de estado; se esta em HIGH mudar para LOW; se estiver em LOW mudar para HIGH
/// @param pin
/// @return
int Protocol::switchPin(const int pin)
{
    Driver *drv = getDrivers()->findByPin(pin);
    if (drv)
    {
        int r = drv->readPin();
#ifdef DEBUG_ON
        debugf("{'switch':%i,'mode':'%s', 'actual':%i, 'to':%i}", pin, drv->getMode(), r, (r > 0) ? LOW : HIGH);
#endif
        return drv->writePin((r > 0) ? LOW : HIGH);
    }
    return -1;
}

#ifndef ARDUINO_AVR

void Protocol::setLedMode(const int mode)
{
    Driver *drv = getDrivers()->findByMode("led");
    if (drv && drv->active)
        drv->interval = (5 - ((mode <= 5) ? mode : 4) * 1000);
}
#endif

#ifndef BASIC
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
#endif

#ifndef ARDUINO_AVR
#ifndef BASIC
const String optionStable[] = {"monostable", "monostableNC", "bistable", "bistableNC"};
#endif
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
#ifndef BASIC
            s += optionStable[st.toInt()];
#else
            s += st;
#endif
            s += "'";
        }
#ifndef BASIC
        int value = readPin(sPin.toInt(), k.value().as<String>());
        s += ", 'value': ";
        s += String(value);

        if (getTimers().containsKey(sPin))
        {
            s += ", 'timer': ";
            s += getTimers()[sPin].as<String>();
        }
#endif
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
#ifndef NO_API
    register_Api_setup();
    getApiDrivers().afterCreate();
#endif
    JsonObject mode = config["mode"];
    for (JsonPair k : mode)
    {
        int pin = String(k.key().c_str()).toInt();
        initPinMode(pin, k.value().as<String>());
    }
#if !(defined(ARDUINO_AVR) || defined(BASIC))
    debug("Registrando os drivers: ");
#ifdef ESP8266
    analogWriteRange(256);
#endif
#endif
    getDrivers()->setup();

#ifndef ARDUINO_AVR

#ifndef BASIC
    for (JsonPair k : getDefaults())
    {
        writePin(String(k.key().c_str()).toInt(), k.value().as<String>().toInt());
    }
#endif

    /// APIs externas
#ifndef NO_API
#ifndef BASIC
    JsonObject sensores = getDevices();
    for (JsonPair k : sensores)
    {
        // init sensor, set pins e setup();
        getApiDrivers().initPinSensor(String(k.key().c_str()).toInt(), k.value().as<String>());
    }
    getApiDrivers().afterSetup();
#endif
#endif
#if !(defined(ARDUINO_AVR) || defined(BASIC))
    debugln("OK");
#endif
#endif
}

#ifndef ARDUINO_AVR
#ifndef BASIC

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
#if defined(DEBUG_ON) || defined(DEBUG_FULL)
    Serial.println("checar se RST e D0 conectados para weakup");
#endif
    ESP.deepSleep(tempo * 1000 * 1000);
#endif
}
#endif

const char HELP[] =
#ifndef BASIC
    "set board <esp8266>\r\n"
    "show config\r\n"
    "gpio <pin> mode <in,out,adc,lc,ldr,dht,rst>\r\n"

    "gpio <pin> default <n>(usado no setup)\r\n"
#endif
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
#ifdef GROOVE_ULTRASONIC
    "set gus_min 50\r\n"
    "set gus_max 150\r\n"
#endif
#ifndef NO_DRV_ADC
    "set adc_min 511\r\n"
    "set adc_max 512\r\n"
#endif
#ifndef BASIC
    "version\r\n"
    "switch <pin>\r\n"
    "show gpio\r\n"
#endif
    "set interval 50\r\n";

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
#ifdef DEBUG_DRV
    getInstanceOfProtocol()->debugf("callback: %s(%i,%i)\r\n", mode.c_str(), pin, value);
#endif
    getInstanceOfProtocol()->checkTrigger(pin, value);
}
void Protocol::driverOkCallbackEvent(String mode, int pin, int value)
{
    Driver *drv = getDrivers()->findByMode(mode);
    if (drv)
    {
#if defined(DEBUG_DRV)
        Serial.printf("driverOkCallback %s: %i set %i\r\n", mode, drv->getPin(), value);
#endif
        drv->writePin(value);
    }
#if defined(DEBUG_DRV)
    else
    {
        Serial.println(" nao encotrei.");
    }
#endif
}

#endif

void Protocol::prepare()
{
#ifdef SPIFFs
    if (!SPIFFS.begin())
    {
#else
#ifdef LITTLEFs
#ifdef ESP32
    if (!LITTLEFS.begin(true))
    {
#else
    if (!LittleFS.begin())
    {
        // LittleFS.format();
#endif
#endif
#endif

        Serial.println("FS mount failed");
        reset();
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
    getDrivers()->reload(); // mudou as configurações, recarregar os parametros;
#ifndef NO_API
    getApiDrivers().reload();
#endif
}

String Protocol::restoreConfig()
{

#if !(defined(ARDUINO_AVR) || defined(BASIC))
    String rt = "nao restaurou config";
    Serial.println("");
#endif
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
#if !(defined(ARDUINO_AVR) || defined(BASIC))
        rt = "OK";
#endif
    }
    catch (const char *e)
    {
        return String(e);
    }
#if !(defined(ARDUINO_AVR) || defined(BASIC))
    Serial.println("");
#endif
    afterConfigChanged();
#ifdef BASIC
    return "OK";
#else
    return rt;
#endif
}
#endif

DynamicJsonDocument Protocol::baseConfig()
{
    DynamicJsonDocument config = DynamicJsonDocument(1024);
    config["label"] = LABEL;
    config.createNestedObject("mode");
    config.createNestedObject("trigger");
    config.createNestedObject("stable");
    config.createNestedObject("timers");
#ifdef BOARD_NAME
    config["board"] = BOARD_NAME;
#else
    config["board"] = "ESP";
#endif
#ifndef ARDUINO_AVR

#ifndef BASIC
    config.createNestedObject("device");
    config.createNestedObject("sensor");
    config.createNestedObject("default");
    config.createNestedObject("scenes");
    config.createNestedObject("scene_triggers");
#endif

#ifndef NO_DRV_ADC
    config["adc_min"] = "125";
    config["adc_max"] = "126";
#endif
#ifndef BASIC
    config["sleep"] = "0";
#endif
#ifdef SSID_NAME
    config["ap_ssid"] = SSID_NAME;
    config["ap_password"] = SSID_PASWORD;
#else
    config["ap_ssid"] = "none";
    config["ap_password"] = "123456780";
#endif
#endif

#if defined(DEBUG_ON)
    config["debug"] = "term";
#else
    config["debug"] = "off";
#endif
    config["interval"] = "500";

#ifdef MQTTClient
    config["mqtt_host"] = "none"; //"test.mosquitto.org";
    config["mqtt_port"] = 1883;
    config["mqtt_user"] = "homeware";
    config["mqtt_password"] = "123456780";
    config["mqtt_interval"] = 30;
    config["mqtt_prefix"] = "homeware";
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

    base["debug"] = "off"; // volta para o default para sempre ligar com debug desabilitado
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
    apis += "telnet,";
    setupTelnet();
#endif
#ifndef ARDUINO_AVR
    afterBegin();
    debug("Resources: ");
    debug(resources);
    debug(" APIs: ");
    debugln(apis);
#endif
    inited = true;
#ifdef WIFI_ENABLED
    Serial.print("Memória livre: ");
    Serial.println(ESP.getFreeHeap());
    Serial.printf("IP: %s\r\n", localIP().c_str());
#endif
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
//    protocol->inTelnet = true;
#ifndef BASIC
    protocol->resetDeepSleep(60 * 5);
#endif
    Serial.print("Telnet: ");
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
        // protocol->inTelnet = false;
        protocol->telnet.disconnectClient();
    }
    else
    {
#ifndef BASIC
        protocol->resetDeepSleep();
#endif
        protocol->print(protocol->doCommand(str) + "\r\n");
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

String Protocol::show()
{
    char buffer[128];
    String ip = localIP();
    sprintf(buffer, "{ 'host':'%s' ,'version':'%s', 'name': '%s', 'ip': '%s'  }", hostname.c_str(), VERSION, config["label"].as<String>().c_str(), ip.c_str());
    return (String)buffer;
}

String Protocol::doCommand(String command)
{
#ifdef DEBUG_ON
    Serial.print("Command: ");
    Serial.println(command);
#endif
#ifndef BASIC
    if (command.startsWith("SERIAL "))
    {
        command.replace("SERIAL ", "");
        Serial.print(command);
        return "OK";
    }
    resetDeepSleep(60 * 5);
#endif
    String *cmd = split(command, ' ');
#ifdef LITTLEFs
    if (cmd[0] == "format")
    {
        LittleFS.format();
        return "formated";
    }
    else
#endif
#ifndef ARDUINO_AVR
#ifndef BASIC
        if (cmd[0] == "sleep" && String(cmd[1]).toInt() > 0)
    {
        doSleep(String(cmd[1]).toInt());
        return "OK";
    }
    else if (cmd[0] == "open")
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
#endif
        if (cmd[0] == "show")
    {
#if !(defined(ARDUINO_AVR) || defined(BASIC))

        Serial.println("cmd: " + command);

        if (cmd[1] == "resources")
            return "{'resources':'" + resources + "', 'apis': '" + apis + "'}";
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
        return show();

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
#ifndef BASIC
        print("reiniciando...");
        delay(1000);
#endif
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
#ifndef BASIC
    else if (cmd[0] == "debug" || cmd[0] == "term")
    {
        config["debug"] = cmd[1];
        return "OK";
    }
#endif
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
#ifndef BASIC
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
#endif
    else if (cmd[0] == "switch")
    {
        int pin = getPinByName(cmd[1]);
        return String(switchPin(pin));
    }
#ifndef BASIC
    else if (cmd[0] == "scene" && !cmd[3].isEmpty())
    {
        String sceneName = cmd[1]; // JsonObject Protocol::getTriggerScenes()
        if (cmd[2] == "set" && getTriggerScenes().containsKey(sceneName))
        {
            int sceneValue = String(cmd[3]).toInt();
            int trigPin = (getTriggerScenes()[sceneName].as<String>()).toInt();
            writePin(trigPin, sceneValue);
            return "OK";
        }
        else if (cmd[2] == "none")
        {
            getTriggerScenes().remove(sceneName);
            return "OK";
        }
        else if (cmd[2] == "trigger")
        {
            String trigPin = cmd[3];
            getTriggerScenes()[sceneName] = trigPin.toInt();
            return "OK";
        }
    }
#endif
    else if (cmd[0] == "gpio")
    {
        int pin = getPinByName(cmd[1]);
        String spin = String(pin);
        if (cmd[1] == "show")
        {
            return showGpio();
        }
#ifndef BASIC
        else if (cmd[2] == "scene")
        {

            String trigPin = cmd[1];
            getScenes()[cmd[3]] = trigPin.toInt();
            return "OK";
        }
#endif
        else if (cmd[2] == "get" || cmd[2] == "set" || cmd[2] == "status")
        {
            Driver *drv = getDrivers()->findByPin(pin);
            if (drv)
            {
#ifndef BASIC
                if (cmd[2] == "status")
                {
                    JsonObject j = drv->readStatus();
                    String rsp;
                    serializeJson(j, rsp);
                    return rsp;
                }
                else
#endif
#ifndef BASIC
                    if (cmd[2] == "get" && drv->isStatus())
                {
                    JsonObject sts = drv->readStatus();
                    String rsp;
                    serializeJson(sts, rsp);
                    return rsp;
                }
                else
#endif
                    if (cmd[2] == "get")
                {
#ifndef BASIC
                    return String(drv->internalRead());
#else
                    return String(drv->readPin());
#endif
                }
                else if (cmd[2] == "set")
                {
                    return String(drv->writePin(convertOnOff(cmd[3])));
                }
            }
        }

        if (cmd[2] == "none")
        {
            // getDrivers()->deleteByPin(pin);
            config["mode"].remove(spin);
            return "OK";
        }
#ifndef BASIC
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
        else if (cmd[2] == "timer")
        {
            if (cmd[3] == "none")
            {
                getTimers().remove(spin);
                return "OK";
            }
            else
            {
                getTimers()[spin] = String(cmd[3]).toInt();
                return "OK";
            }
        }
        else if (cmd[2] == "interval")
        {
            if (cmd[3] == "none")
            {
                getIntervals().remove(spin);
                return "OK";
            }
            else
            {
                getIntervals()[spin] = String(cmd[3]).toInt();
                return "OK";
            }
        }
#endif
        else if (cmd[2] == "mode")
        {
#ifndef ARDUINO_AVR

            if (resources.indexOf(cmd[3]) > -1)
            {
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
            return "OK";
        }

#ifndef BASIC
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
#endif // BASIC
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
            getStable()[spin] = (cmd[4].startsWith("bistable") ? 2 : 0) + (cmd[4].endsWith("NC") ? 1 : 0);

            return "OK";
        }
    }
    return "invalido";
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

#ifndef BASIC
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
#endif
bool inLooping = false;
void Protocol::loop()
{

    if (inLooping)
        return;
    inLooping = true;
    if (!inited)
        begin();
#ifndef BASIC
    lerSerial();
#endif
#ifdef TELNET
    telnet.loop(); // se estive AP, pode conectar por telnet ou pelo browser.
#endif
    eventLoop();

    getDrivers()->loop();
#ifndef NO_API
    getApiDrivers().loop();
#endif
#ifndef ARDUINO_AVR
#ifndef BASIC
    const int sleep = config["sleep"].as<String>().toInt();
    if (sleep > 0 && millis() > sleeptmp)
    {
        doSleep(sleep);
    }
#endif
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

#ifndef BASIC

void Protocol::afterLoop()
{
    // usado na herança
}
#endif
#endif

unsigned long interval = -1;
void Protocol::eventLoop()
{

#ifndef ARDUINO_AVR
    try
    {
#endif
        if (interval < 0 && containsKey("interval"))
        {
            interval = getKey("interval").toInt();
        }
        else
            interval = 500;
        if (interval < 50)
            interval = 50;
        if (millis() - eventLoopMillis > interval)
        {
            JsonObject mode = config["mode"];
            for (JsonPair k : mode)
            {
#ifdef DEBUG_ON
                Serial.printf("readPin(%s, %s)\r\n", k.key().c_str(), k.value().as<String>());
#endif
                readPin(String(k.key().c_str()).toInt(), k.value().as<String>());
            }
            eventLoopMillis = millis();
#ifndef ARDUINO_AVR
#ifndef BASIC
            afterLoop();
#endif
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
