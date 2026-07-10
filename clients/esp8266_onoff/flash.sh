#!/bin/bash
set -e
PIOPATH="$HOME/.platformio/penv/bin"
if [ -f "$PIOPATH/pio" ]; then
    export PATH="$PIOPATH:$PATH"
fi

PORT="/dev/ttyUSB0"
MODE="serial"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port) PORT="$2"; MODE="serial"; shift 2 ;;
        -o|--ota)  PORT="$2"; MODE="ota"; shift 2 ;;
        -h|--help)
            echo "Uso: $0 [-p <porta>] [-o <ip>]"
            echo "  -p, --port   Porta serial (default: /dev/ttyUSB0)"
            echo "  -o, --ota    IP para upload OTA"
            exit 0 ;;
        *) echo "Opcao desconhecida: $1"; exit 1 ;;
    esac
done

command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }

if [[ "$MODE" == "ota" ]]; then
    echo "Building and uploading OTA to ${PORT}..."
    pio run -e esp8266_ota --target upload --upload-port "$PORT"
else
    echo "Building and flashing on ${PORT}..."
    pio run --target upload --upload-port "$PORT"
fi
echo "Done."
