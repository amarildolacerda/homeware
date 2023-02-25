

#ifndef homeware_def
#define homeware_def

#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>

#include <options.h>

#ifdef WIFI_NEW
#include <WManager.h>
#else
#include <WiFiManager.h>
#endif

#include <ArduinoJson.h>


#ifdef ALEXA
#include <Espalexa.h>
#endif
#ifdef TELNET
#include "ESPTelnet.h"
#endif

#include <functions.h>

void linha();

class Homeware
{
public:
    void setServer(ESP8266WebServer *externalServer = nullptr);
    static constexpr int SIZE_BUFFER = 1024;
    DynamicJsonDocument config = DynamicJsonDocument(SIZE_BUFFER);
    IPAddress localIP();
#ifdef ALEXA
    Espalexa alexa = Espalexa();
#endif
#ifdef TELNET
    ESPTelnet telnet;
#endif
    String hostname = "AutoConnect";
    bool inited = false;
    bool connected = false;
    ESP8266WebServer *server;
    unsigned currentAdcState = 0;
    bool inDebug = false;
    void setup(ESP8266WebServer *externalServer = nullptr);
    void begin();
    void loop();
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    void initPinMode(int pin, const String m);
    void resetWiFi();

    JsonObject getTrigger();
    JsonObject getStable();
    JsonObject getMode();
    JsonObject getDevices();
    JsonObject getSensors();
    JsonObject getDefaults();

    int writePin(const int pin, const int value);
    int readPin(const int pin, const String mode = "");
    int switchPin(const int pin);
    void checkTrigger(int pin, int value);
    String help();
    bool readFile(String filename, char *buffer, size_t maxLen);
    String doCommand(String command);
    String print(String msg);
    void printConfig();
    String showGpio();

    void debug(String txt);
    int getAdcState(int pin);
    uint32_t getChipId();
    void errorMsg(String msg);
    JsonObject getValues();

private:
    void setupPins();
    void setupServer();
    void loopEvent();
#ifdef ALEXA
    void setupAlexa();
#endif
#ifdef TELNET
    void setupTelnet();
#endif
};

extern Homeware homeware;
extern ESP8266WebServer server;

#endif
