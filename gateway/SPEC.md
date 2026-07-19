# Spec: Fluxo 

## Geral ##
1. entra em estado de integração com o Bridge (HA Bridge)
2. entra em estado de esperar dados dos clientes;
3. ao aceitar um cliente, manter persistencia do seu estado
4. renovar os dados do bridge em um intevalo que não estrangule a comunicação com o bridge
5. lembra os clientes pareados ao iniciar
6. permite navegação pelo IP aos dados do clietes quando conhecido
7. clientes que não mandam mensagem a muito tempo (1dia), limpar da lista de pareados
8. manter no dashboard <title>xx<title/> que permita localizar o IP usando scan.py
9. para fazer flash OTA, pode tentar localiar o IP usando o scan.py comparando o <title/>

## Protocolo ESP-NOW (pair request)
- `espnow_pair_request_t.device_name`: **32 bytes** (compatível com `s_device_name[32]` dos clients)
- `virtual_sensor_t.name`: **32 bytes**
- `pending_pair_t.name`: **32 bytes**
- EEPROM_SENSOR_SIZE: **48 bytes** por sensor (nome ocupa32 bytes no offset9)
- Qualquer mudança no tamanho de `device_name` exige atualização simultânea de gateway e todos os clients


# Spec: Botão "Adicionar Sensor" — Gateway Dashboard

## Visão Geral

O botão **"+ Adicionar Sensor"** no dashboard do gateway ESP-NOW ativa o modo de pareamento, permitindo que novos sensores virtuais sejam descobertos e registrados via protocolo ESP-NOW.
O botão "+ Adicionar Sensor" tem dupla ação, ativa/cancela estado de paring - quando estiver em pareando mostrar quando falta para esgotar o tempo.

## Comportamento Esperado

### 1. Estado Inicial (Idle)
- Botão exibe `+ Adicionar Sensor`  ou `Cancelar (xs)`
- Classe CSS: `btn btn-primary`
- Nenhum timer ativo (`pairingTimer === null`)


### 2. Clique — Iniciar Pareamento
Ao clicar, o fluxo é:

```
enterPairingMode()
  ├─ POST /api/pair/start
  │   ├─ 200 → startPairingCountdown()
  │   ├─ 409 (already pairing) → startPairingCountdown() -> muda titulo do botão
  │   └─ 400 (max sensors) → showToast(erro)
  └─ Erro de rede → showToast(erro)
```

### 3. Estado Pareando (Pairing Active)
- Botão exibe `✕ Pareando (Xs)` com contagem regressiva
- Classe CSS: `btn btn-pairing` (fundo amarelo + animação pulse)
- Timer `setInterval(1s)` decrementa o contador
- Janela de pareamento padrão: **180 segundos** (configurável via `/api/info.pairing_window_sec`)
- Backend: `espnow_start_pairing()` define `s_pairing_mode = true`, LED status LOW

### 4. Clique Durante Pareamento (Toggle)
- Chama `exitPairingMode()` → interrompe pareamento
- Envia `POST /api/pair/stop`
- Restaura botão ao estado idle

### 5. Timeout Automático
- Quando contador chega a 0, chama `exitPairingMode()`
- Backend: `espnow_handler_loop()` também timeout após `PAIRING_WINDOW_MS` (60s)

### 6. Sensores Pareados During Modo
- Sensor envia `ESPNOW_MSG_PAIR_REQUEST` via ESP-NOW
- Gateway encontra slot livre → `sensor_registry_add()` + `send_pair_response()`
- Se gateway descoberto (`bridge_client_is_discovered()`), registra no bridge via HTTP

## Endpoints Relacionados

| Método | Path | Descrição | Respostas |
|--------|------|-----------|-----------|
| POST | `/api/pair/start` | Inicia pareamento | 200 ok, 409 already pairing, 400 max sensors |
| POST | `/api/pair/stop` | Para pareamento | 200 ok |
| GET | `/api/info` | Retorna `pairing_mode`, `pairing_window_sec` | 200 JSON |

## Estados do Botão

```
[IDLE] ──click──▶ [PAIRING] ──click──▶ [IDLE]
   ▲                  │
   └──── timeout ─────┘
```

## Validações

- Se já está pareando (409), retoma o countdown no frontend
- Se max sensors atingido (400), mostra toast de erro
- Se erro de rede, mostra toast de erro
- `pairingWindowSec` atualizado do backend via `/api/info` no `loadData()`

## MQTT integração com HomeAssistant
- seguir: https://www.home-assistant.io/integrations/mqtt/#discovery-topic