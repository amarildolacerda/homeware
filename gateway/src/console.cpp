#include "console.h"
#include "config.h"

void ConsoleOutput::begin() {
    m_server.begin();
}

void ConsoleOutput::loop() {
    if (m_server.hasClient()) {
        if (m_client && m_client.connected()) {
            m_client.println("Novo cliente conectado, substituindo...");
            m_client.stop();
        }
        m_client = m_server.available();
        if (m_client) {
            m_client.setNoDelay(true);
            m_client.printf("\r\n=== " PLATFORM_PREFIX " Gateway %s ===\r\n", FW_VERSION);
            m_client.print("Console remoto conectado.\r\n");
            m_client.print("Comandos: h=ajuda, l=listar, p=pair, c=clear, r=restart, s=status\r\n");
        }
    }
    if (m_client && !m_client.connected()) {
        m_client.stop();
    }
}

int ConsoleOutput::telnet_available() {
    if (m_client && m_client.connected()) {
        return m_client.available();
    }
    return 0;
}

int ConsoleOutput::telnet_read() {
    if (m_client && m_client.connected()) {
        return m_client.read();
    }
    return -1;
}

size_t ConsoleOutput::write(uint8_t c) {
    Serial.write(c);
    if (m_client && m_client.connected()) {
        m_client.write(c);
    }
    return 1;
}

size_t ConsoleOutput::write(const uint8_t *buffer, size_t size) {
    Serial.write(buffer, size);
    if (m_client && m_client.connected()) {
        m_client.write((const char*)buffer, size);
    }
    return size;
}

ConsoleOutput console;
