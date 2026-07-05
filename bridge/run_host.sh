#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

VENV_DIR="${VENV_DIR:-venv}"
HTTP_PORT="${HTTP_PORT:-80}"
MQTT_HOST="${MQTT_HOST:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"
LOG_LEVEL="${LOG_LEVEL:-info}"
BRIDGE_IP="${BRIDGE_IP:-}"

export HTTP_PORT MQTT_HOST MQTT_PORT MQTT_USER MQTT_PASS LOG_LEVEL BRIDGE_IP

echo "=== bridge_python (host) ==="

if [ ! -d "$VENV_DIR" ]; then
    echo "[1/3] Criando venv..."
    python3 -m venv "$VENV_DIR" --upgrade-deps
fi

echo "[2/3] Verificando dependencias..."
"$VENV_DIR/bin/python" -m pip install -q --upgrade pip
"$VENV_DIR/bin/pip" install -q -r requirements.txt

echo "[3/3] Iniciando bridge..."
exec "$VENV_DIR/bin/python" -m app.main
