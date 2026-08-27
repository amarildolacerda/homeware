#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include "radio_interface.h"
#include <stdint.h>

#define MAX_RADIOS 4

class RadioManager {
public:
    void add_radio(uint8_t radio_type, RadioInterface* radio);

    void init_all();
    void loop_all();

    bool send_command(uint8_t slot, uint8_t state);
    bool send_restart(uint8_t slot);

    unsigned long total_rx_count() const;
    unsigned long total_ack_count() const;
    unsigned long total_crc_errors() const;

    bool any_pairing_active() const;
    bool any_start_pairing();
    void all_stop_pairing();
    void all_announce();
    void all_broadcast_time_sync(uint32_t epoch);
    void all_broadcast_device_list();

    RadioInterface* get_radio(uint8_t radio_type) const;

    int get_count() const { return m_count; }
    uint8_t get_type(int idx) const { return idx < m_count ? m_entries[idx].type : 0; }

private:
    struct RadioEntry {
        uint8_t type;
        RadioInterface* radio;
    };
    RadioEntry m_entries[MAX_RADIOS];
    int m_count = 0;
};

#endif
