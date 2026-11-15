# Especificação Técnica - Firmware IoT Dual Power Mode (ESP8266 / ESP32-S3)

## 1. Objetivo

Desenvolver um firmware  capaz de adaptar automaticamente seu comportamento conforme a fonte de alimentação disponível, operando em dois modos distintos:

1. Battery Mode (baixo consumo)
2. Powered Mode (alimentação externa USB/fonte)

O objetivo é maximizar a autonomia da bateria sem comprometer funcionalidades avançadas quando houver energia disponível.

---

# 2. Requisitos Funcionais

## RF-001 Detecção de Alimentação

O dispositivo deve detectar durante o boot a origem da alimentação.

### Critérios

- Alimentação por bateria → ativar Battery Mode
- Alimentação externa (USB ou fonte) → ativar Powered Mode

### Métodos possíveis

#### Opção A (preferencial)
Monitoramento da presença da linha USB VBUS.

#### Opção B
Medição da tensão de entrada através de ADC.

---

# 3. Battery Mode

## Objetivo
Minimizar consumo energético.

### Comportamento
1. Inicializar hardware mínimo
2. Ler sensores
3. Montar payload
4. Transmitir via ESP-NOW
5. Receber ACK opcional
6. Entrar em Deep Sleep

### Comunicação
Protocolo: ESP-NOW

### Funcionalidades Habilitadas
- Sensores
- Telemetria
- ESP-NOW
- Deep Sleep

### Funcionalidades Desabilitadas
- Web Server
- OTA
- Bluetooth
- MQTT contínuo
- Wi‑Fi conectado

---

# 4. Powered Mode

## Objetivo
Disponibilizar todos os recursos do dispositivo.

### Recursos
- Wi‑Fi conectado continuamente
- WebServer propria para o Dashboard Web, uma sidebar com as opções, mostrando o estado de leitura, permitir configurações... (aka projeto lamp)
- OTA
- Logs detalhados
- Configurações remotas

---

# 5. Gateway ESP-NOW

Responsável por receber pacotes dos nós alimentados por bateria e encaminhar para MQTT, Home Assistant, Node-RED ou outros serviços.

---

# 6. Estrutura de Software

```cpp
enum PowerMode {
    BATTERY_MODE,
    POWERED_MODE
};
```

```cpp
void setup() {
    detectPowerSource();

    switch(mode) {
        case BATTERY_MODE:
            startBatteryMode();
            break;

        case POWERED_MODE:
            startPoweredMode();
            break;
    }
}
```


# 7. Requisitos Energéticos

## Battery Mode
- Deep Sleep < 20 µA
- Tempo acordado < 240 segundos
- Autonomia de semanas a meses com LiPo 1000 mAh

## Powered Mode
- Funcionalidade máxima
- Consumo não é prioritário

---

# 8. Hardware Recomendado

## MCU
- ESP32-S3 Mini
- ESP32-S3 DevKit
- XIAO ESP32S3

## Bateria
- LiPo 3.7V 1000 mAh (recomendado)
- 18650 (alta autonomia)

## Carregador
- TP4056
- MCP73831

---
