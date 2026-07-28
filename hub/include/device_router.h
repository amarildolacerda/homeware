#ifndef DEVICE_ROUTER_H
#define DEVICE_ROUTER_H

#include <stdint.h>

bool device_send_command(const uint8_t *mac, uint8_t slot, uint8_t state);
bool device_send_restart(const uint8_t *mac, uint8_t slot);

#endif
