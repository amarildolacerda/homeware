#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "==> Checking Python 3.10+ ..."
PYTHON="${PYTHON:-python3}"
"$PYTHON" -c "import sys; sys.exit(0 if sys.version_info >= (3,10) else 1)" \
    || { echo "Python 3.10+ required"; exit 1; }

echo "==> Detecting fresh install ..."
FRESH_INSTALL=0
if [ ! -d .venv ] || [ ! -d .git/modules ] || [ -z "$(ls -A shared 2>/dev/null)" ]; then
    FRESH_INSTALL=1
fi

if [ "$FRESH_INSTALL" -eq 1 ]; then
    echo "    Fresh install detected — initializing submodules and tooling."
fi

echo "==> Updating git submodules (shared, bridge, etc) ..."
git submodule update --init --recursive

if [ "$FRESH_INSTALL" -eq 1 ]; then
    echo "==> Installing build essentials for a fresh environment ..."
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update || true
        sudo apt-get install -y python3-venv python3-pip libusb-1.0-0 \
            || echo "    (apt install skipped/failed — continue)"
    fi
fi

echo "==> Installing PlatformIO (pip user install) ..."
if ! command -v pio >/dev/null 2>&1 || [ "$FRESH_INSTALL" -eq 1 ]; then
    "$PYTHON" -m pip install --user -U platformio \
        || pip install -U platformio
fi

echo "==> Ensuring 'pio' is on PATH ..."
if ! command -v pio >/dev/null 2>&1; then
    PY_USER_BIN="$("$PYTHON" -m site --user-base)/bin"
    export PATH="$PY_USER_BIN:$PATH"
    echo "    Add to your shell profile: export PATH=\"$PY_USER_BIN:\$PATH\""
fi
command -v pio >/dev/null 2>&1 || { echo "pio not found on PATH"; exit 1; }

echo "==> PlatformIO version:"
pio --version

echo "==> Installing Python dev venv (tests/tools) ..."
if [ -f setup_venv.sh ]; then
    ./setup_venv.sh
fi

echo ""
echo "Environment ready."
echo "  Build:    pio run -d gateway"
echo "  Flash:    pio run -d gateway -t upload"
echo "  Monitor:  pio device monitor -d gateway"
