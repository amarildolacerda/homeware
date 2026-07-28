#!/bin/bash
set -e
command -v platformio >/dev/null 2>&1 || { echo "PlatformIO (pio) not found"; exit 1; }
platformio run
