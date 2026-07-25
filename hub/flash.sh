#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="/dev/ttyUSB0"

while getopts ":p:o:" opt; do
    case $opt in
        p) PORT="$OPTARG" ;;
        o) OTA_IP="$OPTARG" ;;
        *) echo "Uso: $0 [-p <porta>] [-o <ip_ota>]"; exit 1 ;;
    esac
done

if [ -n "$OTA_IP" ]; then
    pio run -e hub_8266_ota --target upload --upload-port "$OTA_IP"
else
    pio run -e hub_8266 --target upload --upload-port "$PORT"
fi
