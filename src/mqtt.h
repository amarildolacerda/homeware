

#ifndef mqtt_h
#define mqtt_h

#include <functions.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <homeware.h>

WiFiClient mqttWifiClient;
PubSubClient mqttClient(mqttWifiClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

void callback(char *topic, byte *payload, unsigned int length);

class Mqtt
{
public:
    String prefix;
    String name;
    String user;
    String password;
    String host;
    int port;
    unsigned long interval = 1;
    String getTopic()
    {
        return prefix + "/" + name;
    }
    String getTopicIN()
    {
        return getTopic() + "/in";
    }
    void setup(const char *ahost, const int aport, const char *aprefix, const char *aname)
    {
        if (aname)
            name = aname;
        else
            name = String(getChipId(), HEX);
        Serial.println(String(getChipId(), HEX));
        prefix = aprefix;
        host = ahost;
        port = aport;
    }
    void setUser(const String user, const String pass)
    {
        this->user = user;
        this->password = pass;
    }
    void loop()
    {
        enabled = !((host == NULL) || (host == "none"));
        if (enabled)
        {
            reconnect();
            mqttClient.loop();
        }
        if (millis() - lastAlive > 60000)
            sendAlive();
    }
    void sendAlive()
    {
        lastAlive = millis();
        String rsp = homeware.doCommand("show");
        Serial.println(rsp);
        send("/alive", rsp.c_str());
    }
    bool isConnected()
    {
        return mqttClient.connected();
    }
    bool enabled = false;
    bool send(const char *subtopic, const char *payload)
    {
        if (!enabled)
            return false;
        try
        {
            if (isConnected())
            {
                char topic[128];
                sprintf(topic, "%s/%s%s", prefix, name, subtopic);
                char msg[1024];
                sprintf(msg, "%s", payload);
                mqttClient.publish(topic, msg);
                Serial.print("send: ");
                Serial.println(msg);
            }
        }

        catch (int &e)
        {
            Serial.println(e);
        }

        return false;
    }

private:
    unsigned long lastReconnect = 0;
    unsigned long lastAlive = 0;
    bool reconnect()
    {
        if (!enabled)
            return false;
        if (millis() - lastReconnect > 500)
            while (!isConnected())
            {
                lastReconnect = millis();
                Serial.print("MQTT connection...");
                String clientId = String(getChipId(), HEX);
                Serial.println(clientId);

                Serial.println(name.c_str());
                Serial.println(host.c_str());
                mqttClient.setServer(host.c_str(), port);

                if (mqttClient.connect(clientId.c_str()))
                {
                    Serial.println("connected");
                    char ip[32];
                    IPAddress x = homeware.localIP();
                    sprintf(ip, "%s", x.toString().c_str());
                    send("/ip", ip);
                    char subscribe[64];
                    sprintf(subscribe, "%s", getTopicIN().c_str());
                    Serial.println(subscribe);
                    mqttClient.subscribe(subscribe);
                    return true;
                }
                else
                {
                    Serial.println(" try again in 5 seconds");
                    return false;
                }
            }
        return true;
    }
};

extern Mqtt mqtt;
Mqtt mqtt;

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("Message arrived [");
    Serial.print(topic);
    String cmd = "";
    for (int i = 0; i < length; i++)
        cmd += (char)payload[i];
    Serial.println(cmd);
    String result = homeware.doCommand(cmd);
    mqtt.send("/response", result.c_str());
    Serial.print("] ");
    Serial.println();
}

#endif
