#!/bin/bash
# Resolve o executável do PlatformIO sem depender do PATH de quem chamou.
# Uso em qualquer script do repo:
#   source "$(dirname "$0")/../pio_env.sh"   (ou ./pio_env.sh na raiz)
#   "$PIO" run ...

_PIO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v pio >/dev/null 2>&1; then
    PIO="$(command -v pio)"
elif command -v platformio >/dev/null 2>&1; then
    PIO="$(command -v platformio)"
elif [ -x "$HOME/.platformio/penv/bin/pio" ]; then
    PIO="$HOME/.platformio/penv/bin/pio"
elif [ -x "$_PIO_ROOT/.venv/bin/pio" ]; then
    PIO="$_PIO_ROOT/.venv/bin/pio"
else
    echo "PlatformIO não encontrado. Rode ./setup.sh ou:" >&2
    echo "  export PATH=\"$HOME/.platformio/penv/bin:\$PATH\"" >&2
    return 1 2>/dev/null || exit 1
fi

export PATH="$(dirname "$PIO"):$PATH"
unset _PIO_ROOT
