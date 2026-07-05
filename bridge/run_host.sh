#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

VENV_DIR="${VENV_DIR:-$HOME/.venvs/bridge_python}"

source "$DIR/config.sh"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--port)
            HTTP_PORT="$2"; shift 2
            export HTTP_PORT ;;
        -i|--ip)
            BRIDGE_IP="$2"; shift 2
            export BRIDGE_IP ;;
        *)
            echo "Usage: $0 [-p|--port <port>] [-i|--ip <ip>]"
            exit 1 ;;
    esac
done

echo "=== bridge_python (host) ==="

if [ ! -d "$VENV_DIR" ]; then
    echo "[1/3] Criando venv..."
    python3 -m venv "$VENV_DIR" --upgrade-deps
fi

echo "[2/3] Verificando dependencias..."
"$VENV_DIR/bin/python" -m pip install -q --upgrade pip
"$VENV_DIR/bin/pip" install -q --no-binary zeroconf -r requirements.txt

echo "[3/3] Iniciando bridge em http://0.0.0.0:$HTTP_PORT..."
exec "$VENV_DIR/bin/python" -m app.main
