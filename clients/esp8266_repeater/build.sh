#!/bin/bash
set -e
cd "$(dirname "$0")"

ENV="${1:-esp8266}"

echo "Building env: ${ENV}..."
pio run -e "$ENV"
echo "Done."
