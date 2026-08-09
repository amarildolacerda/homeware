#!/bin/bash
# nodes/onoff-lora-avr/flash.sh
set -e
cd "$(dirname "$0")"
PORT="${1:-/dev/ttyUSB0}"
pio run --target upload --upload-port "$PORT"
