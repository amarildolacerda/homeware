#include <Arduino.h>
#include "platform.h"
#include "dashboard.h"
#include "pages.h"
#include "shared_config.h"
#include "common_console.h"
#include "common_util.h"
#include "common_web.h"
#include "common_ota.h"
#include "config.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Update.h>
#else
#include <Updater.h>
#endif

// Dual Power — Powered Mode: web server (port 80) + ArduinoOTA (regra 28/29).
// Estado vem de main.cpp via AcqStatus; mudanças de config voltam via AcqConfigCb.

static MyWebServer s_server(80);
static const AcqStatus *s_status = nullptr;
static AcqConfigCb s_cb = {};

static void handle_root(void) { serve_pgm_page(s_server, PAGE_DASHBOARD); }
static void handle_docs(void) { serve_pgm_page(s_server, PAGE_DOCS); }

static void handle_api_state(void)
{
    String json;
    {
        JsonDocument doc;
        doc["value"] = s_status->value;
        doc["state"] = s_status->value != 0;
        if (s_status->label && s_status->label[0])
            doc["label"] = s_status->label;
        doc["battery"] = s_status->battery;
        doc["device_id"] = getDeviceId();
        doc["device_name"] = s_status->device_name;
        doc["fw_version"] = FW_VERSION;
        doc["platform"] =
#if defined(ARDUINO_ARCH_ESP32)
            "esp32";
#else
            "esp8266";
#endif
        doc["type"] = "acquiry";
        doc["paired"] = s_status->paired;
        doc["gateway_connected"] = s_status->paired;
        doc["slot"] = s_status->slot;
        doc["interval_s"] = s_status->interval_s;
        doc["tx_count"] = s_status->tx_count;
        doc["rx_count"] = s_status->rx_count;
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
        doc["wifi_channel"] = WiFi.channel();
        doc["uptime_s"] = millis() / 1000;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["power_mode"] = "powered";
        doc["last_send_s"] = s_status->last_send_ms
                                 ? (long)((millis() - s_status->last_send_ms) / 1000)
                                 : -1;
        serializeJson(doc, json);
    }
    s_server.send(200, "application/json", json);
}

static void handle_api_settings(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["device_name"] = s_status->device_name;
        doc["interval_s"] = s_status->interval_s;
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, s_server.arg("plain")))
    {
        s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    bool changed = false;
    if (!doc["device_name"].isNull())
    {
        const char *name = doc["device_name"] | "";
        if (name[0] && s_cb.on_name)
        {
            s_cb.on_name(name);
            changed = true;
        }
    }
    if (!doc["interval_s"].isNull())
    {
        int seconds = doc["interval_s"] | -1;
        if (seconds >= DEEP_SLEEP_INTERVAL_MIN && seconds <= DEEP_SLEEP_INTERVAL_MAX && s_cb.on_interval)
        {
            s_cb.on_interval(seconds);
            changed = true;
        }
        else
        {
            s_server.send(400, "application/json",
                          "{\"error\":\"interval out of range\"}");
            return;
        }
    }
    s_server.send(changed ? 200 : 400,
                  "application/json",
                  changed ? "{\"ok\":true}" : "{\"error\":\"nothing to change\"}");
}

static void handle_api_wifi(void)
{
    if (s_server.method() == HTTP_GET)
    {
        String json;
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID();
        serializeJson(doc, json);
        s_server.send(200, "application/json", json);
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, s_server.arg("plain")))
    {
        s_server.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    const char *ssid = doc["ssid"] | "";
    if (!ssid[0])
    {
        s_server.send(400, "application/json", "{\"error\":\"ssid required\"}");
        return;
    }
    const char *pass = doc["password"] | "";
    console.printf("[dashboard] WiFi update: %s\n", ssid);
    WiFi.persistent(true);
    WiFi.begin(ssid, pass);
    s_server.send(200, "application/json", "{\"ok\":true,\"msg\":\"connecting\"}");
}

static void handle_api_restart(void)
{
    s_server.send(200, "application/json", "{\"status\":\"restarting\"}");
    delay(200);
    ESP.restart();
}

static void handle_ota_upload(void)
{
    HTTPUpload &upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
        console.printf("[dashboard] OTA start: %s (%d bytes)\n",
                       upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(upload.totalSize))
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true))
            console.printf("[dashboard] OTA ok: %d bytes\n", upload.totalSize);
        else
            Update.printError(Serial);
    }
}

static void handle_ota(void)
{
    if (!Update.hasError())
    {
        s_server.send(200, "application/json", "{\"status\":\"ok\"}");
        delay(500);
        ESP.restart();
    }
    else
    {
        s_server.send(500, "application/json", "{\"status\":\"error\"}");
    }
}

void dashboard_begin(const AcqStatus *status, const AcqConfigCb *cb)
{
    s_status = status;
    if (cb)
        s_cb = *cb;
    s_server.on("/", handle_root);
    s_server.on("/docs", handle_docs);
    s_server.on("/api/state", handle_api_state);
    s_server.on("/api/settings", HTTP_ANY, handle_api_settings);
    s_server.on("/api/wifi", HTTP_ANY, handle_api_wifi);
    s_server.on("/api/ota", HTTP_POST, handle_ota, handle_ota_upload);
    s_server.on("/api/restart", HTTP_POST, handle_api_restart);
    s_server.begin();
    ota_setup(getDeviceId());
    console.printf("[dashboard] http://%s (docs: /docs)\n",
                   WiFi.localIP().toString().c_str());
}

void dashboard_loop(void)
{
    s_server.handleClient();
    ota_handle();
}
