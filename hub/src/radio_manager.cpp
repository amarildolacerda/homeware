#include "radio_manager.h"
#include "sensor_registry.h"
#include "common_console.h"
#include "log_buffer.h"


void RadioManager::add_radio(uint8_t radio_type, RadioInterface* radio) {
    if (m_count >= MAX_RADIOS) return;
    m_entries[m_count].type = radio_type;
    m_entries[m_count].radio = radio;
    m_count++;
}

void RadioManager::init_all() {
    for (int i = 0; i < m_count; i++) {
        int ret = m_entries[i].radio->init();
        console.printf("[radio] Radio %d initialized (ret=%d)\n", m_entries[i].type, ret);
        log_add("info", "Radio %d init ret=%d", m_entries[i].type, ret);
    }
}

void RadioManager::loop_all() {
    for (int i = 0; i < m_count; i++) {
        m_entries[i].radio->loop();
    }
}

bool RadioManager::send_command(uint8_t slot, uint8_t state) {
    virtual_sensor_t* s = sensor_registry_get(slot);
    if (!s || !s->paired) {
        log_add("warn", "send_command slot %d: not paired or invalid", slot);
        return false;
    }
    RadioInterface* r = get_radio(s->radio_type);
    if (!r) {
        log_add("warn", "send_command slot %d: no radio for type %d", slot, s->radio_type);
        return false;
    }
    log_add("info", "send_command slot %d: radio_type=%d state=%d", slot, s->radio_type, state);
    return r->send_command(s->mac, state);
}

bool RadioManager::send_restart(uint8_t slot) {
    virtual_sensor_t* s = sensor_registry_get(slot);
    if (!s || !s->paired) return false;
    RadioInterface* r = get_radio(s->radio_type);
    if (!r) return false;
    return r->send_restart(s->mac);
}

unsigned long RadioManager::total_rx_count() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_rx_count();
    return t;
}

unsigned long RadioManager::total_ack_count() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_ack_count();
    return t;
}

unsigned long RadioManager::total_crc_errors() const {
    unsigned long t = 0;
    for (int i = 0; i < m_count; i++)
        t += m_entries[i].radio->get_crc_errors();
    return t;
}

bool RadioManager::any_pairing_active() const {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].radio->is_pairing()) return true;
    return false;
}

bool RadioManager::any_start_pairing() {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].radio->start_pairing()) return true;
    return false;
}

void RadioManager::all_stop_pairing() {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->stop_pairing();
}

void RadioManager::all_announce() {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->announce();
}

void RadioManager::all_broadcast_time_sync(uint32_t epoch) {
    for (int i = 0; i < m_count; i++)
        m_entries[i].radio->broadcast_time_sync(epoch);
}

void RadioManager::all_broadcast_device_list() {
    for (int i = 0; i < m_count; i++) {
        m_entries[i].radio->broadcast_device_list();
    }
}

RadioInterface* RadioManager::get_radio(uint8_t radio_type) const {
    for (int i = 0; i < m_count; i++)
        if (m_entries[i].type == radio_type)
            return m_entries[i].radio;
    return nullptr;
}
