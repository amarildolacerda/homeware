#!/bin/bash
# nodes/onoff-lora-avr/monitor.sh
set -e
cd "$(dirname "$0")"
PORT="${1:-/dev/ttyUSB0}"
pio device monitor --port "$PORT"
