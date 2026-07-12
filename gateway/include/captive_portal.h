#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>

void captive_portal_start();
void captive_portal_run();
void captive_dns_poll();
bool captive_portal_submitted();
void captive_portal_set_submitted();

#endif
