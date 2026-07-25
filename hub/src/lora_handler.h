#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#define LORA_MSG_SENSOR_DATA   0x01
#define LORA_MSG_PAIR_REQUEST  0x02
#define LORA_MSG_PAIR_RESPONSE 0x03
#define LORA_MSG_HEARTBEAT     0x04
#define LORA_MSG_NAK           0x05
#define LORA_MSG_GW_ANNOUNCE   0x06
#define LORA_MSG_COMMAND       0x07

#define LORA_RX_BUF_SIZE 256

bool lora_handler_init(void);
void lora_handler_loop(void);

#endif
