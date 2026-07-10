#!/bin/bash
# Build firmware

if command -v pio &> /dev/null; then
    pio run
elif command -v platformio &> /dev/null; then
    platformio run
else
    echo "Error: PlatformIO CLI not found"
    echo "Install: pip install platformio"
    exit 1
fi
