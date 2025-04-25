#include "mdns_pub.h"

#ifndef ESP32

#ifdef ESP32
#include <mdns.h>
#else
#include <ESP8266mDNS.h>
#endif
#include "protocol.h"

void mDNSPubDriver::setup()
{
    MDNS.begin(Protocol::instance()->hostname);
    MDNS.addService("http", "tcp", 80);
}
void mDNSPubDriver::loop()
{
    MDNS.update();
}

#endif