#!/usr/bin/env python3
from __future__ import annotations
import argparse
import asyncio
import json
import os
import sys
import time

_script_dir = os.path.dirname(os.path.abspath(__file__))
_venv_python = os.path.join(_script_dir, "venv", "bin", "python")
if os.path.exists(_venv_python) and sys.executable != _venv_python:
    os.execv(_venv_python, [_venv_python, __file__, *sys.argv[1:]])

def pretty(payload: str) -> str:
    try:
        obj = json.loads(payload)
        return json.dumps(obj, indent=2, ensure_ascii=False)
    except json.JSONDecodeError:
        return payload

async def run(host: str, port: int, topic: str, seconds: int, no_color: bool, username: str = "", password: str = ""):
    import aiomqtt

    if seconds <= 0:
        print(f"Conectado ao MQTT {host}:{port}, topico '{topic}' (Ctrl+C para sair)")
    else:
        print(f"Conectado ao MQTT {host}:{port}, topico '{topic}', tempo: {seconds}s")
    print("-" * 50)

    kwargs = dict(hostname=host, port=port)
    if username:
        kwargs["username"] = username
    if password:
        kwargs["password"] = password

    try:
        async with aiomqtt.Client(**kwargs) as client:
            await client.subscribe(topic)
            start = time.time()
            count = 0
            while True:
                try:
                    async with asyncio.timeout(seconds if seconds > 0 else None):
                        async for msg in client.messages:
                            elapsed = time.time() - start
                            payload = pretty(msg.payload.decode(errors="replace"))
                            if no_color:
                                print(f"[{elapsed:6.1f}s] {msg.topic}: {payload}")
                            else:
                                print(f"\033[36m[{elapsed:6.1f}s]\033[0m \033[33m{msg.topic}\033[0m: {payload}")
                            count += 1
                except asyncio.TimeoutError:
                    break
                except asyncio.CancelledError:
                    break
    except asyncio.TimeoutError:
        pass

    print("-" * 50)
    print(f"Total: {count} mensagens")

def main():
    parser = argparse.ArgumentParser(description="Escuta topicos MQTT")
    parser.add_argument("--host", default="localhost", help="Host MQTT")
    parser.add_argument("--port", type=int, default=1883, help="Porta MQTT")
    parser.add_argument("--topic", default="homeassistant/#", help="Topico ou pattern")
    parser.add_argument("-s", "--seconds", type=int, default=0, help="Tempo de escuta (0=infinito, padrao)")
    parser.add_argument("-u", "--username", default="", help="Usuario MQTT")
    parser.add_argument("-pw", "--password", default="", help="Senha MQTT")
    parser.add_argument("-n", "--no-color", action="store_true", help="Sem cores no output")
    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(1)
    args = parser.parse_args()

    try:
        asyncio.run(run(args.host, args.port, args.topic, args.seconds, args.no_color, args.username, args.password))
    except KeyboardInterrupt:
        print("\nInterrompido pelo usuario")

if __name__ == "__main__":
    main()
