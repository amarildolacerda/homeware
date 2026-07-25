#!/bin/bash
set -e
cd "$(dirname "$0")"

PORT="/dev/ttyUSB0"
MODE="serial"
ENV="esp8266"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) PORT="$2"; MODE="serial"; shift 2 ;;
        -o|--ota)  PORT="$2"; MODE="ota"; shift 2 ;;
        -e|--env)  ENV="$2"; shift 2 ;;
        -h|--help)
            echo "Uso: $0 [-p <porta>] [-o <ip>] [-e <env>]"
            echo "  -p, --port   Porta serial (default: /dev/ttyUSB0)"
            echo "  -o, --ota    IP para upload OTA"
            echo "  -e, --env    Environment (default: esp8266, opcoes: esp8266, esp8266_esp01s)"
            exit 0 ;;
        *) echo "Opcao desconhecida: $1"; exit 1 ;;
    esac
done

if [[ "$MODE" == "ota" ]]; then
    OTA_ENV="${ENV}_ota"
    echo "Building and uploading OTA to ${PORT} (env: ${OTA_ENV})..."
    pio run -e "$OTA_ENV" --target upload --upload-port "$PORT"
else
    echo "Building and flashing on ${PORT} (env: ${ENV})..."
    pio run -e "$ENV" --target upload --upload-port "$PORT"
fi
echo "Done."
