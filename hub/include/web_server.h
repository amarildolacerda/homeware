#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

void web_server_init();
void web_server_loop();
void web_server_handle_client();
bool web_server_wifi_setup(bool force_portal);
void web_server_maintain_wifi();

#endif