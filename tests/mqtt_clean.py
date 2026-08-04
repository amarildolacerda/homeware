#!/usr/bin/env python3
"""
mqtt_clean.py — Limpa tópicos MQTT do Home Assistant (stdlib pura, sem pip).

Uso:
  python3 mqtt_clean.py                    # dry-run: mostra tópicos retained
  python3 mqtt_clean.py --apply            # apaga devices órfãos
  python3 mqtt_clean.py --full-reset       # apaga TUDO homeassistant/*
  python3 mqtt_clean.py --list             # lista todos os tópicos retained
  python3 mqtt_clean.py --debug            # mostra pacotes brutos
  python3 mqtt_clean.py --broker 192.168.1.12 --user kzuca --pass 123
"""

import argparse
import select
import socket
import struct
import sys
import time
from datetime import datetime

DEFAULT_BROKER = "192.168.1.12"
DEFAULT_PORT = 1883
DEFAULT_USER = "kzuca"
DEFAULT_PASS = "123"

# Devices ativos — adicione IDs parciais dos devices que quer PRESERVAR
KNOWN_DEVICES = [
    # ex: "gw294F55", "agri_lamp", "sacada",
]

DEBUG = False


def log(msg):
    if DEBUG:
        print(f"  [DBG] {msg}")


# ── MQTT 3.1.1 minimal client ───────────────────────────────────
class MQTTClient:
    def __init__(self, broker, port, user, password):
        self.broker = broker
        self.port = port
        self.user = user
        self.password = password
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(3)
        self.packet_id = 0
        self.topics = {}

    def _rl_encode(self, length):
        r = bytearray()
        while True:
            b = length % 128
            length //= 128
            if length > 0:
                b |= 0x80
            r.append(b)
            if length == 0:
                break
        return bytes(r)

    def _rl_decode(self, data, off):
        mul = 1
        val = 0
        i = off
        while i < len(data):
            b = data[i]
            val += (b & 0x7F) * mul
            mul *= 128
            i += 1
            if b < 128:
                return val, i - off
        return -1, 0

    def _send(self, data):
        log(f"SEND {len(data)}B: {data[:20].hex()}{'...' if len(data) > 20 else ''}")
        self.sock.sendall(data)

    def _recv(self, size=4096):
        try:
            d = self.sock.recv(size)
            if d:
                log(f"RECV {len(d)}B: {d[:40].hex()}{'...' if len(d) > 40 else ''}")
            return d
        except socket.timeout:
            return b""

    def _recv_wait(self, size=4096, timeout=5.0):
        """Recv com select para evitar timeout."""
        r, _, _ = select.select([self.sock], [], [], timeout)
        if r:
            return self._recv(size)
        return b""

    def connect(self):
        self.sock.connect((self.broker, self.port))
        # CONNECT
        proto = b"\x00\x04MQTT\x04"
        flags = 0xC2
        if self.user:
            flags |= 0x80
        if self.password:
            flags |= 0x40
        client_id = f"mqtt_clean_{int(time.time())}"
        payload = struct.pack(">H", len(client_id)) + client_id.encode()
        if self.user:
            payload += struct.pack(">H", len(self.user)) + self.user.encode()
        if self.password:
            payload += struct.pack(">H", len(self.password)) + self.password.encode()
        body = proto + bytes([flags]) + struct.pack(">H", 60) + payload
        self._send(bytes([0x10]) + self._rl_encode(len(body)) + body)

        # CONNACK
        resp = self._recv_wait(timeout=5)
        if len(resp) < 4:
            raise ConnectionError(f"CONNACK curto: {resp.hex() if resp else 'vazio'}")
        if resp[0] >> 4 != 2:
            raise ConnectionError(f"Primeiro byte não é CONNACK: 0x{resp[0]:02X}")
        rc = resp[3]
        if rc != 0:
            raise ConnectionError(f"CONNACK erro rc={rc}")
        print(f"✓ Conectado a {self.broker}:{self.port}")

    def subscribe(self, topic="#"):
        self.packet_id += 1
        tb = topic.encode()
        body = struct.pack(">H", self.packet_id) + struct.pack(">H", len(tb)) + tb + b"\x01"
        self._send(bytes([0x82]) + self._rl_encode(len(body)) + body)

        # SUBACK
        resp = self._recv_wait(timeout=5)
        if not resp:
            raise ConnectionError("SUBACK não recebido")
        if (resp[0] >> 4) != 9:
            raise ConnectionError(f"Não SUBACK: 0x{resp[0]:02X}")
        rl, sz = self._rl_decode(resp, 1)
        if rl < 0:
            raise ConnectionError("Remaining length inválido no SUBACK")
        print(f"✓ Subscrito a '{topic}'")

    def publish(self, topic, payload=b"", retain=True):
        tb = topic.encode()
        flags = 0x30 | (0x01 if retain else 0)
        body = struct.pack(">H", len(tb)) + tb + payload
        self._send(bytes([flags]) + self._rl_encode(len(body)) + body)

    def collect_retained(self, timeout=3):
        """Coleta tópicos retained lendo pacotes MQTT corretamente."""
        t0 = time.time()
        buf = bytearray()
        print(f"Coletando tópicos retained ({timeout}s)...")

        while time.time() - t0 < timeout:
            chunk = self._recv_wait(size=16384, timeout=min(1, timeout - (time.time() - t0)))
            if not chunk:
                continue
            buf.extend(chunk)
            self._process(buf)

        print(f"Total de tópicos retained: {len(self.topics)}")

    def _process(self, buf):
        """Processa pacotes MQTT completos do buffer."""
        while len(buf) > 0:
            first = buf[0]
            ptype = (first >> 4) & 0x0F

            # Ler remaining length
            rl, rlsz = self._rl_decode(buf, 1)
            if rl < 0:
                break  # incompleto

            header = 1 + rlsz
            total = header + rl
            if len(buf) < total:
                break  # incompleto

            # Pacote completo extraído
            pkt = bytes(buf[:total])
            del buf[:total]

            if ptype == 3:  # PUBLISH
                self._handle_publish(pkt, header, rl, first)
            else:
                log(f"Pkt tipo {ptype} ignorado ({rl}B)")

    def _handle_publish(self, pkt, header, rl, first_byte):
        """Extrai topic + payload de um PUBLISH."""
        idx = header
        # Topic
        if idx + 2 > len(pkt):
            return
        tlen = struct.unpack(">H", pkt[idx:idx+2])[0]
        idx += 2
        topic = pkt[idx:idx+tlen].decode("utf-8", errors="replace")
        idx += tlen

        # QoS
        qos = (first_byte >> 1) & 0x03
        if qos > 0:
            idx += 2  # packet id

        payload = pkt[idx:]
        retain = bool(first_byte & 0x01)

        if retain and len(payload) > 0:
            self.topics[topic] = {
                "payload": payload.decode("utf-8", errors="replace"),
                "retain": True,
                "qos": qos,
                "size": len(payload),
            }
            log(f"RETAINED: {topic} ({len(payload)}B)")

    def disconnect(self):
        try:
            self._send(bytes([0xE0, 0x00]))
        except Exception:
            pass
        self.sock.close()


# ── Limpeza ─────────────────────────────────────────────────────
def find_orphans(topics):
    orphans = []
    for t in sorted(topics):
        if not t.startswith("homeassistant/"):
            continue
        parts = t.split("/")
        if len(parts) < 3:
            continue
        entity = parts[2]
        if not any(d in entity for d in KNOWN_DEVICES):
            orphans.append(t)
    return orphans


def do_remove(client, topics, dry_run=True):
    if not topics:
        print("Nenhum tópico para remover.")
        return
    tag = "DRY-RUN" if dry_run else "REMOVENDO"
    print(f"\n{'='*50}")
    print(f"  {tag}: {len(topics)} tópicos")
    print(f"{'='*50}")
    for i, t in enumerate(topics, 1):
        info = client.topics.get(t, {})
        sz = info.get("size", 0)
        pl = info.get("payload", "")
        vis = pl[:80] + ("..." if len(pl) > 80 else "")
        print(f"  [{i}/{len(topics)}] {t}  [{sz}B]")
        if vis:
            print(f"           {vis}")
        if not dry_run:
            client.publish(t, b"", retain=True)
            time.sleep(0.05)
    print(f"\n{'Use --apply para remover.' if dry_run else f'{len(topics)} tópicos removidos.'}")


def main():
    global DEBUG
    p = argparse.ArgumentParser(description="Limpa MQTT do HA (sem pip)")
    p.add_argument("--broker", default=DEFAULT_BROKER)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--user", default=DEFAULT_USER)
    p.add_argument("--pass", dest="password", default=DEFAULT_PASS)
    p.add_argument("--apply", action="store_true")
    p.add_argument("--full-reset", action="store_true")
    p.add_argument("--wait", type=int, default=3)
    p.add_argument("--list", action="store_true")
    p.add_argument("--debug", action="store_true")
    a = p.parse_args()
    DEBUG = a.debug

    print(f"{'='*50}")
    print(f"  MQTT Clean — Home Assistant")
    print(f"  {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*50}")

    c = MQTTClient(a.broker, a.port, a.user, a.password)
    try:
        c.connect()
        c.subscribe("#")
        c.collect_retained(timeout=a.wait)
    except ConnectionError as e:
        print(f"✗ Erro: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrompido.")
        c.disconnect()
        return

    if not c.topics:
        print("\nNenhum tópico retained encontrado.")
        print("Possíveis causas:")
        print("  - Broker não está rodando?")
        print("  - Nenhum device publicando?")
        print("  - Credenciais erradas?")
        print("  - Tente --debug para ver pacotes brutos")
        return

    if a.list:
        print(f"\nTópicos retained ({len(c.topics)}):")
        for t, info in sorted(c.topics.items()):
            print(f"  {t}  [{info['size']}B]")
        return

    if a.full_reset:
        ha = [t for t in c.topics if t.startswith("homeassistant/")]
        print(f"\nFull reset: {len(ha)} tópicos homeassistant/*")
        if input("Confirma? (s/N): ").strip().lower() in ("s", "y"):
            do_remove(c, ha, dry_run=not a.apply)
    else:
        orphans = find_orphans(c.topics)
        if orphans:
            print(f"\nDevices órfãos: {len(orphans)}")
            do_remove(c, orphans, dry_run=not a.apply)
        else:
            print("\nNenhum device órfão.")

        known = [t for t in c.topics if any(d in t for d in KNOWN_DEVICES)]
        if known:
            print(f"\nPreservados ({len(known)}):")
            for t in sorted(known)[:10]:
                print(f"  ✓ {t}")
            if len(known) > 10:
                print(f"  ... +{len(known)-10}")

    c.disconnect()


if __name__ == "__main__":
    main()
