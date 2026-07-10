#include "ota.h"
#include "console.h"
#include <ArduinoOTA.h>

void ota_init(const char *hostname) {
    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        console.printf("[OTA] Start updating %s\n", type.c_str());
    });
    ArduinoOTA.onEnd([]() {
        console.println("\n[OTA] End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        console.printf("[OTA] Progress: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        console.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) console.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) console.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) console.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) console.println("Receive Failed");
        else if (error == OTA_END_ERROR) console.println("End Failed");
    });
    ArduinoOTA.begin();
    console.printf("[OTA] Ready: %s.local\n", hostname);
}

void ota_loop() {
    ArduinoOTA.handle();
}