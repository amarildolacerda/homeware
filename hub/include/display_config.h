#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

#ifdef HELTEC_W32LA
#define DISPLAY_SDA      4
#define DISPLAY_SCL      15
#define DISPLAY_RST      16
#else
#define DISPLAY_SDA      21
#define DISPLAY_SCL      22
#define DISPLAY_RST      -1
#endif
#define DISPLAY_ADDR     0x3C

#define DISPLAY_WIDTH    128
#define DISPLAY_HEIGHT   64

#define DISPLAY_PAGE_MS  5000

#endif
