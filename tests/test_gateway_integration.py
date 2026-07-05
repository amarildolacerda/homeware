from __future__ import annotations
import time
import pytest
from httpx import AsyncClient, ASGITransport
from app.http_api import create_app
from app.device_registry import DeviceRegistry, MAX_DEVICES


@pytest.fixture
def registry(tmp_path):
    reg = DeviceRegistry(data_dir=str(tmp_path))
    reg.load()
    return reg


@pytest.fixture
async def client(registry):
    app = create_app(registry)
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac


GATEWAY_ID = "esp8266_gateway_A1B2C3"
GATEWAY_IP = "192.168.1.50"


def gw_sensor_id(slot: int) -> str:
    return f"esp8266_gw_12AB34_sensor_{slot}"


@pytest.mark.asyncio
class TestGatewayRegistration:
    async def test_register_gateway(self, client):
        resp = await client.post("/api/device/register", json={
            "id": GATEWAY_ID,
            "type": "bridge",
            "name": "Gateway Living Room",
            "ip": GATEWAY_IP,
        })
        assert resp.status_code == 200
        assert resp.json()["status"] == "ok"

    async def test_register_sensor_with_parent_id(self, client):
        await client.post("/api/device/register", json={
            "id": GATEWAY_ID, "type": "bridge", "name": "GW", "ip": GATEWAY_IP,
        })
        sensor_id = gw_sensor_id(0)
        resp = await client.post("/api/device/register", json={
            "id": sensor_id,
            "type": "temperature",
            "name": "Temp Sensor 0",
            "ip": GATEWAY_IP,
            "parent_id": GATEWAY_ID,
        })
        assert resp.status_code == 200
        data = resp.json()
        assert data["status"] == "ok"

    async def test_register_multiple_sensors(self, client):
        await client.post("/api/device/register", json={
            "id": GATEWAY_ID, "type": "bridge", "name": "GW", "ip": GATEWAY_IP,
        })
        for i in range(5):
            resp = await client.post("/api/device/register", json={
                "id": gw_sensor_id(i),
                "type": "temperature",
                "name": f"Sensor {i}",
                "ip": GATEWAY_IP,
                "parent_id": GATEWAY_ID,
            })
            assert resp.status_code == 200

        resp = await client.get("/api/devices")
        devices = resp.json()
        assert len(devices) == 6

    async def test_gateway_devices_listed_in_info(self, client):
        await client.post("/api/device/register", json={
            "id": GATEWAY_ID, "type": "bridge", "name": "GW", "ip": GATEWAY_IP,
        })
        for i in range(3):
            await client.post("/api/device/register", json={
                "id": gw_sensor_id(i), "type": "temperature",
                "name": f"S{i}", "ip": GATEWAY_IP, "parent_id": GATEWAY_ID,
            })

        resp = await client.get("/api/gateway/info")
        data = resp.json()
        assert data["total_devices"] == 4

    async def test_register_duplicate_sensor_updates(self, client):
        sensor_id = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sensor_id, "type": "temperature", "name": "Old Name", "ip": "0.0.0.0",
        })
        await client.post("/api/device/register", json={
            "id": sensor_id, "type": "temperature", "name": "New Name", "ip": GATEWAY_IP,
        })
        resp = await client.get(f"/api/device/info?id={sensor_id}")
        data = resp.json()
        assert data["name"] == "New Name"
        assert data["ip"] == GATEWAY_IP

    async def test_register_full_registry(self, client):
        for i in range(MAX_DEVICES):
            await client.post("/api/device/register", json={
                "id": f"sensor_{i}", "type": "onoff", "name": f"S{i}", "ip": "",
            })
        resp = await client.post("/api/device/register", json={
            "id": "sensor_extra", "type": "onoff", "name": "Extra", "ip": "",
        })
        assert resp.status_code == 500
        assert resp.json()["status"] == "error"


@pytest.mark.asyncio
class TestGatewayStateUpdates:
    async def test_temperature_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "temperature", "name": "Temp", "ip": GATEWAY_IP,
        })
        resp = await client.post("/api/device/state", json={
            "id": sid, "temperature": 23.5, "humidity": 65.0, "battery": 85, "rssi": -60,
        })
        assert resp.status_code == 200
        resp = await client.get(f"/api/device/info?id={sid}")
        data = resp.json()
        assert data["state"]["temperature"] == 23.5
        assert data["state"]["humidity"] == 65.0
        assert data["state"]["rssi"] == -60
        assert data["state"]["battery"] == 85
        assert data["online"] is True

    async def test_contact_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "contact", "name": "Door", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "contact": 1, "tamper": 0, "battery": 90,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        assert resp.json()["state"]["contact"] == 1

    async def test_occupancy_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "occupancy", "name": "Motion", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "occupancy": 1, "duration": 30, "battery": 80,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        assert resp.json()["state"]["occupancy"] == 1

    async def test_gas_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "gas", "name": "Gas Detector", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "gas_level": 300, "alarm": 0, "battery": 75,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        data = resp.json()
        assert data["state"]["gas_level"] == 300
        assert data["state"]["alarm"] == 0

    async def test_rain_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "rain", "name": "Rain", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "rain_level": 75, "rain_digital": 1, "battery": 95,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        data = resp.json()
        assert data["state"]["rain_level"] == 75
        assert data["state"]["rain_digital"] == 1

    async def test_tanque_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "tanque", "name": "Water Tank", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "level_pct": 42, "distance_cm": 58, "battery": 88,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        data = resp.json()
        assert data["state"]["level_pct"] == 42
        assert data["state"]["distance_cm"] == 58

    async def test_dht_gas_state(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "dht_gas", "name": "DHT+Gas", "ip": GATEWAY_IP,
        })
        await client.post("/api/device/state", json={
            "id": sid, "temperature": 27.0, "humidity": 55.0,
            "gas_level": 200, "alarm": 0, "battery": 70,
        })
        resp = await client.get(f"/api/device/info?id={sid}")
        data = resp.json()
        assert data["state"]["temperature"] == 27.0
        assert data["state"]["humidity"] == 55.0
        assert data["state"]["gas_level"] == 200

    async def test_state_update_nonexistent_device(self, client):
        resp = await client.post("/api/device/state", json={
            "id": "nonexistent", "temperature": 23.5,
        })
        assert resp.status_code == 404

    async def test_state_missing_id(self, client):
        resp = await client.post("/api/device/state", json={"temperature": 23.5})
        assert resp.status_code == 400

    async def test_state_invalid_json(self, client):
        resp = await client.post("/api/device/state", content=b"not json",
                                 headers={"content-type": "application/json"})
        assert resp.status_code == 400


@pytest.mark.asyncio
class TestGatewayHeartbeat:
    async def test_heartbeat_updates_last_seen(self, client, registry):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "temperature", "name": "T1", "ip": GATEWAY_IP,
        })
        dev = registry.get_device(sid)
        dev.last_seen = time.time() - 30

        resp = await client.post("/api/device/heartbeat", json={"id": sid})
        assert resp.status_code == 200
        assert resp.json()["status"] == "ok"

        updated = registry.get_device(sid)
        assert updated.online is True
        assert updated.last_seen > time.time() - 5

    async def test_heartbeat_nonexistent(self, client):
        resp = await client.post("/api/device/heartbeat", json={"id": "nope"})
        assert resp.status_code == 404

    async def test_heartbeat_keeps_device_online(self, client, registry):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "temperature", "name": "T1", "ip": GATEWAY_IP,
        })
        registry.get_device(sid).last_seen = time.time() - 120

        registry.check_heartbeats(timeout=60)
        assert registry.get_device(sid).online is False

        await client.post("/api/device/heartbeat", json={"id": sid})
        assert registry.get_device(sid).online is True
        assert registry.get_device(sid).last_seen > time.time() - 5

    async def test_heartbeat_missing_id(self, client):
        resp = await client.post("/api/device/heartbeat", json={})
        assert resp.status_code == 400


@pytest.mark.asyncio
class TestGatewayCommands:
    async def test_poll_commands_empty(self, client):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "onoff", "name": "SW", "ip": GATEWAY_IP,
        })
        resp = await client.get(f"/api/device/commands?id={sid}")
        assert resp.status_code == 200
        assert resp.json()["commands"] == []

    async def test_add_and_poll_commands(self, client, registry):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "onoff", "name": "SW", "ip": GATEWAY_IP,
        })
        registry.add_command(sid, "onoff", "set_onoff", "1")
        registry.add_command(sid, "onoff", "set_onoff", "0")

        resp = await client.get(f"/api/device/commands?id={sid}")
        cmds = resp.json()["commands"]
        assert len(cmds) == 2
        assert cmds[0] == {"cluster": "onoff", "command": "set_onoff", "data": "1"}
        assert cmds[1] == {"cluster": "onoff", "command": "set_onoff", "data": "0"}

    async def test_commands_consumed_after_poll(self, client, registry):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "onoff", "name": "SW", "ip": GATEWAY_IP,
        })
        registry.add_command(sid, "onoff", "set_onoff", "1")
        await client.get(f"/api/device/commands?id={sid}")

        resp = await client.get(f"/api/device/commands?id={sid}")
        assert resp.json()["commands"] == []

    async def test_commands_from_ha_to_gateway_flow(self, client, registry):
        sid = gw_sensor_id(0)
        await client.post("/api/device/register", json={
            "id": sid, "type": "onoff", "name": "SW", "ip": GATEWAY_IP,
        })
        resp = await client.post("/api/device/commands", json={
            "id": sid,
            "commands": [
                {"cluster": "onoff", "command": "set_onoff", "data": "1"},
            ],
        })
        assert resp.status_code == 200
        assert resp.json()["added"] == 1

        resp = await client.get(f"/api/device/commands?id={sid}")
        assert len(resp.json()["commands"]) == 1

    async def test_commands_missing_id_get(self, client):
        resp = await client.get("/api/device/commands")
        assert resp.status_code == 400


@pytest.mark.asyncio
class TestGatewayFullLifecycle:
    async def test_full_lifecycle(self, client, registry):
        sid = gw_sensor_id(0)

        r1 = await client.post("/api/device/register", json={
            "id": sid, "type": "temperature", "name": "Living Room",
            "ip": GATEWAY_IP, "parent_id": GATEWAY_ID,
        })
        assert r1.status_code == 200

        r2 = await client.post("/api/device/state", json={
            "id": sid, "temperature": 22.5, "humidity": 60.0,
            "battery": 90, "rssi": -55,
        })
        assert r2.status_code == 200

        dev = registry.get_device(sid)
        assert dev.state["temperature"] == 22.5
        assert dev.state["humidity"] == 60.0
        assert dev.state["battery"] == 90

        r3 = await client.post("/api/device/heartbeat", json={"id": sid})
        assert r3.status_code == 200

        registry.add_command(sid, "onoff", "set_onoff", "1")
        r4 = await client.get(f"/api/device/commands?id={sid}")
        assert len(r4.json()["commands"]) == 1

        r5 = await client.post("/api/device/remove", json={"id": sid})
        assert r5.status_code == 200
        assert registry.get_device(sid) is None


@pytest.mark.asyncio
class TestGatewayConcurrentSensors:
    async def test_multiple_sensor_types_simultaneously(self, client):
        sensors = [
            ("temp1", "temperature", {"temperature": 25.0, "humidity": 60}),
            ("contact1", "contact", {"contact": 1, "tamper": 0}),
            ("motion1", "occupancy", {"occupancy": 0, "duration": 0}),
            ("gas1", "gas", {"gas_level": 500, "alarm": 0}),
            ("rain1", "rain", {"rain_level": 30, "rain_digital": 0}),
            ("tank1", "tanque", {"level_pct": 80, "distance_cm": 20}),
            ("dhtgas1", "dht_gas", {"temperature": 26.0, "humidity": 55, "gas_level": 100, "alarm": 0}),
        ]

        for sid, stype, state in sensors:
            await client.post("/api/device/register", json={
                "id": sid, "type": stype, "name": sid, "ip": GATEWAY_IP,
            })
            await client.post("/api/device/state", json={"id": sid, **state})

        resp = await client.get("/api/devices")
        devices = resp.json()
        assert len(devices) == len(sensors)

        for dev in devices:
            info = await client.get(f"/api/device/info?id={dev['id']}")
            data = info.json()
            assert data["online"] is True

    async def test_gateway_plus_sensors_count(self, client):
        await client.post("/api/device/register", json={
            "id": "gw_main", "type": "bridge", "name": "GW", "ip": GATEWAY_IP,
        })
        for i in range(10):
            await client.post("/api/device/register", json={
                "id": f"esp8266_gw_AA_sensor_{i}", "type": "temperature",
                "name": f"S{i}", "ip": GATEWAY_IP, "parent_id": "gw_main",
            })

        resp = await client.get("/api/gateway/info")
        assert resp.json()["total_devices"] == 11
