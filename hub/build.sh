#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../pio_env.sh"

"$PIO" run -e hub_8266
echo "Build complete. Firmware in .pio/build/hub_8266/firmware.bin"
