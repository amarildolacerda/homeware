#!/usr/bin/env bash
# Reinicia um dispositivo ESP-NOW via HTTP /api/restart.
# Uso: ./restart.sh <ip> [porta]
# Sem IP: usa scan.py para descobrir e reinicia o primeiro encontrado.
set -e
cd "$(dirname "$0")"

IP="$1"
PORT="${2:-80}"

if [ -z "$IP" ]; then
    echo "Nenhum IP informado. Usando scan.py para descobrir dispositivos..."
    if [ -f scan.py ]; then
        python3 scan.py
    fi
    echo "Informe o IP: ./restart.sh <ip>"
    exit 1
fi

echo "Reiniciando $IP:$PORT ..."
python3 device_cli.py "$IP" restart -p "$PORT"
