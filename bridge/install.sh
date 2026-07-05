#!/bin/bash
set -e

REPO_URL="${REPO_URL:-https://github.com/amarildolacerda/homeware.git}"
REPO_BRANCH="${REPO_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-/addons/bridge_python}"
HTTP_PORT="${HTTP_PORT:-80}"
MQTT_HOST="${MQTT_HOST:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"
LOG_LEVEL="${LOG_LEVEL:-info}"

usage() {
    echo "Uso: curl -fsSL <url> | [VAR=valor] bash"
    echo ""
    echo "Variaveis de ambiente:"
    echo "  REPO_URL       URL do git (default: $REPO_URL)"
    echo "  REPO_BRANCH    Branch (default: $REPO_BRANCH)"
    echo "  INSTALL_DIR    Diretorio de instalacao (default: $INSTALL_DIR)"
    echo "  HTTP_PORT      Porta HTTP (default: $HTTP_PORT)"
    echo "  MQTT_HOST      Host MQTT (default: $MQTT_HOST)"
    echo "  MQTT_PORT      Porta MQTT (default: $MQTT_PORT)"
    echo "  MQTT_USER      Usuario MQTT"
    echo "  MQTT_PASS      Senha MQTT"
    echo ""
    echo "Exemplo:"
    echo "  curl -fsSL https://raw.githubusercontent.com/amarildolacerda/homeware/dev/bridge/install.sh | bash"
    echo "  curl -fsSL https://raw.githubusercontent.com/amarildolacerda/homeware/dev/bridge/install.sh | MQTT_HOST=core-mosquitto MQTT_USER=kzuca MQTT_PASS=123 HTTP_PORT=80 bash"
    exit 1
}

for arg in "$@"; do
    case "$arg" in
        --help|-h) usage ;;
    esac
done

echo "========================================"
echo " Instalador bridge_python"
echo "========================================"
echo " Repo:   $REPO_URL ($REPO_BRANCH)"
echo " Dest:   $INSTALL_DIR"
echo " HTTP:   port $HTTP_PORT"
echo " MQTT:   $MQTT_HOST:$MQTT_PORT"
echo "========================================"
echo ""

echo "[1/4] Clonando repositorio..."
TMP_DIR=$(mktemp -d)
git clone --depth 1 --branch "$REPO_BRANCH" "$REPO_URL" "$TMP_DIR" 2>&1 | tail -2

echo "[2/4] Instalando arquivos..."
mkdir -p "$INSTALL_DIR"
rsync -av --delete \
    --exclude='venv/' \
    --exclude='__pycache__/' \
    --exclude='.pytest_cache/' \
    --exclude='tests/' \
    --exclude='*.pyc' \
    --exclude='.git/' \
    "$TMP_DIR/bridge/" "$INSTALL_DIR/" 2>&1 | tail -3

rm -rf "$TMP_DIR"

echo "[3/4] Configurando ambiente virtual..."
cd "$INSTALL_DIR"
python3 -m venv venv --upgrade-deps
venv/bin/pip install -q --upgrade pip 2>&1 | tail -1
venv/bin/pip install -q -r requirements.txt 2>&1 | tail -3

echo "[4/4] Iniciando bridge..."
if pgrep -f "app.main" > /dev/null 2>&1; then
    echo "  Bridge ja esta rodando, reiniciando..."
    pkill -f "app.main" 2>/dev/null || true
    sleep 2
fi

nohup env MQTT_HOST="$MQTT_HOST" MQTT_PORT="$MQTT_PORT" MQTT_USER="$MQTT_USER" MQTT_PASS="$MQTT_PASS" HTTP_PORT="$HTTP_PORT" LOG_LEVEL="$LOG_LEVEL" \
    "$INSTALL_DIR/venv/bin/python" -m app.main \
    > "$INSTALL_DIR/bridge.log" 2>&1 &

sleep 3

if pgrep -f "app.main" > /dev/null 2>&1; then
    echo ""
    echo "========================================"
    echo " Bridge instalado e rodando!"
    echo "========================================"
    echo " URL:    http://$(ip route get 1 2>/dev/null | awk '{print $7}'):$HTTP_PORT"
    echo " Log:    $INSTALL_DIR/bridge.log"
    echo " Parar:  pkill -f 'app.main'"
    echo " Iniciar: nohup $INSTALL_DIR/venv/bin/python -m app.main &"
    echo ""
    echo " Para ver os logs:"
    echo "   tail -f $INSTALL_DIR/bridge.log"
else
    echo "ERRO: bridge nao iniciou. Verifique o log:"
    echo "  tail -50 $INSTALL_DIR/bridge.log"
    exit 1
fi
