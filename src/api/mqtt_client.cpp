#include "mqtt_client.h"

#include <ESP8266WiFi.h>
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
#ifdef MQTT_DEBUG_ON
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
    String result = getInstanceOfProtocol()->doCommand(cmd);
    mqtt->send("/response", result.c_str());
#ifdef MQTT_DEBUG_ON
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
        interval = 10000;
    if (isEnabled())
    {
        client.setServer(host.c_str(), port);
        client.setCallback(callback);
    }
}

void MqttClientDriver::sendAlive()
{
    String rsp = getInstanceOfProtocol()->doCommand("show");
#ifdef MQTT_DEBUG_ON
    Serial.println(rsp);
#endif
    send("/alive", rsp.c_str());
}
void MqttClientDriver::subscribes()
{
    char topic[132];
    sprintf(topic, "%s/%s%s", prefix.c_str(), name.c_str(), "/in");
    client.subscribe(topic);
}

bool MqttClientDriver::isConnected()
{
    if (!isEnabled())
        return false;

    if (!client.connected() && (millis() - lastOne > 5000))
    {
#ifdef MQTT_DEBUG_ON
        Serial.print("MQT: Conectando ...");
#endif
        if (client.connect(clientId.c_str(), user.c_str(), password.c_str()))
        {
#ifdef MQTT_DEBUG_ON
            Serial.println("connected");
#endif
            sendAlive();
            subscribes();
            //            client.subscribe("inTopic");
            lastOne = 0;
        }
        else
        {
            Serial.print("MQT: failed, rc=");
            Serial.print(client.state());
            Serial.println("MQT: try again in 5 seconds");
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

bool MqttClientDriver::send(const char *subtopic, const char *payload)
{
    if (!isEnabled())
        return false;
    try
    {
        if (isConnected())
        {
            char topic[128];
            sprintf(topic, "%s/%s%s\0", prefix.c_str(), name.c_str(), subtopic);
            char msg[1024];
            sprintf(msg, "%s", payload);
            int n = strnlen(msg, 1024);
            client.publish(topic, msg, n);
            lastAlive = millis();
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
    isConnected();
}