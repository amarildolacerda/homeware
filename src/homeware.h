

#ifndef homeware_def
#define homeware_def

#ifdef ESP32
#include "WiFi.h"
#include "WebServer.h"
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include <options.h>

#include "homewareWiFiManager.h"

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
#ifdef ESP32
    void setServer(WebServer *externalServer = nullptr);
#else
    void setServer(ESP8266WebServer *externalServer = nullptr);
#endif
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
#ifdef ESP32
    WebServer *server;
#else
    ESP8266WebServer *server;
#endif
    unsigned currentAdcState = 0;
    bool inDebug = false;
#ifdef ESP32
    void setup(WebServer *externalServer = nullptr);
#else
    void setup(ESP8266WebServer *externalServer = nullptr);
#endif
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
#ifdef ESP32
    const char *getChipId();
#else
    uint32_t getChipId();
#endif
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
#ifdef ESP32
extern WebServer server;
#else
extern ESP8266WebServer server;
#endif

#endif
