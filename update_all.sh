#!/bin/bash
# update_all.sh - Atualiza dispositivos via OTA — usa scan.py para descobrir, checa versão, atualiza se necessário
# Uso: ./update_all.sh [-f]  (força OTA em todos)

set -euo pipefail

FORCE=0
if [[ "${1:-}" == "-f" ]] || [[ "${1:-}" == "--force" ]]; then
    FORCE=1
fi

ROOT="$(cd "$(dirname "$0")" && pwd)"

# ---- helpers ----

find_pio() {
    for p in pio platformio "$HOME/.local/bin/platformio"; do
        if command -v "$p" &>/dev/null; then echo "$p"; return; fi
    done
    echo "PlatformIO CLI not found" >&2; exit 1
}

get_fw_version() {
    local project_dir="$1"
    local files=(
        "$project_dir/include/config.h"
        "$project_dir/src/config.h"
        "$project_dir/include/pages.h"
        "$project_dir/../shared/src/shared_config.h"
        "$project_dir/../../shared/src/shared_config.h"
        "$ROOT/shared/src/shared_config.h"
    )
    for f in "${files[@]}"; do
        [[ -f "$f" ]] || continue
        local ver
        ver=$(grep -oP '#define\s+FW_VERSION\s+"([^"]+)"' "$f" 2>/dev/null | head -1 | grep -oP '"[^"]+"' | tr -d '"')
        if [[ -n "$ver" ]]; then echo "$ver"; return; fi
    done
    echo ""
}

# Compara versões: retorna 0 se local_ver > device_ver (precisa atualizar)
needs_update() {
    local local_ver="$1" device_ver="${2:-}"
    [[ -z "$device_ver" ]] && return 0

    local re='^v?([0-9]+)\.([0-9]+)\.([0-9]+)'
    local la lb lc da db dc

    if ! [[ "$local_ver" =~ $re ]]; then return 0; fi
    la=${BASH_REMATCH[1]}; lb=${BASH_REMATCH[2]}; lc=${BASH_REMATCH[3]}

    if ! [[ "$device_ver" =~ $re ]]; then return 0; fi
    da=${BASH_REMATCH[1]}; db=${BASH_REMATCH[2]}; dc=${BASH_REMATCH[3]}

    [[ "$la" -gt "$da" ]] && return 0
    [[ "$la" -lt "$da" ]] && return 1
    [[ "$lb" -gt "$db" ]] && return 0
    [[ "$lb" -lt "$db" ]] && return 1
    [[ "$lc" -gt "$dc" ]] && return 0
    return 1
}

# ---- descoberta via scan.py ----

discover_devices() {
    local scan_py="$ROOT/scan.py"
    if [[ ! -f "$scan_py" ]]; then
        echo "scan.py não encontrado em $scan_py" >&2; exit 1
    fi
    echo -e "\e[36mDescobrindo dispositivos via scan.py...\e[0m" >&2
    python3 "$scan_py" --json 2>/dev/null
}

# ---- deploy ----

update_device_type() {
    local project_dir="$1" env_name="$2" label="$3"
    shift 3
    local devices=("$@")

    local local_ver
    local_ver=$(get_fw_version "$project_dir")
    [[ -z "$local_ver" ]] && { echo "  [aviso] FW_VERSION não encontrado em $project_dir"; return; }

    local force_str=""
    [[ "$FORCE" -eq 1 ]] && force_str=" [FORCE]"
    echo -e "\n\e[36m=== $label (local: $local_ver)$force_str ===\e[0m"

    # Filtra dispositivos que precisam de update
    local pending=()
    for dev in "${devices[@]}"; do
        local dev_ver dev_ip
        dev_ver=$(echo "$dev" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('fw_version',''))" 2>/dev/null || echo "")
        dev_ip=$(echo "$dev" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('ip',''))" 2>/dev/null || echo "")

        if [[ "$FORCE" -eq 1 ]]; then
            pending+=("$dev")
        elif needs_update "$local_ver" "$dev_ver"; then
            pending+=("$dev")
        else
            echo -e "  \e[32m[SKIP] $dev_ip: já atualizado ($dev_ver)\e[0m"
        fi
    done

    if [[ ${#pending[@]} -eq 0 ]]; then
        echo "  Todos atualizados."; return
    fi

    echo "  Build..."
    $PIO run -d "$project_dir" -e "$env_name" || { echo "  Build falhou" >&2; return; }

    for dev in "${pending[@]}"; do
        local dev_ip
        dev_ip=$(echo "$dev" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('ip',''))" 2>/dev/null || echo "")
        echo "  Enviando OTA para $dev_ip..."
        if $PIO run -d "$project_dir" -e "$env_name" --target upload --upload-port "$dev_ip"; then
            echo -e "  \e[32m[OK] $dev_ip -> $local_ver\e[0m"
        else
            echo -e "  \e[31mOTA falhou: $dev_ip\e[0m" >&2
        fi
    done
}

# ---- main ----

PIO=$(find_pio)

RAW_DEVICES=$(discover_devices)
if [[ -z "$RAW_DEVICES" ]]; then
    echo "Nenhum dispositivo encontrado."; exit 0
fi

echo -e "\n\e[36mDispositivos encontrados:\e[0m"
echo "$RAW_DEVICES" | python3 -c "
import sys, json
for d in json.load(sys.stdin):
    print(f\"  [{d.get('type','?')}] {d.get('ip','?')}  FW={d.get('fw_version','?')}  platform={d.get('platform','?')}\")
"

# Hub ESP32
GW_ESP32=$(echo "$RAW_DEVICES" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
result = [d for d in devices if d.get('type')=='gateway' and d.get('platform','')!='esp8266']
print(json.dumps(result))" 2>/dev/null || echo "[]")

if [[ "$GW_ESP32" != "[]" ]] && [[ -n "$GW_ESP32" ]]; then
    mapfile -t devs < <(echo "$GW_ESP32" | python3 -c "import sys,json; [print(json.dumps(d)) for d in json.load(sys.stdin)]")
    update_device_type "$ROOT/hub" "hub_32_ota" "hub (esp32)" "${devs[@]}"
fi

# Hub ESP8266
GW_8266=$(echo "$RAW_DEVICES" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
result = [d for d in devices if d.get('type')=='gateway' and d.get('platform','')=='esp8266']
print(json.dumps(result))" 2>/dev/null || echo "[]")

if [[ "$GW_8266" != "[]" ]] && [[ -n "$GW_8266" ]]; then
    mapfile -t devs < <(echo "$GW_8266" | python3 -c "import sys,json; [print(json.dumps(d)) for d in json.load(sys.stdin)]")
    update_device_type "$ROOT/hub" "hub_8266_ota" "hub (esp8266)" "${devs[@]}"
fi

# Nodes: dir por tipo
declare -A NODE_DIR=(
    [lampada]="lamp"
    [dht_gas]="climate-gas"
    [pir]="presence"
    [repeater]="extender"
    [onoff]="switch"
)

declare -A NODE_ENV=(
    [lampada_esp8266]="esp8266_ota"
    [dht_gas_esp8266]="esp8266_ota"
    [pir_esp8266]="esp8266_ota"
    [repeater_esp8266]="esp8266_ota"
    [onoff_esp8266]="esp8266_ota"
)

# Agrupa devices por tipo
for type in "${!NODE_DIR[@]}"; do
    type_devices=$(echo "$RAW_DEVICES" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
result = [d for d in devices if d.get('type')=='$type']
print(json.dumps(result))" 2>/dev/null || echo "[]")

    [[ "$type_devices" == "[]" ]] && continue

    # Agrupa por plataforma
    platforms=$(echo "$type_devices" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
plats = set(d.get('platform','esp8266') for d in devices)
for p in sorted(plats): print(p)" 2>/dev/null)

    while IFS= read -r platform; do
        [[ -z "$platform" ]] && continue
        key="${type}_${platform}"
        env="${NODE_ENV[$key]:-esp8266_ota}"
        dir="$ROOT/nodes/${NODE_DIR[$type]}"

        group=$(echo "$type_devices" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
result = [d for d in devices if d.get('platform','esp8266')=='$platform']
print(json.dumps(result))" 2>/dev/null)

        [[ "$group" == "[]" ]] && continue

        mapfile -t devs < <(echo "$group" | python3 -c "import sys,json; [print(json.dumps(d)) for d in json.load(sys.stdin)]")
        update_device_type "$dir" "$env" "$key" "${devs[@]}"
    done <<< "$platforms"
done

# Desconhecidos
unknown=$(echo "$RAW_DEVICES" | python3 -c "
import sys, json
devices = json.load(sys.stdin)
known = {'gateway','lampada','dht_gas','pir','repeater','onoff'}
result = [d for d in devices if d.get('type','') not in known]
if result:
    print('   '.join(f\"{d.get('ip','')} ({d.get('type','')})\" for d in result))
" 2>/dev/null)

if [[ -n "$unknown" ]]; then
    echo -e "\n  \e[90m[ignored] tipo desconhecido:\e[0m"
    echo -e "           \e[90m$unknown\e[0m"
fi

echo -e "\n\e[32m=== Concluído ===\e[0m"
