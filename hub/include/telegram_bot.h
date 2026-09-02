#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <Arduino.h>

// Initialize Telegram bot
void telegram_bot_init();

// Main loop - call from main.cpp loop()
void telegram_bot_loop();

// Send notification (used by other modules)
void telegram_send_alert(const char* level, const char* message);

// Notify lamp state change (feedback from node) - updates reply keyboard
void telegram_on_lamp_state_change(int slot);

// Check if Telegram is enabled and configured
bool telegram_is_enabled();

#endif
