#!/usr/bin/env python3
import argparse
import subprocess
import sys
import re
import xml.etree.ElementTree as ET

def run_nmap(ip, scan_type, ports, extra_args):
    cmd = ["nmap"]
    if scan_type == "quick":
        cmd += ["-F"]
    elif scan_type == "full":
        cmd += ["-p-"]
    elif scan_type == "service":
        cmd += ["-sV"]
    else:
        cmd += ["-sV", "--version-intensity", "5"]
    if ports:
        cmd += ["-p", ports]
    cmd += extra_args
    cmd += ["-oX", "-", ip]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0 and not result.stdout:
            print(f"nmap error: {result.stderr.strip()}")
            return None
        return result.stdout
    except FileNotFoundError:
        print("nmap not found. Install: sudo apt install nmap")
        sys.exit(1)
    except subprocess.TimeoutExpired:
        print(f"nmap timeout after 120s for {ip}")
        return None

def parse_nmap_xml(xml_str):
    try:
        root = ET.fromstring(xml_str)
    except ET.ParseError:
        print("Failed to parse nmap output")
        return None
    results = []
    for host in root.findall(".//host"):
        status = host.find("status")
        if status is None or status.get("state") != "up":
            continue
        addr_el = host.find("address[@addrtype='ipv4']")
        ip = addr_el.get("addr") if addr_el is not None else "unknown"
        mac_el = host.find("address[@addrtype='mac']")
        mac = mac_el.get("addr") if mac_el is not None else None
        mac_vendor = mac_el.get("vendor") if mac_el is not None else None
        hostnames = []
        for hn in host.findall(".//hostname"):
            hostnames.append(hn.get("name", ""))
        ports = []
        for port in host.findall(".//port"):
            portid = port.get("portid", "")
            proto = port.get("protocol", "")
            state_el = port.find("state")
            state = state_el.get("state", "") if state_el is not None else ""
            service_el = port.find("service")
            svc = {}
            if service_el is not None:
                for attr in ["name", "product", "version", "extrainfo", "method", "ostype"]:
                    val = service_el.get(attr)
                    if val:
                        svc[attr] = val
            scripts = []
            for script in port.findall("script"):
                scripts.append({
                    "id": script.get("id", ""),
                    "output": script.get("output", "")
                })
            ports.append({
                "port": portid,
                "proto": proto,
                "state": state,
                "service": svc,
                "scripts": scripts
            })
        results.append({
            "ip": ip,
            "mac": mac,
            "mac_vendor": mac_vendor,
            "hostnames": hostnames,
            "ports": ports
        })
    return results

def print_results(results, show_scripts):
    if not results:
        print("No hosts found up.")
        return
    for h in results:
        print(f"\n{'='*60}")
        print(f"  IP:    {h['ip']}")
        if h["mac"]:
            print(f"  MAC:   {h['mac']}")
            if h["mac_vendor"]:
                print(f"  Vendor: {h['mac_vendor']}")
        if h["hostnames"]:
            print(f"  Names: {', '.join(h['hostnames'])}")
        if not h["ports"]:
            print("  Ports: none open")
            continue
        print(f"  Ports ({len(h['ports'])} open):")
        for p in h["ports"]:
            svc = p["service"]
            svc_name = svc.get("name", "")
            svc_product = svc.get("product", "")
            svc_version = svc.get("version", "")
            extra = svc.get("extrainfo", "")
            ver_str = f" {svc_version}" if svc_version else ""
            product_str = f" ({svc_product})" if svc_product else ""
            extra_str = f" {extra}" if extra else ""
            print(f"    {p['port']:>5}/{p['proto']:<4} {p['state']:<12} {svc_name}{product_str}{ver_str}{extra_str}")
            if show_scripts and p["scripts"]:
                for s in p["scripts"]:
                    for line in s["output"].split("\n"):
                        print(f"           | {line}")
    print(f"\n{'='*60}")
    total_ports = sum(len(h["ports"]) for h in results)
    print(f"  Hosts: {len(results)}  |  Open ports: {total_ports}")

def main():
    parser = argparse.ArgumentParser(
        description="whois.py - Network reconnaissance via nmap",
        epilog="Examples:\n"
               "  python3 whois.py 192.168.1.1\n"
               "  python3 whois.py 192.168.1.0/24 -t quick\n"
               "  python3 whois.py 10.0.0.5 -p 80,443,8080 -s service\n",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("target", help="IP, hostname, or CIDR range (e.g. 192.168.1.1 or 192.168.1.0/24)")
    parser.add_argument("-t", "--type", choices=["quick", "full", "service", "default"],
                        default="default", help="Scan type (default: default)")
    parser.add_argument("-p", "--ports", default=None,
                        help="Ports to scan (e.g. 80,443 or 1-1024). Overrides scan type defaults.")
    parser.add_argument("-s", "--scripts", action="store_true", help="Run default nmap scripts (NSE)")
    parser.add_argument("--os-detect", action="store_true", help="Enable OS detection")
    parser.add_argument("--traceroute", action="store_true", help="Enable traceroute")
    args = parser.parse_args()

    extra = []
    if args.scripts:
        extra.append("-sC")
    if args.os_detect:
        extra.append("-O")
    if args.traceroute:
        extra.append("--traceroute")

    scan_label = {"quick": "Quick (-F)", "full": "Full port (-p-)", "service": "Service version (-sV)", "default": "Default"}
    print(f"whois.py - nmap scan")
    print(f"  Target:  {args.target}")
    print(f"  Type:    {scan_label[args.type]}")
    if args.ports:
        print(f"  Ports:   {args.ports}")
    print(f"  Extra:   {', '.join(extra) if extra else 'none'}")
    print()

    xml_out = run_nmap(args.target, args.type, args.ports, extra)
    if not xml_out:
        print("No output from nmap.")
        sys.exit(1)

    results = parse_nmap_xml(xml_out)
    print_results(results, show_scripts=args.scripts)

if __name__ == "__main__":
    main()
