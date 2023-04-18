#include "options.h"

#ifdef ARDUINO_AVR

#else

#include <Arduino.h>
#include "portal.h"
#include "functions.h"
// #include <wm_strings_pt_BR.h>
#include "homeware.h"

#ifdef ALEXA
#include "api/alexa.h"
#endif

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include "chart.h"

#ifdef TIMMED
#include <SimpleTimer.h>
SimpleTimer timer;
#endif

#ifdef WEBSOCKET
#include <WebSocketsServer.h>
WebSocketsServer webSocket = WebSocketsServer(81);
#endif

#ifdef ESP32
void Portal::setup(WebServer *externalServer)
#else
void Portal::setup(ESP8266WebServer *externalServer)
#endif
{
    server = externalServer;
    homeware.setLedMode(3);
}

bool timeout_reconect = millis();

#ifdef WEBSOCKET

uint8_t ws_num;
void debugCallbackFunc(String texto)
{
    webSocket.sendTXT(ws_num, texto);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    ws_num = num;
    switch (type)
    {
    case WStype_CONNECTED:

        webSocket.sendTXT(num, "Connected ");
        webSocket.sendTXT(num, homeware.resources);
        homeware.config["debug"] = "term";
#ifndef BASIC
        homeware.resetDeepSleep(60);
#endif

        break;
    case WStype_DISCONNECTED:
        homeware.config["debug"] = "off";
        break;

    case WStype_TEXT:
        if (length > 0)
        {
            char buf[128];
            sprintf(buf, "%s", payload);
            String rsp = homeware.doCommand(buf);
            webSocket.sendTXT(num, rsp);
        }
        break;
    }
}
#endif

void Portal::loop()
{
    if ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
    {
        homeware.setLedMode(5);
        if (millis() - timeout_reconect > 60000)
            homeware.reset();
        else
        {
            WiFi.setAutoReconnect(true);
            WiFi.reconnect();
            if (WiFi.status() != WL_CONNECTED)
            {
                homeware.setLedMode(0);
            }
        }
    }
    else
    {
        timeout_reconect = millis();
#ifdef WEBSOCKET
        webSocket.loop();
#endif
    }
}

void wifiCallback()
{
    homeware.loop();
}

#ifdef TIMMED
void sendUptime()
{
    homeware.loop();
}
#endif

void startWebSocket()
{
#ifdef WEBSOCKET
    homeware.debug("WebSocket: ");
    homeware.apis += "ws,";
    homeware.debugCallback = debugCallbackFunc;
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    homeware.debugln("OK");
#endif
}

void Portal::autoConnect(const String slabel)
{
    unsigned start = millis();
    unsigned timeLimitMsec = 20000;
#ifdef TIMMED
    int ntimer = timer.setInterval(1000, sendUptime);
#endif
#ifdef BOARD_NAME
    // String tmp = BOARD_NAME;
    // Serial.printf("Board: %s \r\n", BOARD_NAME);
#endif
    label = slabel;
    if (homeware.config.containsKey("password") && homeware.config.containsKey("ssid"))
    {
        homeware.debug("Conectando na rede: ");
#ifndef BASIC
        homeware.resetDeepSleep();
#endif
        WiFi.enableSTA(true);
        WiFi.setAutoReconnect(true);
        WiFi.begin(homeware.config["ssid"], homeware.config["password"].as<String>().c_str());
        Serial.println(homeware.config["ssid"].as<String>());
        while (WiFi.status() != WL_CONNECTED && millis() - start < timeLimitMsec)
        {
            delay(500);

            Serial.print(".");
        }
        Serial.println();
    }

#ifdef NO_WM

    if (WiFi.status() != WL_CONNECTED)
    {

        Serial.printf("Criando ponto de acesso (%s) ", label);
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_AP);
        if (WiFi.softAP(label, homeware.config["ap_password"].as<String>().c_str()))
            Serial.println(WiFi.softAPIP());
        else
        {
            Serial.println("falha ao criar o ponto de acesso");
            delay(1000);
            homeware.reset();
        }
    }
    else
    {
        homeware.debugln(homeware.localIP());
    }

#else
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (!connected)
    {
        WiFi.mode(WIFI_AP_STA);
        WiFi.setHostname(homeware.hostname.c_str());
#ifndef BASIC
        homeware.resetDeepSleep(5);
#endif
        wifiManager.setConfigPortalTimeout(180);
        wifiManager.setDebugOutput(false);
        wifiManager.setConfigWaitingcallback(wifiCallback);
        wifiManager.setConnectTimeout(30);
        if (homeware.config["ap_ssid"] != "none")
        {
            connected = wifiManager.autoConnect(homeware.config["ap_ssid"], homeware.config["ap_password"]);
        }
        else
            connected = wifiManager.autoConnect(homeware.hostname.c_str());

        if (connected)
        {
            WiFi.enableAP(false);
        }
    }

#ifdef SEM_USO
    homeware.connected = connected;
#endif
    if (!connected)
    {
        homeware.reset();
    }
#endif

#ifndef NO_WM
    setupServer();
#endif

#ifdef TIMMED
    timer.deleteTimer(ntimer);
#endif

    startWebSocket();
}

void Portal::reset()
{
    homeware.resetWiFi();
}

#ifndef NO_WM
String button(const String name, const char *link, const String style = "")
{
    return stringf("<br/><form action='%s' method='get'><button %s>%s</button></form>", link, (style.isEmpty()) ? "" : String(stringf("class='%s'", style)), name);
}
String inputH(const String name, const String value)
{
    return String(stringf("<input type=\"hidden\" name=\"%s\" value=\"%s\">", name, value));
}

void setManager(HomewareWiFiManager *wf)
{
    String hostname = portal.label;
#ifdef WIFI_NEW
    // wf->setHostname(hostname);
    // wf->setTitle("Homeware");
#endif
}

const char BUTTON_SCRIPT[] PROGMEM =
    "\r\nfunction buttonClick(buttonId, pinNumber)"
    "{"
    "\r\nconsole.log(buttonId,pinNumber);"
    "\r\nvar xhr = new XMLHttpRequest();"
    "\r\nxhr.onreadystatechange = function()"
    "{"
    "\r\nif (xhr.readyState == 4 &&xhr.status ==200)"
    "{"
    "\r\nvar button = document.getElementById(buttonId);"
    "\r\nbutton.innerText = (xhr.responseText=='1')?'ON':'OFF';"
    "\r\nbutton.value = xhr.responseText;"
    "\r\nconsole.log('response',xhr.responseText);"
    "\r\n}"
    "};"
    "\r\nvar button = document.getElementById(buttonId);"
    "\r\nvar lnk = '/switch?p='+pinNumber; lnk+='&q='; \r\n lnk+=(button.value=='1')?'OFF':'ON';"
    "\r\nconsole.log(lnk);"
    "\r\nxhr.open('GET', lnk , true);"
    "\r\nxhr.send();"
    "\r\n}";

#ifdef WEBSOCKET
const char HTTP_TERM[] PROGMEM =
    "<div class='chatbox'><input type='text' id='commandInput'/></div><button id='button'>enviar</button>"
    "<div class='chatbox-messages' id='responseText'></div></div>"
    "<script>"
    "var gateway = `ws://{ip}:81/`;"
    "var websocket;"
    "function initWebSocket()"
    "{"
    "console.log('Trying to open a WebSocket connection...');"
    "websocket = new WebSocket(gateway);"
    "websocket.onopen = onOpen;"
    "websocket.onclose = onClose;"
    "websocket.onmessage = onMessage; "
    "} "
    "function onOpen(event)"
    "{"
    "console.log('Connection opened');"
    "} "

    "function onClose(event)"
    "{"
    "console.log('Connection closed');"
    "setTimeout(initWebSocket, 2000);"
    "} "
    "function onMessage(event)"
    "{"
    "response(event.data);"
    "} "
    "window.addEventListener('load', onLoad);"
    "function onLoad(event)"
    "{"
    "initWebSocket();"
    "initButton();"
    "}"

    "function initButton()"
    "{"
    "document.getElementById('button').addEventListener('click', send);"

    "const input = document.getElementById('commandInput');"
    "input.addEventListener("
    "'keydown', function(event) {"
    "if (event.key === 'Enter')"
    "{"
    "   document.getElementById('button').click();"
    "}"
    "});"

    "}"
    "function ask(txt)"
    "{"
    " var r =document.getElementById('responseText'); r.innerHTML  = '<div class=\"message-ask\">'+txt + '</div><br>';"
    "}"
    "function response(txt)"
    "{"
    " var r =document.getElementById('responseText'); r.innerHTML  += '<div class=\"message\">'+txt + '</div><br>';"
    "}"
    "function send()"
    "{"
    "var elem =document.getElementById('commandInput');"
    "var txt = elem.value;"
    "ask(txt);"
    "websocket.send(txt);"
    "elem.value = '';"
    "elem.focus();"
    "}"
    "</script>";
char HTTP_CUSTOM_HEAD[] PROGMEM =
    "<style>"
    ".chatbox-messages{min-width:calc(100%-50px); max-width:calc(100%-50px);height:calc(100%-80px);overflow:auto;}"
    ".message-ask{text-align: right;padding:1px;position:relative;right:0;background-color:#d2d2d2}"
    ".message{padding:1px;}"
    ".message:nth-child(even){background-color:#f2f2f2}"
    ".chatbox-input {position:absolute;bottom:0;width:100%;}"
    "</style>";

#endif
void Portal::setupServer()
{
    Serial.println("");
    homeware.debug("Criando ServerPage: ");

    server->on("/", []()
               {

#ifndef BASIC
        homeware.resetDeepSleep(5);
#endif
        HomewareWiFiManager wf;
        setManager(&wf);
        String head = "";
        head += FPSTR(HTTP_CHART_HEADER);

        String pg = "";
        JsonObject mode = homeware.getMode();
        String charts = "";
        String scp = "";
        for (JsonPair k : mode)
        {
            String md = k.value().as<String>();
            String p1 = k.key().c_str();
            String v1 = k.value().as<String>();
            if (md == "out" || md == "lc")
            {
                int v = homeware.readPin(p1.toInt(), v1);
                String s = (v == 1) ? "ON" : (v > 0) ? String(v)
                                                     : "OFF";
                String j = "<br/><button id='b{id}' class='D' value='{v}'>" + s + "</button></form>"; 
                j.replace("{id}", p1);
                j.replace("{v}",String(v));
                pg += j;
                j = "\r\nvar b{id} = document.getElementById('b{id}');\r\n";
                j.replace("{id}", p1);
                scp += j;
                j = "\r\n b{id}.addEventListener('click', function() { buttonClick('b{id}', '{id}');});\r\n";
                j.replace("{id}", p1);
                scp += j;
        }

        // chart
        if (md == "adc" || md == "ldr" || md == "pwm" || md == "dht")
        {
            Chart chart = Chart();
            charts += chart.render("p" + p1, "Pin " + p1 + " - " + v1, "valor", "/get?p=" + p1);
        }
        }
        pg += charts;

        pg += button("GPIO", "/gs");
        pg += button("Reiniciar","/reset");
        pg += button("Terminal", "/term");
        pg += "<div style='height:100'></div><a href=\"/update\" class='D'>firmware</a><div class='msg'> Ver: {ver}</div>";
        pg.replace("{ver}",VERSION);

        if (scp.length() > 0)
        {
        pg += "<script>\r\n";
        pg += FPSTR(BUTTON_SCRIPT);
            pg += scp;

            pg += "\r\n</script>\r\n";
        }
        wf.setCustomHeadElement(head.c_str());

        portal.server->send(200, "text/html", wf.pageMake("Homeware", pg)); });

    server->on("/switch", []()
               {
                String p = portal.server->arg("p");
                int rsp = homeware.switchPin(p.toInt());
                portal.server->send(200, "text/plain", String(rsp)); });

    server->on("/set", []()
               {
                //if (portal.server->hasArg("p") && portal.server->hasArg("q")){
                String p = portal.server->arg("p");
                String q = portal.server->arg("q");
                int rsp = homeware.writePin(p.toInt(), (q == "ON") ? 1 : 0);
                portal.server->send(200, "text/plain", String(rsp)); });
    server->on("/get", []()
               { 
               String p = portal.server->arg("p");
               int v = homeware.readPin(p.toInt(), homeware.getMode()[p].as<String>());
               Serial.print(p+": ");
               Serial.println(v);
               portal.server->send(200, "text/plain", String(v)); });
    homeware.server->on("/reset", []()
                        {
                            HomewareWiFiManager wf;
                            portal.server->send(200, "text/html", wf.pageMake("Homeware", "<a href='/'>reiniciando...</>"));
                            delay(100);
                            homeware.doCommand("reset"); });

    server->on("/gs", []()
               {
        HomewareWiFiManager wf ;
        setManager(&wf);

        String pg = "GPIO Status<hr>";
        pg+="<table style=\"width:80%\">";
        pg+="<thead><tr><th>Pin</th><th>Mode</th><th>Value</th></tr></thead><tbody>";
        JsonObject mode = homeware.getMode();
        for (JsonPair k : mode)
        {
            String p1 = k.key().c_str();
            String v1 = k.value().as<String>();
            int v = homeware.readPin(p1.toInt(), v1);
            String s = (v==1)?"ON": (v>1)?  String(v):"OFF";

            pg += stringf("<tr><td>%s</td><td>%s</td><td>%s</td></tr>", p1, v1,s);
        }

        pg+="</tbody></table>";
        pg += button("Menu","/");
        portal.server->send(200, "text/html", wf.pageMake("Homeware",pg)); });
#ifdef WEBSOCKET
    server->on("/term", []()
               {
                   HomewareWiFiManager wf;
                   String hd = FPSTR(HTTP_CUSTOM_HEAD);
                   
                    wf.setCustomHeadElement(hd.c_str());
                   setManager(&wf);
                   String pg = "Terminal<hr>";
                   pg += FPSTR(HTTP_TERM);
                   pg.replace("{ip}",homeware.localIP() );
                   portal.server->send(200, "text/html", wf.pageMake("Homeware", pg)); });
#endif
#ifdef ALEXA
    server->onNotFound([]()
                       {
	if (!Alexa::getAlexa()->handleAlexaApiCall(homeware.server->uri(),homeware.server->arg(0)))
	{
        homeware.server->send(404, "text/plain", "Not found");
    } });
#endif
    homeware.debugln("OK");
}
#endif

Portal portal;
#endif