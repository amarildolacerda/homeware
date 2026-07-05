#!/bin/bash
# config.sh — bridge environment defaults
# Source this from run_host.sh or other scripts:
#   source "$(dirname "$0")/config.sh"

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- defaults (match docker-compose.yml + config.yaml) ---
HTTP_PORT="${HTTP_PORT:-80}"
MQTT_HOST="${MQTT_HOST:-localhost}"
MQTT_PORT="${MQTT_PORT:-1883}"
MQTT_USER="${MQTT_USER:-}"
MQTT_PASS="${MQTT_PASS:-}"
LOG_LEVEL="${LOG_LEVEL:-info}"
BRIDGE_IP="${BRIDGE_IP:-}"

export HTTP_PORT MQTT_HOST MQTT_PORT MQTT_USER MQTT_PASS LOG_LEVEL BRIDGE_IP

echo "  Config: HTTP_PORT=$HTTP_PORT MQTT=$MQTT_HOST:$MQTT_PORT LOG=$LOG_LEVEL BRIDGE_IP=${BRIDGE_IP:-"(auto)"}"
