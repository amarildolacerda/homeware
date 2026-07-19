#!/bin/bash
set -e

cd "$(dirname "$0")"

VENV_DIR=".venv"
PYTHON="${PYTHON:-python3}"

echo "Creating virtualenv in $VENV_DIR ..."
if [ ! -d "$VENV_DIR" ]; then
    "$PYTHON" -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"

echo "Upgrading pip ..."
python -m pip install --upgrade pip

REQ_FILE="requirements.txt"
if [ -f "$REQ_FILE" ]; then
    echo "Installing from $REQ_FILE ..."
    pip install -r "$REQ_FILE"
else
    echo "No requirements.txt found, installing base dev dependencies ..."
    pip install pytest fastapi httpx pyyaml paho-mqtt
fi

echo "venv ready. Activate with: source $VENV_DIR/bin/activate"
