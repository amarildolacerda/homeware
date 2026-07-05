#!/bin/bash
# install_pi.sh — Instala bridge_python no Raspberry Pi com HA existente
set -e

HA_IP=""
MQTT_HOST=""
MQTT_PORT="1883"
BRIDGE_USER="${USER}"
INSTALL_DIR="${HOME}/homeware/bridge"

usage() {
    echo "Uso: $0 [opcoes]"
    echo ""
    echo "Opcoes:"
    echo "  --ha-ip <ip>       IP do Home Assistant (obrigatorio)"
    echo "  --mqtt-host <host>  Host MQTT (padrao: mesmo do --ha-ip)"
    echo "  --mqtt-port <port>  Porta MQTT (padrao: 1883)"
    echo "  --dir <path>       Diretorio de instalacao (padrao: ${INSTALL_DIR})"
    echo "  --user <user>      Usuario para o servico systemd (padrao: ${BRIDGE_USER})"
    echo ""
    echo "Exemplo:"
    echo "  $0 --ha-ip 192.168.1.50"
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --ha-ip) HA_IP="$2"; shift 2 ;;
        --mqtt-host) MQTT_HOST="$2"; shift 2 ;;
        --mqtt-port) MQTT_PORT="$2"; shift 2 ;;
        --dir) INSTALL_DIR="$2"; shift 2 ;;
        --user) BRIDGE_USER="$2"; shift 2 ;;
        *) echo "Opcao desconhecida: $1"; usage ;;
    esac
done

if [ -z "$HA_IP" ]; then
    echo "ERRO: --ha-ip e obrigatorio (ex: --ha-ip 192.168.1.50)"
    exit 1
fi

MQTT_HOST="${MQTT_HOST:-$HA_IP}"
REPO_BRIDGE="$(cd "$(dirname "$0")" && pwd)"

echo "========================================"
echo " Instalador bridge_python"
echo "========================================"
echo " HA IP:        $HA_IP"
echo " MQTT:         $MQTT_HOST:$MQTT_PORT"
echo " Instalar em:  $INSTALL_DIR"
echo " Usuario:      $BRIDGE_USER"
echo "========================================"

echo ""
echo "[1/6] Instalando dependencias do sistema..."
sudo apt-get update -qq
sudo apt-get install -y -qq python3-venv python3-pip

echo "[2/6] Verificando conexao MQTT..."
if command -v nc &>/dev/null; then
    if nc -zv "$MQTT_HOST" "$MQTT_PORT" -w 3 2>/dev/null; then
        echo "  OK: Mosquitto respondendo em $MQTT_HOST:$MQTT_PORT"
    else
        echo "  AVISO: Nao foi possivel conectar em $MQTT_HOST:$MQTT_PORT"
        echo "  Verifique se o add-on Mosquitto broker esta rodando no HA."
        echo "  Continuando mesmo assim..."
    fi
else
    echo "  (nc nao disponivel, pulando verificacao)"
fi

echo "[3/6] Copiando arquivos do bridge..."
mkdir -p "$INSTALL_DIR"
rsync -av --delete \
    --exclude='venv/' \
    --exclude='__pycache__/' \
    --exclude='.pytest_cache/' \
    --exclude='tests/' \
    --exclude='*.pyc' \
    --exclude='.git/' \
    "$REPO_BRIDGE/" "$INSTALL_DIR/"

echo "[4/6] Criando ambiente virtual..."
VENV_DIR="${INSTALL_DIR}/venv"
python3 -m venv "$VENV_DIR" --upgrade-deps
"$VENV_DIR/bin/pip" install -q --upgrade pip
"$VENV_DIR/bin/pip" install -q --no-binary zeroconf -r "$INSTALL_DIR/requirements.txt"

echo "[5/6] Criando servico systemd..."
SERVICE_FILE="/etc/systemd/system/bridge_python.service"
sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=ESP-HA Bridge Python
After=network.target

[Service]
Type=simple
User=${BRIDGE_USER}
WorkingDirectory=${INSTALL_DIR}
Environment=MQTT_HOST=${MQTT_HOST}
Environment=MQTT_PORT=${MQTT_PORT}
Environment=HTTP_PORT=80
Environment=LOG_LEVEL=info
Environment=BRIDGE_IP=${HA_IP}
ExecStart=${VENV_DIR}/bin/python -m app.main
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

echo "[6/6] Ativando e iniciando servico..."
sudo systemctl daemon-reload
sudo systemctl enable bridge_python
sudo systemctl restart bridge_python

echo ""
echo "========================================"
echo " Instalacao concluida!"
echo "========================================"
echo ""
echo " Comandos uteis:"
echo "   sudo systemctl status bridge_python  — status do servico"
echo "   sudo journalctl -u bridge_python -f  — logs em tempo real"
echo "   sudo systemctl stop bridge_python     — parar"
echo "   sudo systemctl start bridge_python    — iniciar"
echo ""
echo " Painel web: http://$(hostname -I | awk '{print $1}'):80"
echo ""
echo " No Home Assistant: Settings > Devices & Services > MQTT"
echo " Os dispositivos aparecerao automaticamente via MQTT Discovery."
echo ""
echo " Se algo der errado, veja os logs:"
echo "   sudo journalctl -u bridge_python -n 50 --no-pager"
echo ""