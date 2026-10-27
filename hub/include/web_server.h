#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include "platform.h"

extern MyWebServer s_server;

void web_server_init();
void web_server_loop();
void web_server_handle_client();
bool web_server_wifi_setup(bool force_portal);
void web_server_maintain_wifi();

// Agenda restart via web_server_loop() — nunca chamar ESP.restart() direto
// em handlers async (corre com o flush da resposta HTTP).
void web_server_request_restart(unsigned long delay_ms);

int getModeOperStrategy();
int op_mode_load();
void op_mode_save(int mode);

#endif