# PIR Battery ESP-NOW Client

## Visão Geral
Sensor PIR com deep sleep para economia de energia. Acorda periodicamente, lê o estado do PIR (GPIO4), envia dados via ESP-NOW broadcast, e dorme.

## Características
- Deep sleep entre ciclos (intervalo configurável 1-3600s, default 60s)
- Telnet na porta 23 suspende o deep sleep; desconectar telnet faz restart
- Comando `p` no menu serial/telnet para re-parear
- Comando RESTART via ESP-NOW (gateway)
- Configuração WiFiManager (portal se botão pressionado ou sem gateway salvo)
- Nome do dispositivo e intervalo configuráveis via WiFiManager

## Hardware
| Componente | GPIO |
|---|---|
| PIR sensor | GPIO4 (D2) |
| Botão config | GPIO0 (D3, pull-up, LOW = pressionado) |
| LED | GPIO2 (D4, active LOW) |

## Comandos (Serial/Telnet)
| Tecla | Ação |
|---|---|
| `s` | Status (PIR, WiFi, intervalo) |
| `i` | Alterar intervalo de sleep |
| `r` | Restart |
| `p` | Re-parear com gateway |
| `h` | Ajuda |

## Deep sleep
- Se telnet conectar → suspende sleep
- Se telnet desconectar → restart (volta ciclo normal)
- Falha de leitura/ACK → retry com `DEEP_SLEEP_RETRY_INTERVAL` (10s)

## ESP-NOW
- Broadcast (`FF:FF:FF:FF:FF:FF`)
- PAIR_REQUEST com `SENSOR_TYPE_MOTION`
- SENSOR_DATA com `payload_motion_t` + 4 bytes IP
- Aguarda ACK do gateway (timeout 10s, 2 retries)