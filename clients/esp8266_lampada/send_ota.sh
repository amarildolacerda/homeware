#!/bin/bash
set -e

PIOPATH="$HOME/.platformio/penv/bin"
if [ -f "$PIOPATH/pio" ]; then
    export PATH="$PIOPATH:$PATH"
fi

IP="${1:-192.168.4.1}"
PORT="${2:-80}"
FW_BIN=".pio/build/esp8266_ota/firmware.bin"

command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }

echo "Building..."
pio run -e esp8266_ota

if [ ! -f "$FW_BIN" ]; then
    echo "Firmware nao encontrado: $FW_BIN"
    exit 1
fi

echo "Sending OTA via /api/ota to ${IP}:${PORT}..."
curl -f -F "firmware=@${FW_BIN}" "http://${IP}:${PORT}/api/ota" \
    && echo "OTA enviado. Aguarde o reinicio do device." \
    || { echo "Falha no envio OTA"; exit 1; }
