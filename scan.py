#!/usr/bin/env python3
import os
import socket
import json
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.request import urlopen, Request
from urllib.error import URLError

def get_local_ip():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        return ip
    except:
        return None

def check_port(ip, port=80, timeout=1):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        result = sock.connect_ex((ip, port))
        sock.close()
        return ip if result == 0 else None
    except:
        return None

def fetch_title(ip, port):
    try:
        req = Request(f"http://{ip}:{port}/", method="GET", headers={"User-Agent": "scan.py"})
        resp = urlopen(req, timeout=2)
        html = resp.read().decode("utf-8", errors="ignore")
        start = html.lower().find("<title>")
        end = html.lower().find("</title>")
        if start != -1 and end != -1:
            return html[start+7:end].strip()
        return None
    except:
        return None

def get_mac_via_arp(ip):
    try:
        import subprocess, re
        if os.name == "nt":
            out = subprocess.check_output(f"arp -a {ip}", shell=True, timeout=3).decode("utf-8", errors="ignore")
            m = re.search(r"([0-9A-Fa-f]{2}[-:][0-9A-Fa-f]{2}[-:][0-9A-Fa-f]{2}[-:][0-9A-Fa-f]{2}[-:][0-9A-Fa-f]{2}[-:][0-9A-Fa-f]{2})", out)
        else:
            out = subprocess.check_output(f"arp -n {ip}", shell=True, timeout=3).decode("utf-8", errors="ignore")
            m = re.search(r"([0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2}:[0-9a-f]{2})", out, re.I)
        if m: return m.group(1).replace("-", ":")
    except: pass
    return None

def identify(ip, port):
    title = fetch_title(ip, port)
    mac = None
    
    try:
        req = Request(f"http://{ip}:{port}/api/info", method="GET", headers={"User-Agent": "scan.py"})
        resp = urlopen(req, timeout=2)
        data = json.loads(resp.read())
        gw_id = data.get("gateway_id", "")
        fw = data.get("fw_version", "")
        paired = data.get("paired_count", "?")
        online = data.get("online_count", "?")
        mac = data.get("gateway_mac") or data.get("mac") or data.get("sta_mac")
        is_gateway = "gateway" in gw_id or "gateway" in str(mac) or mac is not None
        label = "GATEWAY" if is_gateway else "bridge"
        info = f"  [{label}] {ip}:{port}  FW={fw}  paired={paired} online={online}  id={gw_id}"
        if mac: info += f"  MAC={mac}"
        if title: info += f"  title=\"{title}\""
        return (info, mac)
    except:
        mac = get_mac_via_arp(ip) if not mac else mac
        if title:
            info = f"  [HTTP] {ip}:{port}  title=\"{title}\""
            if mac: info += f"  MAC={mac}"
            return (info, mac)
        return None

def main():
    parser = argparse.ArgumentParser(description="Scan for ESP-NOW gateway on local network")
    parser.add_argument("-p", "--port", type=int, default=80, help="Port to scan (default: 80)")
    parser.add_argument("--mac", action="store_true", help="Show only gateway MAC (for repeater config)")
    args = parser.parse_args()

    local_ip = get_local_ip()
    if not local_ip:
        print("Could not detect local IP")
        return
    subnet = ".".join(local_ip.split(".")[:3]) + "."
    ips = [f"{subnet}{i}" for i in range(1, 253)]

    print(f"Scanning {len(ips)} IPs on {subnet}0/24 for port {args.port}...")
    print(f"Local IP: {local_ip}")
    found = []

    with ThreadPoolExecutor(max_workers=50) as executor:
        futures = {executor.submit(check_port, ip, args.port): ip for ip in ips}
        for future in as_completed(futures):
            result = future.result()
            if result:
                marker = "  <--" if result == local_ip else ""
                print(f"  Found: {result}:{args.port}{marker}")
                found.append(result)
            else:
                pass

    if not found:
        print(f"\nNo devices found with port {args.port} open")
        return

    print(f"\nIdentificando dispositivos...")
    found_gateway_mac = None
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = {executor.submit(identify, ip, args.port): ip for ip in found}
        for future in as_completed(futures):
            result = future.result()
            if result:
                info, mac = result
                print(info)
                if mac and not found_gateway_mac:
                    found_gateway_mac = mac

    if args.mac:
        if found_gateway_mac:
            print(f"\n{found_gateway_mac}")
        else:
            print("\nGateway MAC nao encontrado")

if __name__ == "__main__":
    main()
