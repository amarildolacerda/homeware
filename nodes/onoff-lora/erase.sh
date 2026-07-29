#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-/dev/ttyUSB0}"
ENV="${2:-lora_esp32}"
cd "$SCRIPT_DIR"
platformio run -e "$ENV" -t erase --upload-port "$PORT"
