#!/usr/bin/env python3
"""CLI para enviar comandos HTTP aos dispositivos ESP-NOW (clients/gateway).

Uso:
  python3 device_cli.py <ip> state
  python3 device_cli.py <ip> restart
  python3 device_cli.py <ip> settings
  python3 device_cli.py <ip> settings --name "Novo Nome"
  python3 device_cli.py <ip> pin --gpio 5
  python3 device_cli.py <ip> pin --gpio 5 --state 1
  python3 device_cli.py <ip> docs
  python3 device_cli.py <ip> root
"""
import argparse
import json
import os
import socket
import sys
import threading
import time
import urllib.request
import urllib.error

DEFAULT_PORT = 80
TELNET_PORT = 23


def _req(ip, path, method="GET", data=None, port=DEFAULT_PORT, timeout=5):
    url = f"http://{ip}:{port}{path}"
    body = None
    headers = {"User-Agent": "device_cli.py"}
    if data is not None:
        if isinstance(data, (dict, list)):
            body = json.dumps(data).encode()
            headers["Content-Type"] = "application/json"
        else:
            body = data.encode() if isinstance(data, str) else data
    r = urllib.request.Request(url, data=body, headers=headers, method=method)
    try:
        with urllib.request.urlopen(r, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", errors="ignore")
            return resp.status, raw
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="ignore")
    except Exception as e:  # noqa: BLE001
        return None, str(e)


def _print(status, raw):
    if status is None:
        print(f"ERRO: {raw}")
        sys.exit(1)
    print(f"HTTP {status}")
    try:
        print(json.dumps(json.loads(raw), indent=2, ensure_ascii=False))
    except (ValueError, TypeError):
        print(raw)


def _monitor(ip, port=TELNET_PORT):
    """Conecta via telnet e espelha o console do device (Ctrl+C para sair)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    try:
        sock.connect((ip, port))
    except Exception as e:  # noqa: BLE001
        print(f"Erro ao conectar telnet {ip}:{port}: {e}")
        sys.exit(1)
    sock.setblocking(False)
    print(f"Console remoto {ip}:{port} (Ctrl+C para sair)\n")

    def reader():
        try:
            while True:
                try:
                    d = sock.recv(4096)
                    if not d:
                        print("\n[Conexao fechada]")
                        os._exit(0)
                    sys.stdout.buffer.write(d)
                    sys.stdout.buffer.flush()
                except (BlockingIOError, socket.timeout):
                    time.sleep(0.05)
                except (ConnectionResetError, OSError):
                    print("\n[Conexao perdida]")
                    os._exit(0)
        except Exception:
            os._exit(0)

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    try:
        while True:
            d = sys.stdin.buffer.read(1)
            if not d:
                break
            sock.sendall(d)
    except (EOFError, KeyboardInterrupt):
        print("\nEncerrando...")


def _flash(ip, port, file_path):
    """Envia firmware via /api/ota (POST multipart) ao device."""
    import urllib.parse
    boundary = "----devicecliboundary"
    try:
        with open(file_path, "rb") as f:
            data = f.read()
    except OSError as e:
        print(f"ERRO ao ler firmware: {e}")
        sys.exit(1)
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="firmware"; filename="firmware.bin"\r\n'
        f"Content-Type: application/octet-stream\r\n\r\n"
    ).encode() + data + f"\r\n--{boundary}--\r\n".encode()
    headers = {
        "User-Agent": "device_cli.py",
        "Content-Type": f"multipart/form-data; boundary={boundary}",
        "Content-Length": str(len(body)),
    }
    url = f"http://{ip}:{port}/api/ota"
    r = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(r, timeout=60) as resp:
            print(f"HTTP {resp.status}: {resp.read().decode('utf-8', errors='ignore')}")
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code}: {e.read().decode('utf-8', errors='ignore')}")
    except Exception as e:  # noqa: BLE001
        print(f"ERRO: {e}")


def main():
    p = argparse.ArgumentParser(description="Envia comandos HTTP a um device ESP-NOW")
    p.add_argument("ip", help="IP do dispositivo")
    p.add_argument("command", choices=["state", "restart", "settings", "pin", "docs", "root", "monitor", "flash"])
    p.add_argument("-p", "--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--name", help="novo device_name (settings)")
    p.add_argument("--gpio", type=int, help="pino (pin)")
    p.add_argument("--state", type=int, choices=[0, 1], help="estado 0/1 (pin POST)")
    p.add_argument("--file", help="caminho do firmware.bin (flash)")
    args = p.parse_args()

    if args.command == "monitor":
        _monitor(args.ip)
        return

    if args.command == "flash":
        if not args.file:
            print("ERRO: informe --file <firmware.bin>")
            sys.exit(1)
        _flash(args.ip, args.port, args.file)
        return

    if args.command == "state":
        s, r = _req(args.ip, "/api/state", port=args.port)
    elif args.command == "restart":
        s, r = _req(args.ip, "/api/restart", method="POST", port=args.port)
    elif args.command == "settings":
        if args.name is not None:
            s, r = _req(args.ip, "/api/settings", method="POST",
                        data={"device_name": args.name}, port=args.port)
        else:
            s, r = _req(args.ip, "/api/settings", port=args.port)
    elif args.command == "pin":
        if args.state is not None:
            if args.gpio is None:
                print("ERRO: --gpio obrigatorio com --state")
                sys.exit(1)
            s, r = _req(args.ip, "/api/pin", method="POST",
                        data={"gpio": args.gpio, "state": args.state}, port=args.port)
        elif args.gpio is not None:
            s, r = _req(args.ip, f"/api/pin?gpio={args.gpio}", port=args.port)
        else:
            print("ERRO: informe --gpio")
            sys.exit(1)
    elif args.command == "docs":
        s, r = _req(args.ip, "/docs", port=args.port)
    elif args.command == "root":
        s, r = _req(args.ip, "/", port=args.port)
    _print(s, r)


if __name__ == "__main__":
    main()
