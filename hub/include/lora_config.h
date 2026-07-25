#ifndef LORA_CONFIG_H
#define LORA_CONFIG_H

#ifdef MCU_TTGO
#define LORA_SS_PIN     18
#define LORA_RST_PIN    14
#define LORA_DIO0_PIN   26
#define LORA_DIO1_PIN   35
#define LORA_SS         LORA_SS_PIN
#define LORA_RST        LORA_RST_PIN
#define LORA_DIO0       LORA_DIO0_PIN
#define LORA_SCK        5
#define LORA_MISO       19
#define LORA_MOSI       27
#endif

#define LORA_FREQ       915.0
#define LORA_SF         10
#define LORA_BW         125
#define LORA_CR         7
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17

#endif
