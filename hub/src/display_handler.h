#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#if defined(DISPLAY_TTGO) || defined(DISPLAY_HELTEC)
void display_handler_init(void);
void display_handler_loop(void);
#else
static inline void display_handler_init(void) {}
static inline void display_handler_loop(void) {}
#endif

#endif
