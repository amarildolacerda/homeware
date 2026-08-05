# Alexa Device Test Script - Design Spec

**Data:** 2026-08-04
**Status:** Aprovado

## Objetivo

Criar script Python para testar a integração Alexa no node lamp, validando:
- Descoberta via SSDP/UPnP
- Controle on/off via API
- Estado feedback em tempo real
- Log de resultados

## Localização

`nodes/lamp/test_alexa.py`

## Funcionalidades

### 1. Descoberta SSDP
- Envia M-SEARCH multicast (239.255.255.250:1900)
- Timeout configurável (padrão 3s)
- Coleta respostas HTTP dos nodes
- Parse LOCATION header para obter IP/porta

### 2. Teste de API
- `GET /api/lights` → lista devices Alexa
- `GET /api/<user>/lights/<id>` → estado atual
- `PUT /api/<user>/lights/<id>` → controle on/off

### 3. Controle
- Ligar: PUT com `{"state":"ON"}`
- Desligar: PUT com `{"state":"OFF"}`
- Verificar mudança de estado após comando

### 4. Monitoramento
- Polling periódico de estado (intervalo configurável)
- Delta detection: detecta mudanças de estado
- Timeout de monitoramento (padrão 60s)

### 5. Log
- Salva resultados em `test_results.json`
- Timestamp de cada operação
- Status pass/fail
- Erros e mensagens

## Modos de Execução

### Modo Interativo
```bash
python test_alexa.py
```
Menu de opções:
1. Descobrir devices
2. Testar controle
3. Monitorar estado
4. Rodar todos os testes
5. Sair

### Modo Automático
```bash
python test_alexa.py --ip <IP> --discover --control --monitor --duration <segundos>
```

**Argumentos:**
- `--ip IP` - IP do node (opcional, descobre automaticamente se omitido)
- `--discover` - rodar teste de descoberta
- `--control` - rodar teste de controle
- `--monitor` - rodar monitoramento
- `--duration SEGUNDOS` - duração do monitoramento (padrão 60)
- `--interval SEGUNDOS` - intervalo de polling (padrão 2)
- `--output ARQUIVO` - arquivo de log (padrão test_results.json)

## Fluxo do Teste

### Descoberta
1. Envia M-SEARCH multicast
2. Aguarda respostas por `timeout` segundos
3. Para cada resposta:
   - Extrai IP/PORT do LOCATION
   - Faz GET /api/lights
   - Valida resposta JSON
4. Retorna lista de devices encontrados

### Controle
1. Para cada device descoberto:
   - GET estado atual
   - PUT liga (ON)
   - Aguarda 1s
   - GET estado → espera ON
   - PUT desliga (OFF)
   - Aguarda 1s
   - GET estado → espera OFF
2. Registra pass/fail para cada operação

### Monitoramento
1. Inicia polling de estado
2. A cada `interval` segundos:
   - GET estado atual
   - Compara com anterior
   - Registra delta se houver mudança
3. Para após `duration` segundos
4. Retorna histórico de mudanças

## Requisitos

- Python 3.10+
- Bibliotecas: `requests`, `socket`, `json`, `time`, `datetime`
- Network: acesso multicast (239.255.255.250:1900)

## Formato do Log

```json
{
  "timestamp": "2026-08-04T12:00:00",
  "node_ip": "192.168.1.100",
  "tests": [
    {
      "name": "discovery",
      "status": "pass",
      "duration_ms": 3200,
      "devices_found": 1,
      "details": {...}
    },
    {
      "name": "control_on",
      "status": "pass",
      "duration_ms": 150,
      "details": {...}
    }
  ],
  "summary": {
    "total": 5,
    "passed": 4,
    "failed": 1
  }
}
```
