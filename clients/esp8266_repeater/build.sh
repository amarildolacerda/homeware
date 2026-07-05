#!/bin/bash
if command -v pio &> /dev/null; then
    pio run
elif command -v platformio &> /dev/null; then
    platformio run
else
    echo "Error: PlatformIO CLI not found"
    exit 1
fi
