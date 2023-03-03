

#ifndef homeware_def
#define homeware_def

#ifdef ESP32
#include "WiFi.h"
#include "WebServer.h"
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include "homewareWiFiManager.h"
#include <options.h>

#include <ArduinoJson.h>

#ifdef ALEXA
#include <Espalexa.h>
#endif

#include <functions.h>

#include "protocol.h"

const unsigned long timeoutDeepSleep = 10000;

void linha();

class Homeware : public Protocol
{
public:
#ifdef ESP32
    void setServer(WebServer *externalServer = nullptr);
#else
    void setServer(ESP8266WebServer *externalServer = nullptr);
#endif
    IPAddress localIP();
#ifdef ALEXA
    Espalexa alexa = Espalexa();
#endif
    bool inited = false;
    bool connected = false;
#ifdef ESP32
    WebServer *server;
#else
    ESP8266WebServer *server;
#endif
    bool inDebug = false;
#ifdef ESP32
    void setup(WebServer *externalServer = nullptr);
#else
    void setup(ESP8266WebServer *externalServer = nullptr);
#endif
    //===================== revisado para ficar aqui mesmo
    virtual int readPin(const int pin, const String mode = "");
    virtual void afterChanged(const int pin, const int value, const String mode);

    //============================= potencial para mudar para Protocol
    void begin();
    void loop();
    String restoreConfig();
    void defaultConfig();
    String saveConfig();
    void resetWiFi();
    void resetDeepSleep(const unsigned int t = 60000);

    int switchPin(const int pin);
    void checkTrigger(int pin, int value);
    String getStatus();
    String help();
    void setLedMode(const int mode);
    bool readFile(String filename, char *buffer, size_t maxLen);
    String doCommand(String command);
    void printConfig();
    String showGpio();

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
    void setupSensores();
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
