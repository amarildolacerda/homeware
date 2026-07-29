#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="/dev/ttyUSB0"
ENV="hub_32_lora"

while getopts ":p:o:e:" opt; do
    case $opt in
        p) PORT="$OPTARG" ;;
        o) OTA_IP="$OPTARG" ;;
        e) ENV="$OPTARG" ;;
        *) echo "Uso: $0 [-p <porta>] [-o <ip_ota>]"; exit 1 ;;
    esac
done

if [ -n "$OTA_IP" ]; then
    platformio run -e "$ENV" --target upload --upload-port "$OTA_IP"
else
    platformio run -e "$ENV" --target upload --upload-port "$PORT"
fi
