#!/usr/bin/env python3
"""Reinicia gateway(s) via POST /api/restart"""
import sys
import os
import subprocess
import json
from urllib.request import urlopen, Request
from urllib.error import URLError

GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
CYAN = "\033[96m"
RESET = "\033[0m"

def restart(ip):
    try:
        req = Request(f"http://{ip}/api/restart", method="POST",
                      headers={"Content-Type": "application/json"})
        resp = urlopen(req, timeout=10)
        code = resp.getcode()
        if code == 200:
            print(f"  {ip} ok (HTTP {code})")
        elif code == 400:
            print(f"  {ip} erro: max sensors reached or already pairing")
        else:
            print(f"  {ip} falha: HTTP {code}")
    except URLError as e:
        print(f"  {ip} {RED}falha: {e}{RESET}")
    except Exception as e:
        print(f"  {ip} {RED}falha: {e}{RESET}")

def main():
    args = sys.argv[1:]

    if not args or args[0] in ("-h", "--help"):
        print("Uso:")
        print("  python restart.py <ip>       Reinicia um gateway")
        print("  python restart.py --all      Reinicia todos os dispositivos da rede")
        return

    if args[0] == "--all":
        script_dir = os.path.dirname(os.path.abspath(__file__))
        scan_py = os.path.join(os.path.dirname(script_dir), "scan.py")
        if not os.path.exists(scan_py):
            print(f"{RED}scan.py nao encontrado em {scan_py}{RESET}")
            sys.exit(1)
        print(f"{CYAN}Scanning rede...{RESET}")
        result = subprocess.run([sys.executable, scan_py, "--json"],
                                capture_output=True, text=True)
        devices = json.loads(result.stdout) if result.stdout.strip() else []
        if not devices:
            print(f"{YELLOW}Nenhum dispositivo encontrado na rede{RESET}")
            return
        print(f"{CYAN}Reiniciando {len(devices)} dispositivo(s):{RESET}")
        for dev in devices:
            restart(dev["ip"])
    else:
        restart(args[0])

if __name__ == "__main__":
    main()
