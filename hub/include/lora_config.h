#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

// TTGO LORA32 T3_v1.6 — SX1278 via sandeepmistry/LoRa
#define LORA_SS     18
#define LORA_RST    14
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27

// Legacy aliases (compatibilidade)
#define LORA_SS_PIN   LORA_SS
#define LORA_RST_PIN  LORA_RST
#define LORA_DIO0_PIN LORA_DIO0

// Radio parameters
#define LORA_FREQ       868.0
#define LORA_SF         10
#define LORA_BW         125
#define LORA_CR         7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17

#endif
