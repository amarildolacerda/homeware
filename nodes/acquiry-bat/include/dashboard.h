#pragma once
#include <Arduino.h>

// Dual Power Mode — Powered Mode: dashboard web + OTA.
// Este módulo é o dono do web server (port 80) e do ArduinoOTA.
// main.cpp mantém AcqStatus atualizado e responde às mudanças de config via callbacks.

// Estado do node publicado em /api/state (mantido por main.cpp).
struct AcqStatus {
    uint8_t value;              // estado da aquisição (0/1)
    const char *label;          // label corrente ("aberto"/"fechado")
    int battery;                // %
    bool paired;                // registrado no hub
    uint8_t slot;
    const char *device_name;    // buffer estático de main.cpp
    int interval_s;             // intervalo de ciclo/deep sleep (s)
    uint32_t tx_count;
    uint32_t rx_count;
    unsigned long last_send_ms; // millis() do último envio (0 = nunca)
};

// Callbacks de configuração — implementados em main.cpp (persistência + estado local).
struct AcqConfigCb {
    void (*on_name)(const char *name);    // nome validado não vazio
    void (*on_interval)(int seconds);     // intervalo validado (MIN..MAX)
};

void dashboard_begin(const AcqStatus *status, const AcqConfigCb *cb);
void dashboard_loop(); // server.handleClient() + OTA handle — chamar em todo loop()
