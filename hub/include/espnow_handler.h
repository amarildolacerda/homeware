#ifndef ESPNOW_HANDLER_H
#define ESPNOW_HANDLER_H

#include "espnow_protocol.h"
#include "sensor_registry.h"
#include <stdint.h>

bool espnow_handler_init();
void espnow_handler_loop();
bool espnow_start_pairing();
void espnow_stop_pairing();
bool espnow_is_pairing();
unsigned long espnow_pairing_remaining_ms();
unsigned long espnow_get_rx_count();
unsigned long espnow_get_ack_count();
unsigned long espnow_get_crc_errors();
uint8_t* espnow_get_gateway_mac();
const uint8_t* espnow_dest_for_chip(const uint8_t *mac, uint8_t client_chip);
bool espnow_send_command(const uint8_t *mac, uint8_t slot, uint8_t state);
bool espnow_send_restart(const uint8_t *mac, uint8_t slot);
void espnow_broadcast_time_sync(uint32_t epoch_seconds);
void espnow_announce();

#endif