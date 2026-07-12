#!/bin/bash
set -e

PIOPATH="$HOME/.platformio/penv/bin"
if [ -f "$PIOPATH/pio" ]; then
    export PATH="$PIOPATH:$PATH"
fi

IP="${1:-192.168.4.1}"
PORT="${2:-8266}"

command -v pio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }

echo "Building..."
pio run -e esp8266_ota

echo "Sending OTA to ${IP}:${PORT}..."
pio run -e esp8266_ota --target upload --upload-port "${IP}"
