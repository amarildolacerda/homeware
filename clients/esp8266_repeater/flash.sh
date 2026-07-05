#!/bin/bash
IP="${1:-192.168.1.101}"

if command -v pio &> /dev/null; then
    pio run -t upload --upload-port "$IP"
elif command -v platformio &> /dev/null; then
    platformio run -t upload --upload-port "$IP"
elif command -v espota.py &> /dev/null; then
    pio run
    espota.py -i "$IP" -p 8266 -f .pio/build/esp8266/firmware.bin
else
    echo "Error: PlatformIO CLI not found"
    exit 1
fi
