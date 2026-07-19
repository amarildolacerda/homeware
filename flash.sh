#!/bin/bash
# flash.sh - source + build + flash (default port /dev/ttyUSB0)
# Uso: ./flash.sh [-p <porta>] [-d <dir_projeto>]
set -e

cd "$(dirname "$0")"

PORT="/dev/ttyUSB0"
PROJECT="gateway"

while getopts ":p:d:" opt; do
    case $opt in
        p) PORT="$OPTARG" ;;
        d) PROJECT="$OPTARG" ;;
        *) echo "Uso: $0 [-p <porta>] [-d <dir_projeto>]"; exit 1 ;;
    esac
done

# Garante que o ambiente (submodules + venv + pio) esteja pronto
if [ ! -d .venv ] || [ -z "$(ls -A shared 2>/dev/null)" ]; then
    echo "==> Ambiente incompleto, rodando setup.sh ..."
    ./setup.sh
fi

# Ativa o venv (onde o platformio/pio está instalado)
if [ -f .venv/bin/activate ]; then
    source .venv/bin/activate
fi

echo "==> Building + flashing $PROJECT on $PORT ..."
pio run -d "$PROJECT" -t upload --upload-port "$PORT"

echo "==> Done. Use ./monitor.sh -p $PORT para acompanhar a saída."
