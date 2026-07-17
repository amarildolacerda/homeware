#!/usr/bin/env python3
"""Cliente HTTP para o gateway ESP-NOW.

Uso:
    python gateway_client.py [host] comando [args]

Comandos:
    info                      - status geral do gateway
    sensors                   - lista sensores pareados
    logs                      - logs do ring buffer
    clear-logs                - limpa os logs
    pair-start                - abre janela de pareamento
    pair-stop                 - fecha janela de pareamento
    clear                     - remove todos os sensores
    broadcast                 - forca publish MQTT de todos
    sensor <slot> name <nome> - renomeia sensor
    sensor <slot> on|off      - envia comando ON/OFF (ONOFF/LIGHT)
    sensor <slot> remove      - remove sensor
    restart                   - reinicia o gateway
    mqtt                      - mostra config MQTT
    wifi                      - mostra config WiFi

Exemplos:
    python gateway_client.py 192.168.1.14 sensors
    python gateway_client.py 192.168.1.14 sensor 2 on
    python gateway_client.py info
"""
import sys
import json
import urllib.request
import urllib.error

DEFAULT_HOST = "192.168.1.14"
TIMEOUT = 8


def _request(host, path, method="GET", data=None):
    url = f"http://{host}{path}"
    headers = {"Content-Type": "application/json"}
    body = None
    if data is not None:
        body = json.dumps(data).encode("utf-8")
    req = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
            raw = resp.read().decode("utf-8", "replace")
            return resp.status, raw
    except urllib.error.HTTPError as e:
        raw = e.read().decode("utf-8", "replace")
        return e.code, raw
    except Exception as e:  # noqa: BLE001
        return -1, str(e)


def _pretty(raw):
    try:
        return json.dumps(json.loads(raw), indent=2, ensure_ascii=False)
    except Exception:
        return raw


def main(argv):
    args = list(argv)
    host = DEFAULT_HOST
    if args and not args[0].startswith("-") and "/" not in args[0] and args[0] not in (
        "info", "sensors", "logs", "clear-logs", "pair-start", "pair-stop",
        "clear", "broadcast", "restart", "mqtt", "wifi", "sensor",
    ):
        host = args.pop(0)

    if not args:
        print(__doc__)
        return 1

    cmd = args.pop(0)

    if cmd == "info":
        st, raw = _request(host, "/api/info")
    elif cmd == "sensors":
        st, raw = _request(host, "/api/sensors")
    elif cmd == "logs":
        st, raw = _request(host, "/api/logs")
    elif cmd == "clear-logs":
        st, raw = _request(host, "/api/logs/clear", method="POST")
    elif cmd == "pair-start":
        st, raw = _request(host, "/api/pair/start", method="POST")
    elif cmd == "pair-stop":
        st, raw = _request(host, "/api/pair/stop", method="POST")
    elif cmd == "clear":
        st, raw = _request(host, "/api/clear", method="POST")
    elif cmd == "broadcast":
        st, raw = _request(host, "/api/broadcast", method="POST")
    elif cmd == "restart":
        st, raw = _request(host, "/api/restart", method="POST")
    elif cmd == "mqtt":
        st, raw = _request(host, "/api/config/mqtt")
    elif cmd == "wifi":
        st, raw = _request(host, "/api/config/wifi")
    elif cmd == "sensor":
        if len(args) < 2:
            print("uso: sensor <slot> name <nome> | on | off | remove")
            return 1
        slot = args[0]
        action = args[1]
        if action == "name":
            if len(args) < 3:
                print("uso: sensor <slot> name <nome>")
                return 1
            name = " ".join(args[2:])
            st, raw = _request(host, f"/api/sensor/{slot}/name", method="POST",
                               data={"name": name})
        elif action in ("on", "off"):
            st, raw = _request(host, f"/api/sensor/{slot}/command", method="POST",
                               data={"state": action == "on"})
        elif action == "remove":
            st, raw = _request(host, f"/api/sensor/{slot}/remove", method="POST")
        else:
            print(f"acao desconhecida: {action}")
            return 1
    else:
        print(f"comando desconhecido: {cmd}")
        print(__doc__)
        return 1

    if st == -1:
        print(f"ERRO ao conectar em {host}: {raw}")
        return 1

    print(f"HTTP {st}")
    print(_pretty(raw))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
