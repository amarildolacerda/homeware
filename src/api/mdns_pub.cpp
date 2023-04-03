#include "mdns_pub.h"

#include <ESP8266mDNS.h>

void mDNSPubDriver::setup()
{
    MDNS.begin(getInstanceOfProtocol()->hostname);
    MDNS.addService("http", "tcp", 80);
}
void mDNSPubDriver::loop()
{
    MDNS.update();
}
