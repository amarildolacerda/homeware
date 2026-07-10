#!/bin/bash
# Serial monitor

if command -v pio &> /dev/null; then
    pio device monitor --baud 115200
elif command -v platformio &> /dev/null; then
    platformio device monitor --baud 115200
else
    echo "Error: PlatformIO CLI not found"
    echo "Install: pip install platformio"
    exit 1
fi
