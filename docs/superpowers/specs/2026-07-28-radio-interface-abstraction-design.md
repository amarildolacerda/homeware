# RadioInterface Abstraction — Hub Multi-Rádio

**Data**: 2026-07-28
**Tags**: v1.1.0-radio-refactor
**Status**: Aprovado para implementação

## 1. Objetivo

Permitir que o hub compile seletivamente diferentes rádios (ESP-NOW, LoRa, futuros)
sem referências diretas a handlers concretos no `main.cpp`, `web_server.cpp` e
`device_router.cpp`. Cada rádio é uma classe que implementa `RadioInterface`
e se registra via `RadioManager`.

## 2. `RadioInterface` (shared/ — expandida)

Adicionar métodos virtuais opcionais com default no-op à interface existente:

```cpp
class RadioInterface {
public:
    virtual ~RadioInterface() {}

    virtual int init() = 0;
    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual void loop() = 0;
    virtual bool is_ready() const = 0;

    using rx_callback_t = void (*)(const uint8_t*, size_t, int16_t, void*);
    void set_rx_callback(rx_callback_t cb, void* arg = nullptr);

    // Operações opcionais (default no-op)
    virtual bool send_command(const uint8_t* mac, uint8_t state) { (void)mac; (void)state; return false; }
    virtual bool send_restart(const uint8_t* mac) { (void)mac; return false; }
    virtual unsigned long get_rx_count() const { return 0; }
    virtual unsigned long get_ack_count() const { return 0; }
    virtual unsigned long get_crc_errors() const { return 0; }
    virtual bool start_pairing() { return false; }
    virtual void stop_pairing() {}
    virtual bool is_pairing() const { return false; }
    virtual unsigned long pairing_remaining_ms() const { return 0; }
    virtual uint8_t* get_radio_mac() { return nullptr; }
    virtual void announce() {}
    virtual void broadcast_time_sync(uint32_t epoch) { (void)epoch; }
};
```

## 3. `EspnowHandler` (hub/)

Refatorar de funções globais para classe `EspnowHandler : public RadioInterface`.

- `espnow_handler_init()` → `EspnowHandler::init()`
- `espnow_handler_loop()` → `EspnowHandler::loop()`
- `send_ack()` / `send_pair_response()` / `send_gw_announce()` → métodos privados
- Fila de pareamento, contadores, MAC do gateway, estado de pareamento → variáveis de instância (não globais)
- Callback `espnow_recv_cb` → método estático que redireciona para instância via ponteiro global (`EspnowHandler* s_self`)
- `espnow_send_wrapper()` → mantido como função livre (ou `EspnowHandler::send_raw()`)

## 4. `LoraHandler` (hub/ — adaptação)

- `send_command()` implementado como método virtual
- `send_restart()` retorna false (não suportado)
- Demais métodos opcionais mantêm default (no-op)

## 5. `RadioManager` (hub/ — novo)

Gerencia instâncias e provê dispatch agregado.

```cpp
// radio_manager.h
class RadioManager {
public:
    void add_radio(uint8_t radio_type, RadioInterface* radio);

    void init_all();
    void loop_all();

    bool send_command(uint8_t slot, uint8_t state);   // dispatches por radio_type do slot
    bool send_restart(uint8_t slot);

    unsigned long total_rx_count();
    unsigned long total_ack_count();
    unsigned long total_crc_errors();

    bool any_pairing_active();
    bool any_start_pairing();
    void all_stop_pairing();
    void all_announce();
    void all_broadcast_time_sync(uint32_t epoch);

    RadioInterface* get_radio(uint8_t radio_type);
};
```

## 6. Fluxo no `main.cpp`

```cpp
#include "radio_manager.h"

#ifdef HABILITA_ESPNOW
#include "espnow_handler.h"
static EspnowHandler s_espnow;
#endif
#ifdef HABILITA_LORA
#include "lora_handler.h"
static LoraHandler s_lora;
#endif

static RadioManager s_radio_mgr;

void setup() {
    // ... WiFi, console, etc ...
#ifdef HABILITA_ESPNOW
    s_espnow.set_rx_callback(espnow_rx_cb, nullptr);
    s_radio_mgr.add_radio(RADIO_ESPNOW, &s_espnow);
#endif
#ifdef HABILITA_LORA
    s_lora.set_rx_callback(lora_rx_cb, nullptr);
    s_radio_mgr.add_radio(RADIO_LORA, &s_lora);
#endif
    s_radio_mgr.init_all();
    // ... MQTT ...
}

void loop() {
    // ... console, web, etc ...
    s_radio_mgr.loop_all();
    // ... telemetria via s_radio_mgr.total_rx_count() ...
    // ... time sync via s_radio_mgr.all_broadcast_time_sync() ...
}
```

## 7. Mudanças por arquivo

| Arquivo | Ação |
|---------|------|
| `shared/src/radio_interface.h` | Adicionar métodos opcionais |
| `hub/include/espnow_handler.h` | Reescrever como classe EspnowHandler : RadioInterface |
| `hub/src/espnow_handler.cpp` | Refatorar funções → métodos. Guard `#ifdef HABILITA_ESPNOW` |
| `hub/include/radio_manager.h` | Novo |
| `hub/src/radio_manager.cpp` | Novo |
| `hub/src/main.cpp` | Usar RadioManager. Sem include direto de espnow_handler. |
| `hub/src/device_router.cpp` | Usar RadioManager p/ dispatch. Remover `extern LoraHandler`. |
| `hub/src/web_server.cpp` | Usar RadioManager. |
| `hub/src/display_handler.cpp` | Usar RadioManager. |
| `hub/src/mqtt_client.cpp` | Remover `#include "espnow_handler.h"` (não usado). |
| `hub/platformio.ini` | `-DHABILITA_ESPNOW` nos envs ESP-NOW. Envs lora herdam de hub_32. |

## 8. Compilação condicional

| Arquivo | Guarda |
|---------|--------|
| `shared/radio_interface.h` | Sempre |
| `hub/include/espnow_handler.h` | `#ifdef HABILITA_ESPNOW` |
| `hub/src/espnow_handler.cpp` | `#ifdef HABILITA_ESPNOW` |
| `hub/radio_manager.h/.cpp` | Sempre |
| `hub/src/main.cpp` | Declaração e registro: `#ifdef HABILITA_ESPNOW` e `#ifdef HABILITA_LORA`. Uso do RadioManager: sempre. |
| `hub/src/device_router.cpp` | Sempre (dispatches via RadioManager) |
| `hub/src/web_server.cpp` | Sempre |
| `hub/src/display_handler.cpp` | Sempre |

## 9. Comportamento com ESP-NOW desabilitado

- `EspnowHandler` não compila
- `RadioManager` itera sobre 0 ou 1 radio (LoRa apenas)
- `total_rx_count()` retorna 0
- `any_pairing_active()` retorna false
- `send_command()` retorna false (sem radio que atenda)
- `/api/info` mostra rx=0, ack=0, pairing_mode=false
- Pairing endpoints retornam 400
- Display mostra RX=0 ACK=0
- Console `l` ainda lista sensores (registry é independente do rádio)
- Console `p` não faz nada

## 10. Ordem de implementação

1. Expandir `radio_interface.h` (shared)
2. Criar `radio_manager.h/.cpp` (hub)
3. Refatorar `espnow_handler` para classe EspnowHandler
4. Adaptar `LoraHandler` (adicionar send_command)
5. Atualizar `main.cpp`, `device_router.cpp`, `web_server.cpp`, `display_handler.cpp`
6. Atualizar `platformio.ini`
7. Remover include não usado em `mqtt_client.cpp`
8. Build test `hub_8266`, `hub_32`, `hub_32_lora_heltec`
