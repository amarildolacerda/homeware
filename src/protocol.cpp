#include "protocol.h"
#include <functions.h>
#include <LittleFS.h>

#define ledTimeout 200
bool ledStatus = false;
unsigned int ultimoLedChanged = millis();
unsigned int timeoutDeepSleep = 10000;

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
    if (m == "in" || m == "rst" || m == "ldr" || m == "dht")
        pinMode(pin, INPUT);
    else if (m == "out" || m == "led")
        pinMode(pin, OUTPUT);
    JsonObject mode = getMode();
    mode[String(pin)] = m;
}

int Protocol::writePin(const int pin, const int value)
{
    String mode = getMode()[String(pin)];
    int v = value;
    if (mode != NULL)
        if (mode == "pwm")
        {
            analogWrite(pin, value);
            digitalWrite(pin, value > 0);
        }
#ifdef DHT_SENSOR
        else if (mode == "dht")
        {
            return value;
        }
#endif
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

int Protocol::writePWM(const int pin, const int value, const int timeout)
{
    if (value == 0)
    {
        digitalWrite(pin, LOW);
    }
    else
    {
        analogWrite(pin, value);
        if (value > 0 && timeout > 0)
        {
            const int entrada = millis();
            digitalWrite(pin, HIGH);
            delay(timeout);
            digitalWrite(pin, LOW);
            return millis() - entrada;
        }
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

bool Protocol::pinValueChanged(const int pin, const int newValue)
{
    if (pinValue(pin) != newValue)
    {
        char buffer[32];
        sprintf(buffer, "pin %d : %d ", pin, newValue);
        debug(buffer);
        docPinValues[String(pin)] = newValue;
        afterChanged(pin, newValue, getPinMode(pin));
        return true;
    }
    return false;
}

int Protocol::readPin(const int pin, const String mode)
{
    int newValue = 0;
    if (mode == "led")
    {
        newValue = ledLoop(pin);
    }
    else if (mode == "rst" && newValue == 1)
    {
        reset();
    }
    else if (mode == "pwm")
    {
        newValue = analogRead(pin);
    }
    else if (mode == "adc")
    {
        newValue = analogRead(pin);
    }
    else if (mode == "lc")
    {
        newValue = docPinValues[String(pin)];
    }
    else if (mode == "ldr")
    {
        newValue = getAdcState(pin);
    }
    else
        newValue = digitalRead(pin);

    pinValueChanged(pin, newValue);

    return newValue;
}

void Protocol::debug(String txt)
{
    const bool erro = txt.indexOf("ERRO") > -1;
    if (config["debug"] == "on" || erro)
    {
        print(txt);
    }
    else if (config["debug"] == "term" || erro)
        Serial.println(txt);
}
int Protocol::ledLoop(const int pin)
{
    ledPin = pin;
    if (pin == 255 || pin < 0)
        ledPin = findPinByMode("led");
    if (ledPin > -1)
    {
        if (pin < 0)
        {
            ledStatus = false;
            digitalWrite(ledPin, ledStatus);
        }
        else
        {
            if (millis() - ultimoLedChanged > ledTimeChanged)
            {
                ledStatus = !ledStatus;
                digitalWrite(ledPin, ledStatus);
                ultimoLedChanged = millis();
            }
            else if (ledStatus && millis() - ultimoLedChanged > ledTimeout)
            {
                ledStatus = false;
                digitalWrite(ledPin, ledStatus);
            }
        }
    }
    return ledStatus;
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
    Serial.print("RSP: ");
    Serial.println(msg);
#ifdef TELNET
    telnet.println(msg);
#endif
    return msg;
}

int Protocol::getAdcState(int pin)
{

    unsigned int tmpAdc = analogRead(pin);
    int rt = currentAdcState;
    const unsigned int v_min = config["adc_min"].as<int>();
    const unsigned int v_max = config["adc_max"].as<int>();
    if (tmpAdc >= v_max)
        rt = LOW;
    if (tmpAdc < v_min)
        rt = HIGH;
    char buffer[64];
    sprintf(buffer, "adc %d,currentAdcState %d, adcState %s  (%i,%i) ", tmpAdc, currentAdcState, (rt > 0) ? "ON" : "OFF", v_min, v_max);
    debug(buffer);
    currentAdcState = rt;
    return rt;
}

void Protocol::checkTrigger(int pin, int value)
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

int Protocol::switchPin(const int pin)
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

void Protocol::setLedMode(const int mode)
{
    ledTimeChanged = (5 - (mode <= 5) ? mode : 4) * 1000;
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

void Protocol::setupPins()
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

unsigned long sleeptmp = millis() + timeoutDeepSleep;
void Protocol::resetDeepSleep(const unsigned int t)
{
    unsigned int v = millis() + (timeoutDeepSleep * t);
    if (v > sleeptmp)
    {
        sleeptmp = v;
        Serial.println(sleeptmp);
    }
}

const char sleeping[] = "Sleeping: ";
void Protocol::doSleep(const int tempo)
{
    if (millis() > sleeptmp)
    {
        Serial.print(FPSTR(sleeping));
        Serial.println(tempo);
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
void Protocol::setup()
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

void Protocol::defaultConfig()
{
    DynamicJsonDocument doc = baseConfig();
    for (JsonPair k : doc.as<JsonObject>())
    {
        String key = k.key().c_str();
        config[key] = doc[key];
    }
}
