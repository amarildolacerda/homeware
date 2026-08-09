#!/usr/bin/env python3
"""Monitoramento da integração ESP-NOW entre o lamp (node) e o hub.

Uso:
    python monitor_integration.py [--hub=IP] [--lamp=IP] [--interval=N] [--once]

Monitora:
  - Reachability (ping + HTTP /api/state)
  - Estado do ESP-NOW (paired, gateway_connected, tx_count, rx_count)
  - Estado do repeater (repeater_supported, repeater_enabled, repeater_fwd)
  - Canal WiFi (deve ser igual entre lamp e hub)
  - RSSI e uptime
  - Estado do relay
"""
import argparse
import json
import socket
import subprocess
import sys
import time
from datetime import datetime
from urllib.request import urlopen
from urllib.error import URLError, HTTPError

HUB_IP = "192.168.1.14"
LAMP_IP = "192.168.1.114"
TIMEOUT = 5


def ping(ip, count=1):
    try:
        out = subprocess.run(
            ["ping", "-n", str(count), "-w", "2000", ip],
            capture_output=True, text=True, timeout=10
        )
        return "TTL=" in out.stdout or "tempo=" in out.stdout
    except Exception:
        return False


def http_get(url):
    try:
        resp = urlopen(url, timeout=TIMEOUT)
        return json.loads(resp.read())
    except (URLError, HTTPError, socket.timeout, Exception):
        return None


def get_state(ip):
    return http_get(f"http://{ip}/api/state")


def get_hub_info(ip):
    return http_get(f"http://{ip}/api/info")


def get_sensors(ip):
    return http_get(f"http://{ip}/api/sensors")


def fmt_row(label, value, width=24):
    return f"  {label:<{width}}{value}"


def print_header(title):
    print()
    print("=" * 70)
    print(f"  {title}")
    print("=" * 70)


def print_lamp_status(ip):
    print_header(f"LAMPIADA ({ip})")
    state = get_state(ip)
    if state is None:
        print(fmt_row("HTTP", "OFFLINE (timeout)"))
        if not ping(ip):
            print(fmt_row("PING", "OFFLINE"))
        else:
            print(fmt_row("PING", "ONLINE (HTTP nao responde)"))
        print(fmt_row("ESP-NOW", "Indisponivel -- HTTP offline"))
        return

    print(fmt_row("HTTP", "ONLINE"))
    print(fmt_row("PING", "ONLINE" if ping(ip) else "OFFLINE"))
    print(fmt_row("Device ID", state.get("device_id", "?")))
    print(fmt_row("Nome", state.get("device_name", "?")))
    print(fmt_row("IP", state.get("ip", "?")))
    print(fmt_row("RSSI", f'{state.get("rssi", "?")} dBm'))
    print(fmt_row("Canal WiFi", state.get("wifi_channel", "?")))
    print(fmt_row("Relay", "ON" if state.get("state") else "OFF"))
    print(fmt_row("Uptime", state.get("uptime", "?")))

    # ESP-NOW status
    paired = state.get("paired", False)
    gw_conn = state.get("gateway_connected", False)
    print(fmt_row("ESP-NOW paired", "SIM" if paired else "NÃO"))
    print(fmt_row("Gateway conectado", "SIM" if gw_conn else "NÃO"))
    print(fmt_row("TX count", state.get("tx_count", 0)))
    print(fmt_row("RX count", state.get("rx_count", 0)))
    print(fmt_row("Slot", state.get("slot", "?")))

    # Repeater status
    rep_supported = state.get("repeater_supported", None)
    if rep_supported is None:
        print(fmt_row("Repeater supported", "NÃO (flag não compilada)"))
    else:
        rep_en = state.get("repeater_enabled", False)
        rep_fwd = state.get("repeater_fwd", 0)
        print(fmt_row("Repeater supported", "SIM" if rep_supported else "NÃO"))
        print(fmt_row("Repeater enabled", "SIM" if rep_en else "NÃO"))
        print(fmt_row("Repeater forwards", rep_fwd))
        clients = state.get("repeater_clients", [])
        if clients:
            print(fmt_row("Repeater clients", f"{len(clients)}"))
            for c in clients:
                print(fmt_row("  └─", f"{c.get('mac','?')}: {c.get('packets',0)} pkts"))


def print_hub_status(ip):
    print_header(f"HUB GATEWAY ({ip})")
    info = get_hub_info(ip)
    if info is None:
        print(fmt_row("HTTP", "OFFLINE (timeout)"))
        if not ping(ip):
            print(fmt_row("PING", "OFFLINE"))
        else:
            print(fmt_row("PING", "ONLINE (HTTP não responde)"))
        return

    print(fmt_row("HTTP", "ONLINE"))
    print(fmt_row("PING", "ONLINE" if ping(ip) else "OFFLINE"))
    print(fmt_row("Gateway ID", info.get("gateway_id", "?")))
    print(fmt_row("FW Version", info.get("fw_version", "?")))
    print(fmt_row("Platform", info.get("platform", "?")))
    print(fmt_row("IP", info.get("ip", "?")))
    print(fmt_row("Canal WiFi", info.get("wifi_channel", "?")))
    print(fmt_row("SSID", info.get("wifi_ssid", "?")))
    print(fmt_row("RSSI", f'{info.get("wifi_rssi", info.get("rssi", "?"))} dBm'))

    # Paired devices
    sensors = get_sensors(ip)
    if sensors:
        online = sum(1 for s in sensors if s.get("online"))
        print(fmt_row("Dispositivos pareados", len(sensors)))
        print(fmt_row("Dispositivos online", online))
        for s in sensors:
            status = "ONLINE" if s.get("online") else "OFFLINE"
            rssi = s.get("last_rssi", -127)
            rssi_str = f"{rssi} dBm" if rssi > -127 else "--"
            print(fmt_row(f"  +-- {s.get('name','?')}", f"{status}  RSSI={rssi_str}  slot={s.get('slot','?')}"))
    else:
        print(fmt_row("Dispositivos pareados", "N/A (API não disponível)"))


def check_channel_mismatch(hub_ip, lamp_ip):
    """Verifica se lamp e hub estão no mesmo canal WiFi (Regra 25)."""
    hub = get_hub_info(hub_ip)
    lamp = get_state(lamp_ip)
    if hub and lamp:
        hub_ch = hub.get("wifi_channel")
        lamp_ch = lamp.get("wifi_channel")
        if hub_ch and lamp_ch and hub_ch != lamp_ch:
            print()
            print("  ALERTA: MISMATCH DE CANAL WiFi")
            print(f"      Hub ({hub_ip}):  canal {hub_ch}")
            print(f"      Lamp ({lamp_ip}): canal {lamp_ch}")
            print(f"      ESP-NOW requer o MESMO canal (Regra 25)!")
            return True
        elif hub_ch and lamp_ch:
            print()
            print("  ✓ Canais WiFi alinhados: hub={}, lamp={}".format(hub_ch, lamp_ch))
            return False
    return None


def check_pairing(hub_ip, lamp_ip):
    """Verifica se o lamp está pareado e online no hub."""
    sensors = get_sensors(hub_ip)
    lamp_state = get_state(lamp_ip)
    if not sensors or not lamp_state:
        return

    lamp_mac = None
    lamp_name = lamp_state.get("device_name", "?")
    for s in sensors:
        if s.get("name") == lamp_name or s.get("bridge_device_id", "").endswith(lamp_state.get("device_id", "").split("_")[-1]):
            lamp_mac = s.get("mac")
            if not s.get("online"):
                print()
                print("  ALERTA: Lamp pareado no hub mas OFFLINE")
                print(f"      Slot {s.get('slot')}: {s.get('name')}")
                print(f"      RSSI: {s.get('last_rssi', -127)} (sem sinal ESP-NOW)")
                print(f"      Último contato: nunca (last_seen={s.get('last_seen')})")
                print(f"      Solução: verificar canal WiFi e REPEATER_ENABLED")
            else:
                print()
            print("  OK: Lamp ONLINE no hub -- ESP-NOW funcionando!")
            return


def run_once(hub_ip, lamp_ip):
    now = datetime.now().strftime("%H:%M:%S")
    print(f"\n[{now}] Coletando dados...")
    print_lamp_status(lamp_ip)
    print_hub_status(hub_ip)
    check_channel_mismatch(hub_ip, lamp_ip)
    check_pairing(hub_ip, lamp_ip)
    now2 = datetime.now().strftime("%H:%M:%S")
    print(f"\n[{now2}] OK\n")


def main():
    parser = argparse.ArgumentParser(description="Monitora integração ESP-NOW lamp↔hub")
    parser.add_argument("--hub", default=HUB_IP, help=f"IP do hub (default: {HUB_IP})")
    parser.add_argument("--lamp", default=LAMP_IP, help=f"IP do lamp (default: {LAMP_IP})")
    parser.add_argument("--interval", type=int, default=10, help="Intervalo entre amostras (s)")
    parser.add_argument("--once", action="store_true", help="Coleta uma vez e sai")
    args = parser.parse_args()

    print("Monitor de Integracao ESP-NOW -- Lamp <--> Hub")
    print(f"  Hub:  {args.hub}")
    print(f"  Lamp: {args.lamp}")
    print(f"  Intervalo: {args.interval}s")

    try:
        while True:
            run_once(args.hub, args.lamp)
            if args.once:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nMonitoramento interrompido pelo usuário.")
        sys.exit(0)


if __name__ == "__main__":
    main()
