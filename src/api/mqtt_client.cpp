#include "mqtt_client.h"

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>

WiFiClient espClient;
PubSubClient client(espClient);

MqttClientDriver *mqtt;

MqttClientDriver *getMqtt()
{
    return mqtt;
}

void callback(char *topic, byte *payload, unsigned int length)
{
#if defined(MQTT_DEBUG_ON) or defined(DEBUG_ON)
    Serial.println("Message arrived [");
    Serial.print(String(topic) + ": ");
#endif

    String cmd = "";
    for (int i = 0; i < length; i++)
        cmd += (char)payload[i];
    cmd.replace("\r", "");
    cmd.replace("\n", "");
#ifdef MQTT_DEBUG_ON
    Serial.printf("%i. %s\r\n", length, cmd.c_str());
#endif
    String *spl = split(topic, '/');
    String command = cmd;
    if (spl[1] == "scene")
    {
        command = "scene ";
        command += spl[2];
        command += " set ";
        command += cmd;
        if (spl[3].equals(mqtt->clientId)) // checa se quem enviou foi este dispositivo
        {
#if defined(MQTT_DEBUG_ON) or defined(DEBUG_ON)
            Serial.print("proprio: ");
            Serial.println(command);
#endif
            return;
        }
    }
    String result = getInstanceOfProtocol()->doCommand(command);
    mqtt->send("/response", result.c_str());
#if defined(MQTT_DEBUG_ON) or defined(DEBUG_ON)
    Serial.print(command);
    Serial.print(" rsp: ");
    Serial.println(result);
    Serial.print("] ");
    Serial.println();
#endif
}

void MqttClientDriver::init()
{
    DynamicJsonDocument config = getInstanceOfProtocol()->config;
    port = config["mqtt_port"].as<String>().toInt();
    host = config["mqtt_host"].as<String>();
    user = config["mqtt_user"].as<String>();
    password = config["mqtt_password"].as<String>();
    prefix = config["mqtt_prefix"].as<String>();
    name = config.containsKey("mqtt_name") ? config["mqtt_name"].as<String>() : config["label"].as<String>();
    interval = (config["mqtt_interval"].as<String>().toInt()) * 1000;
    // Serial.println(interval);
    if (interval < 100)
        interval = 60000;
    if (isEnabled())
    {
        client.setServer(host.c_str(), port);
        client.setCallback(callback);
    }
}

void MqttClientDriver::sendAlive()
{
    String rsp = getInstanceOfProtocol()->show();
#ifdef MQTT_DEBUG_ON
    Serial.println(rsp);
#endif
    send("/alive", rsp.c_str());
}
void MqttClientDriver::subscribes()
{
    char topic[132];
    sprintf(topic, "%s/scene/#", prefix.c_str());
    client.subscribe(topic);
    Serial.printf("Subscribe: %s, ", topic);
    sprintf(topic, "%s/%s/in", prefix.c_str(), name.c_str());
    client.subscribe(topic);
    Serial.printf("%s \r\n", topic);

    /* sprintf(topic, "%s/%s%s", prefix.c_str(), name.c_str(), "/in");
     client.subscribe(topic);
     char scene[132];
     sprintf(topic, "%s/scene/#", prefix.c_str());
     client.subscribe(scene);
    */
}

bool MqttClientDriver::isConnected(bool force)
{
    if (!isEnabled())
        return false;
    if (!client.connected() && (millis() - lastOne > 60000 || force))
    {
        Serial.print("MQT: Conectando ...");
        if (client.connect(clientId.c_str(), user.c_str(), password.c_str()))
        {
            Serial.println("connected");
            // sendAlive();
            subscribes();
            lastOne = millis();
        }
        else
        {
#ifdef MQTT_DEBUG_ON
            Serial.print("MQT: failed, rc=");
            Serial.print(client.state());
            Serial.println("MQT: try again in 10 seconds");
#endif
            lastOne = millis();
        }
    }
    return client.connected();
}

bool MqttClientDriver::isEnabled()
{
    return !host.equals("none");
}

void MqttClientDriver::changed(const String value)
{
    send("/action", value.c_str());
}

bool MqttClientDriver::sendScene(const char *scene, const int value)
{
    if (!isEnabled())
        return false;
#ifdef DEBUG_ON
    Serial.print("MQT: scene/");
    Serial.print(scene);
    Serial.print(" payload: ");
    Serial.println(value);
#endif
    if (isConnected())
    {
        lastAlive = millis();
        char topic[128];
        sprintf(topic, "%s/scene/%s/%s", prefix.c_str(), scene, clientId.c_str());
        char msg[32];
        sprintf(msg, "%i", value);
        int n = strnlen(msg, 32);
        client.publish(topic, msg, n);
        return true;
    }
    return false;
}

bool MqttClientDriver::send(const char *subtopic, const char *payload)
{
    if (!isEnabled())
        return false;
    try
    {
        if (isConnected(true))
        {
            lastAlive = millis();
            char topic[128];
            sprintf(topic, "%s/%s%s\0", prefix.c_str(), name.c_str(), subtopic);
            char msg[1024];
            sprintf(msg, "%s", payload);
            int n = strnlen(msg, 1024);
            client.publish(topic, msg, n);
            Serial.printf("MQT: %s %s\r\n",topic,msg);
            return true;
        }
    }

    catch (int &e)
    {
        Serial.println(e);
    }

    return false;
}

void MqttClientDriver::loop()
{
    if (isConnected())
    {
        if (millis() - lastAlive > interval)
            sendAlive();
        client.loop();
    }
}
void MqttClientDriver::reload()
{
    if (client.connected())
        client.disconnect();
    init();
}

void MqttClientDriver::setup()
{
    init();
    mqtt = this;
    if (isEnabled())
        Serial.println("MQT: <" + host + "> ");
    /*for (int i = 0; i < 5; i++)
    {
        Serial.print(".");
        if (isConnected())
            break;
        delay(5000);
    }
    Serial.println("");
    */
}