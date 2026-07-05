from __future__ import annotations
import json
import os

from pydantic_settings import BaseSettings


CONFIG_FILE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config.json")


def load_config_file() -> dict:
    if os.path.isfile(CONFIG_FILE):
        try:
            with open(CONFIG_FILE) as f:
                return json.load(f)
        except Exception:
            pass
    return {}


class Settings(BaseSettings):
    mqtt_host: str = "core-mosquitto"
    mqtt_port: int = 1883
    mqtt_user: str = ""
    mqtt_pass: str = ""
    log_level: str = "info"
    http_port: int = 80
    discovery_port: int = 5000
    bridge_ip: str = ""
    data_dir: str = "/data/bridge_python"
    addon_slug: str = "bridge_python"
    version: str = "v0.0.16"

    model_config = {"env_prefix": ""}


for k, v in load_config_file().items():
    key = k.upper()
    if v is not None and key not in os.environ:
        os.environ.setdefault(key, str(v))

settings = Settings()

def save_settings(updates: dict) -> dict:
    current = load_config_file()
    current.update(updates)
    os.makedirs(os.path.dirname(CONFIG_FILE), exist_ok=True)
    with open(CONFIG_FILE, "w") as f:
        json.dump(current, f, indent=2)
    return current
