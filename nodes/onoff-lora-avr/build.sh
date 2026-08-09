#!/bin/bash
# nodes/onoff-lora-avr/build.sh
set -e
cd "$(dirname "$0")"
pio run
