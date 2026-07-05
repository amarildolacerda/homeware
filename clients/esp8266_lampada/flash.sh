#!/bin/bash
# Flash via OTA
# Usage: ./flash.sh [IP_ADDRESS]

IP="${1:-192.168.1.100}"

if command -v pio &> /dev/null; then
    pio run -t upload --upload-port "$IP"
elif command -v platformio &> /dev/null; then
    platformio run -t upload --upload-port "$IP"
elif command -v espota.py &> /dev/null; then
    echo "Using espota.py directly..."
    pio run
    espota.py -i "$IP" -p 8266 -f .pio/build/esp8266/firmware.bin
else
    echo "Error: PlatformIO CLI not found"
    echo "Install: pip install platformio"
    exit 1
fi
