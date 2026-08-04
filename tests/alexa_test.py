#!/usr/bin/env python3
"""
alexa_test.py — stub para testar se Alexa está alcançando o Espalexa no ESP.

Simula o que um Echo faz durante discovery e controle:
  1. Envia M-SEARCH UDP (SSDP) e coleta respostas
  2. Baixa description.xml de cada bridge encontrado
  3. Testa a API Hue (devicetype, lights, state)
  4. Modo direto: testa um IP sem discovery

Uso:
  python3 alexa_test.py                     # discovery automático
  python3 alexa_test.py --ip 192.168.1.41   # testa IP direto
  python3 alexa_test.py --ip 192.168.1.41 --control  # testa on/off
"""

import socket
import struct
import time
import sys
import argparse
import xml.etree.ElementTree as ET
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError

SSDP_ADDR = "239.255.255.250"
SSDP_PORT = 1900
MSEARCH = (
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 3\r\n"
    "ST: urn:schemas-upnp-org:device:basic:1\r\n"
    "\r\n"
)

BOLD = "\033[1m"
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RESET = "\033[0m"


def log(tag, msg, color=CYAN):
    print(f"{color}{BOLD}[{tag}]{RESET} {msg}")


def log_ok(msg):
    log("OK", msg, GREEN)


def log_warn(msg):
    log("WARN", msg, YELLOW)


def log_fail(msg):
    log("FAIL", msg, RED)


def log_info(msg):
    log("INFO", msg, CYAN)


# ── SSDP Discovery ──────────────────────────────────────────────

def ssdp_discover(timeout=5):
    """Envia M-SEARCH e retorna lista de (ip, port, headers_dict)."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(timeout)
    sock.sendto(MSEARCH.encode(), (SSDP_ADDR, SSDP_PORT))

    results = []
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            break
        text = data.decode("utf-8", errors="replace")
        headers = {}
        for line in text.split("\r\n"):
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().upper()] = v.strip()
        results.append((addr[0], addr[1], headers))
        log_ok(f"SSDP response de {addr[0]}:{addr[1]}")
        for k, v in headers.items():
            print(f"    {k}: {v}")
    sock.close()
    return results


# ── HTTP helpers ─────────────────────────────────────────────────

def http_get(url, timeout=5):
    """GET simples, retorna (status, body_str)."""
    try:
        req = Request(url, headers={"User-Agent": "AlexaTest/1.0"})
        resp = urlopen(req, timeout=timeout)
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    except HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except URLError as e:
        return 0, str(e.reason)
    except Exception as e:
        return 0, str(e)


def http_post_raw(url, body_str, content_type="text/plain", timeout=5):
    """POST com body raw, retorna (status, body_str)."""
    try:
        headers = {"User-Agent": "AlexaTest/1.0"}
        if content_type:
            headers["Content-Type"] = content_type
        req = Request(
            url,
            data=body_str.encode(),
            headers=headers,
            method="POST",
        )
        resp = urlopen(req, timeout=timeout)
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    except HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except URLError as e:
        return 0, str(e.reason)
    except Exception as e:
        return 0, str(e)


def http_put_json(url, json_str, timeout=5):
    """PUT com body JSON, retorna (status, body_str)."""
    try:
        req = Request(
            url,
            data=json_str.encode(),
            headers={
                "Content-Type": "application/json",
                "User-Agent": "AlexaTest/1.0",
            },
            method="PUT",
        )
        resp = urlopen(req, timeout=timeout)
        body = resp.read().decode("utf-8", errors="replace")
        return resp.status, body
    except HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")
    except URLError as e:
        return 0, str(e.reason)
    except Exception as e:
        return 0, str(e)


# ── Description.xml parser ──────────────────────────────────────

def parse_description(xml_text):
    """Extrai campos-chave do description.xml."""
    info = {}
    try:
        root = ET.fromstring(xml_text)
        ns = {"d": "urn:schemas-upnp-org:device-1-0"}

        tags = {
            "friendlyName": "d:device/d:friendlyName",
            "modelName": "d:device/d:modelName",
            "modelNumber": "d:device/d:modelNumber",
            "serialNumber": "d:device/d:serialNumber",
            "manufacturer": "d:device/d:manufacturer",
            "udn": "d:device/d:UDN",
            "urlBase": "d:URLBase",
        }
        for key, path in tags.items():
            el = root.find(path, ns)
            if el is not None:
                info[key] = el.text or ""
    except ET.ParseError as e:
        info["parse_error"] = str(e)
    return info


# ── Testes completos ────────────────────────────────────────────

def test_bridge(ip, port=80, do_control=False):
    """Testa um bridge Espalexa no IP dado."""
    base = f"http://{ip}:{port}"
    print()
    log_info(f"═══ Testando bridge em {base} ═══")

    # 1) description.xml
    print(f"\n{BOLD}1. GET {base}/description.xml{RESET}")
    status, body = http_get(f"{base}/description.xml")
    if status == 200:
        log_ok(f"HTTP {status} — {len(body)} bytes")
        info = parse_description(body)
        if "parse_error" in info:
            log_fail(f"XML parse error: {info['parse_error']}")
        else:
            for k, v in info.items():
                print(f"    {k}: {v}")
    else:
        log_fail(f"HTTP {status} — {body[:200]}")
        return False

    # 2) POST /api (devicetype) — testa vários Content-Types
    print(f"\n{BOLD}2. POST {base}/api{RESET}")
    attempts = [
        ("text/plain", '{"devicetype":"test#alexa_test"}'),
        ("", '{"devicetype":"test#alexa_test"}'),
        ("application/x-www-form-urlencoded", "devicetype=test%23alexa_test"),
    ]
    for ct, body_data in attempts:
        label = ct or "(sem CT)"
        print(f"    {BOLD}tentativa: Content-Type={label}{RESET}")
        status, body = http_post_raw(f"{base}/api", body_data, content_type=ct)
        if status == 200:
            log_ok(f"HTTP {status} — {body[:200]}")
            import json
            try:
                data = json.loads(body)
                if isinstance(data, list) and len(data) > 0:
                    username = data[0].get("success", {}).get("username")
                    if username:
                        log_ok(f"API username: {username}")
                        break
            except Exception:
                pass
        else:
            log_warn(f"HTTP {status} — {body[:100]}")
    username = None
    if status == 200:
        log_ok(f"HTTP {status} — {body[:200]}")
        import json
        try:
            data = json.loads(body)
            if isinstance(data, list) and len(data) > 0:
                username = data[0].get("success", {}).get("username")
                if username:
                    log_ok(f"API username: {username}")
        except Exception:
            log_warn("Não conseguiu parsear username")
    else:
        log_fail(f"HTTP {status} — {body[:200]}")

    # 3) GET /api/<username>/lights
    if username:
        print(f"\n{BOLD}3. GET {base}/api/{username}/lights{RESET}")
        status, body = http_get(f"{base}/api/{username}/lights")
        if status == 200:
            log_ok(f"HTTP {status} — {len(body)} bytes")
            import json
            try:
                lights = json.loads(body)
                for lid, ldata in lights.items():
                    name = ldata.get("name", "?")
                    state = ldata.get("state", {})
                    on = state.get("on", "?")
                    bri = state.get("bri", "?")
                    reachable = state.get("reachable", "?")
                    print(f"    Light {lid}: {name} on={on} bri={bri} reachable={reachable}")
            except Exception as e:
                log_warn(f"Parse error: {e}")
        else:
            log_fail(f"HTTP {status} — {body[:200]}")

        # 4) Controle on/off
        if do_control:
            print(f"\n{BOLD}4. PUT {base}/api/{username}/lights/1/state (ON){RESET}")
            status, body = http_put_json(
                f"{base}/api/{username}/lights/1/state", '{"on":true}'
            )
            log_ok(f"HTTP {status} — {body[:200]}")

            time.sleep(2)

            print(f"\n{BOLD}5. PUT {base}/api/{username}/lights/1/state (OFF){RESET}")
            status, body = http_put_json(
                f"{base}/api/{username}/lights/1/state", '{"on":false}'
            )
            log_ok(f"HTTP {status} — {body[:200]}")

    # 5) /espalexa status page
    print(f"\n{BOLD}6. GET {base}/espalexa{RESET}")
    status, body = http_get(f"{base}/espalexa")
    if status == 200:
        log_ok(f"HTTP {status}")
        for line in body.split("\n")[:10]:
            print(f"    {line.rstrip()}")
    else:
        log_warn(f"HTTP {status} — {body[:200]}")

    return True


# ── Main ─────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Teste Alexa/Espalexa discovery")
    parser.add_argument("--ip", help="IP direto (pula discovery UDP)")
    parser.add_argument("--port", type=int, default=80, help="Porta HTTP (default 80)")
    parser.add_argument("--control", action="store_true", help="Testa on/off (cuidado!)")
    parser.add_argument("--timeout", type=int, default=5, help="Timeout discovery (s)")
    args = parser.parse_args()

    print(f"{BOLD}{'='*50}{RESET}")
    print(f"{BOLD}  Alexa/Espalexa Test Stub{RESET}")
    print(f"{BOLD}{'='*50}{RESET}")

    if args.ip:
        # Modo direto
        ok = test_bridge(args.ip, args.port, args.control)
        if ok:
            print(f"\n{GREEN}{BOLD}Bridge respondeu. Verifique se o Alexa está no mesmo roteador.{RESET}")
        else:
            print(f"\n{RED}{BOLD}Bridge não respondeu. Verifique IP/porta e se o ESP está ligado.{RESET}")
        sys.exit(0 if ok else 1)

    # Modo discovery
    print(f"\n{BOLD}Enviando M-SEARCH SSDP para {SSDP_ADDR}:{SSDP_PORT}...{RESET}")
    results = ssdp_discover(timeout=args.timeout)

    if not results:
        log_fail("Nenhum bridge encontrado via SSDP!")
        print(f"\nPossíveis causas:")
        print(f"  - ESP e PC não estão no mesmo roteador/sub-rede")
        print(f"  - UDP multicast bloqueado pelo roteador")
        print(f"  - Alexa UDP multicast falhou no ESP (ver serial)")
        print(f"  - FW não tem ALEXA_ENABLED definido")
        print(f"\nTente: python3 {sys.argv[0]} --ip <IP_DO_ESP>")
        sys.exit(1)

    # Testa cada bridge encontrado
    for ip, port, headers in results:
        location = headers.get("LOCATION", "")
        if location:
            log_info(f"Bridge encontrado em {ip}, description: {location}")

    # Testa o primeiro bridge
    test_bridge(results[0][0], args.port, args.control)


if __name__ == "__main__":
    main()
