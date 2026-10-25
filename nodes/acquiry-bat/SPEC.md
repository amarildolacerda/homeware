# Spec: Acquiry-BAT

## Visão geral
O nó Acquiry-BAT é um sensor de aquisição de estado de baixa potência, alimentado por bateria e opcionalmente painel solar, que monitora um pino de entrada digital (`ACQUERY_PIN`) e publica o estado por ESP-NOW para o hub central. O dispositivo opera em ciclo de despertar, leitura, envio e deep sleep. O objetivo é detectar um evento de aquisição (ligado/desligado) com o menor consumo possível e manter o hub sincronizado sem depender de WiFi contínua.

O projeto é arquitetado para reutilizar o protocolo compartilhado em `shared/`, o que também torna o nó compatível com o hub e com a descoberta MQTT/Home Assistant já existentes.

## Requisitos de hardware
- MCU: ESP8266/ESP32, preferencialmente com boa eficiência de energia e suporte a deep sleep
- Entrada de sensor: pino digital associado ao sinal de aquisição (`ACQUERY_PIN`)
- Alimentação: bateria + painel solar + carregador (ex.: 18650 + TP4056)
- Padrão de wakeup: o pino do sensor aciona o contexto de leitura; após envio, o node entra em deep sleep por intervalos configuráveis
- LED: usado como feedback de transmissão/estado e não deve bloquear o loop

## Fluxo de operação
```
[deep sleep] -> [despertar por ciclo ou por sinal] -> [leitura do pino] -> [par de gateway/ESP-NOW] -> [envio de payload] -> [ACK/timeout] -> [sleep]
```

### Semântica da leitura
- O estado do sensor é representado como `0` ou `1`
- A leitura deve permanecer estável por um tempo mínimo (`ACQUIRY_HOLD_TIME_MS`)
- Se o pino permanecer em nível alto por mais do que o hold time, o estado final é tratado como ativo
- O dispositivo não é um sensor de presença completo: ele sinaliza estado ligado/desligado do ponto monitorado

## Protocolo e integração com hub
- O node usa `shared/` e `espnow_protocol.h` para pares, mensagens e pacotes ESP-NOW
- Mensagens principais:
  - `MSG_PAIR_REQUEST`
  - `MSG_PAIR_RESPONSE`
  - `MSG_SENSOR_DATA`
  - `MSG_ACK`
  - `MSG_RESTART`
- O hub identifica o dispositivo por MAC de rádio + slot atribuído
- O node deve reaproveitar a regra de `device_name[32]` e o armazenamento de nome no EEPROM/shared
- O payload do sensor deve ser enviado com o tipo correto de sensor (`SENSOR_TYPE_ACQUIRY`) e manter `battery_pct` e `rssi` no cabeçalho quando aplicável

## Payload esperável
O payload do nó é compatível com o esquema de sensor compartilhado e deve transportar:
- estado da aquisição (on/off)
- tempo/hold de aquisição
- leitura de bateria (quando disponível)
- identificador do node e nome configurável

Exemplo de intenção lógica:
```json
{
  "sensor_type": "acquiry",
  "state": 1,
  "battery_pct": 82,
  "device_name": "sensor_01",
  "chip_id": "123456"
}
```

## Regras de firmware
- Device ID dinâmico em formato `agri_<chip_id>`; nunca hardcoded
- Nome do dispositivo configurável via WiFiManager, salvo em EEPROM e validado
- `FW_VERSION` deve ser a mesma da tag no hub, no server Python e nos nodes
- `DEVICE_NAME` e `device_name[32]` devem respeitar buffer de 32 bytes
- `loop()` deve ser não-bloqueante; não usar `delay()` para lógica crítica de rotina
- `ESP-NOW` deve operar no canal do WiFi do hub; no caso do roteador/AP, o canal deve ser compatível
- Para ESP8266, usar `WiFi.setSleepMode(WIFI_NONE_SLEEP)` quando o módulo precisar manter operação ativa em comunicação
- Em deep sleep, salvar o estado de configuração e sair de forma previsível; nunca deixar o node preso em loop de retry infinito

## WiFi e configuração
- O nó usa WiFiManager para conectar e para portal de configuração quando necessário
- O portal deve permitir alterar nome do dispositivo e o intervalo de sleep
- O botão físico de configuração deve forçar o portal se pressionado ou se não houver gateway pareado
- A conexão WiFi deve ser tratada como etapa de setup, não como rotina permanente de operação
- Nenhum `autoConnect()` bloqueante é permitido; a rotina deve respeitar timeout e retornar ao comportamento seguro de sleep

## Gateway e pareamento
- O nó deve tentar parear por broadcast em `espnow_pair_request_t`
- Após despertar e inicializar ESP-NOW, aguardar 2 segundos para estabilização do rádio antes do primeiro pareamento/envio
- Se o gateway responder com `MSG_PAIR_RESPONSE`, o node grava o MAC e o slot e passa a enviar dados
- O node deve reconectar/reativar pareamento se o gateway cair ou se a resposta não chegar
- O par não deve depender de comunicação HTTP direta; o fluxo é por ESP-NOW exclusivamente

## Energia e consumo
- O consumo de deep sleep deve ser priorizado em relação à funcionalidade de rede
- O ciclo de leitura deve ser curto e determinístico
- O estado da bateria deve ser lido apenas quando necessário; não deve causar latência ou jitter relevantes
- Se houver leitura inválida ou falha de envio, o node deve entrar em sleep com política de retry, sem travar o ciclo

## Comandos e feedback
- O gateway pode enviar comandos de restart ou reconexão para o node
- Qualquer mudança de estado relevante deve disparar feedback imediato ao hub
- O ACK deve ser tratado como confirmação de entrega; se não houver confirmação, o nó pode reenviar até o limite configurado

## Dashboard e console
- O node pode expor um console serial/telnet com comandos mínimos, como:
  - `s` status
  - `i` intervalo
  - `r` restart
  - `p` re-parear
  - `h` ajuda
- O console deve suportar telnet e serial sem bloquear a rotina principal
- Quando o telnet desconecta, o device deve seguir a regra específica da plataforma e reiniciar quando necessário

## Checklist de implementação
- [ ] criar `config.h` com defines locais do node
- [ ] respeitar `platform.h` para abstrações de plataforma
- [ ] usar `lib_extra_dirs = ../../shared` e `build_flags` apropriados
- [ ] usar `DEVICE_NAME`, `FW_VERSION`, `EEPROM_*`, `DEEP_SLEEP_INTERVAL_*` conforme o shared
- [ ] implementar pareamento ESP-NOW e envio de dados com payload do protocolo compartilhado
- [ ] configurar entrada do sensor e feedback visual (LED)
- [ ] registrar nome do dispositivo e intervalo em EEPROM com validação
- [ ] testar re-pareamento, ACK e deep sleep
- [ ] validar firmware com tag e sincronização com hub/server

## Regras herdadas do projeto
- Device ID dinâmico, nunca fixo (`agri_<chip_id>`)
- Nome do dispositivo configurável via WiFiManager
- Reusar código do `shared/` em vez de duplicar implementações
- Evitar globais desnecessárias e manter código por responsabilidade
- Respeitar `shared_config.h` e `platform.h` para cross-platform
- Priorizar operação estável, com retries suaves e sem `delay()` crítico
- Qualquer nova regra e correção de comportamento deve ser documentada para reaproveitamento posterior

## Próximos passos
- Ajustar o `main.cpp` para o ciclo real do nó Acquiry-BAT
- Validar `ACQUERY_PIN`, `CFG_BUTTON_PIN`, `LED_PIN` e `EEPROM` no hardware final
- Garantir compatibilidade com os padrões do hub atual e com a tag de firmware do projeto
- Expandir testes em bancada para pareamento, deep sleep e reinicialização por watchdog/telnet
