#include "protocol.h"
#include <functions.h>

#define ledTimeout 200
bool ledStatus = false;
unsigned int ultimoLedChanged = millis();

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
