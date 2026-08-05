# Alexa Test Script Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Criar script Python para testar integração Alexa no node lamp (descoberta, controle, monitoramento, log)

**Architecture:** Script CLI único com modos interativo e automático. Usa SSDP multicast para descoberta e HTTP API para controle/monitoramento.

**Tech Stack:** Python 3.10+, requests, socket, json, time, datetime

## Global Constraints

- Python 3.10+ (usa `match/case` e type hints)
- Bibliotecas: requests, socket, json, time, datetime, argparse
- Network: acesso multicast 239.255.255.250:1900
- Output: test_results.json com timestamp

---

## File Structure

- **Create:** `nodes/lamp/test_alexa.py` - Script principal (750-900 linhas)
- **Modify:** Nenhum (script independente)
- **Test:** Execução manual + validação com node real

---

### Task 1: Estrutura base + Argumentos CLI

**Files:**
- Create: `nodes/lamp/test_alexa.py`

**Interfaces:**
- Consumes: N/A (primeira task)
- Produces: `parse_args()`, `main()`, menu interativo

- [ ] **Step 1: Criar estrutura do script com imports e constants**

```python
#!/usr/bin/env python3
"""
Alexa Device Test Script for AgriSense IoT
Tests SSDP discovery, API control, and state monitoring.
"""

import sys
import json
import time
import socket
import struct
import argparse
import datetime
from typing import Optional, Dict, List, Any
from dataclasses import dataclass, asdict

try:
    import requests
except ImportError:
    print("Error: 'requests' library required. Install with: pip install requests")
    sys.exit(1)

# Constants
SSDP_MULTICAST = "239.255.255.250"
SSDP_PORT = 1900
SSDP_TIMEOUT = 3  # seconds
DEFAULT_MONITOR_DURATION = 60
DEFAULT_POLL_INTERVAL = 2
DEFAULT_OUTPUT_FILE = "test_results.json"
API_LIGHTS_PATH = "/api/lights"
API_USER_LIGHTS_PATH = "/api/{user}/lights/{id}"
```

- [ ] **Step 2: Criar dataclass para resultados de teste**

```python
@dataclass
class TestResult:
    name: str
    status: str  # "pass", "fail", "error"
    duration_ms: int
    details: Dict[str, Any]
    error: Optional[str] = None

@dataclass
class TestSuite:
    timestamp: str
    node_ip: str
    tests: List[TestResult]
    summary: Dict[str, int]
```

- [ ] **Step 3: Criar funções de parsing de argumentos**

```python
def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Alexa Device Test Script for AgriSense IoT",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Interactive mode:  python test_alexa.py
  Auto discovery:    python test_alexa.py --ip 192.168.1.100 --discover
  Full test:         python test_alexa.py --ip 192.168.1.100 --discover --control --monitor
  Monitor 2 min:     python test_alexa.py --ip 192.168.1.100 --monitor --duration 120
        """
    )
    parser.add_argument("--ip", help="Node IP address (auto-discover if omitted)")
    parser.add_argument("--discover", action="store_true", help="Run SSDP discovery test")
    parser.add_argument("--control", action="store_true", help="Run control test (on/off)")
    parser.add_argument("--monitor", action="store_true", help="Run state monitoring")
    parser.add_argument("--duration", type=int, default=DEFAULT_MONITOR_DURATION,
                        help=f"Monitor duration in seconds (default: {DEFAULT_MONITOR_DURATION})")
    parser.add_argument("--interval", type=int, default=DEFAULT_POLL_INTERVAL,
                        help=f"Poll interval in seconds (default: {DEFAULT_POLL_INTERVAL})")
    parser.add_argument("--output", default=DEFAULT_OUTPUT_FILE,
                        help=f"Output JSON file (default: {DEFAULT_OUTPUT_FILE})")
    parser.add_argument("--user", default="agri", help="API user (default: 'agri')")
    return parser.parse_args()
```

- [ ] **Step 4: Criar menu interativo**

```python
def interactive_menu() -> argparse.Namespace:
    print("\n=== Alexa Device Test ===")
    print("1. Discover devices (SSDP)")
    print("2. Test control (on/off)")
    print("3. Monitor state")
    print("4. Run all tests")
    print("5. Exit")
    
    choice = input("\nChoice (1-5): ").strip()
    
    ip = input("Node IP (leave empty for auto-discover): ").strip() or None
    user = input("API user [agri]: ").strip() or "agri"
    
    args = argparse.Namespace(
        ip=ip,
        discover=choice in ("1", "4"),
        control=choice in ("2", "4"),
        monitor=choice in ("3", "4"),
        duration=60,
        interval=2,
        output=DEFAULT_OUTPUT_FILE,
        user=user
    )
    return args
```

- [ ] **Step 5: Criar main() com dispatch**

```python
def main() -> int:
    args = parse_args()
    
    # If no flags, use interactive mode
    if not any([args.discover, args.control, args.monitor]):
        args = interactive_menu()
    
    print(f"\n[INFO] Target: {args.ip or 'auto-discover'}")
    print(f"[INFO] Tests: discover={args.discover}, control={args.control}, monitor={args.monitor}")
    
    results: List[TestResult] = []
    
    try:
        if args.discover:
            print("\n--- Discovery Test ---")
            # Task 2 will implement
            pass
        
        if args.control:
            print("\n--- Control Test ---")
            # Task 3 will implement
            pass
        
        if args.monitor:
            print("\n--- Monitor Test ---")
            # Task 4 will implement
            pass
    
    except KeyboardInterrupt:
        print("\n[INFO] Test interrupted by user")
    except Exception as e:
        print(f"\n[ERROR] Unexpected error: {e}")
        return 1
    
    # Task 5 will implement save_results()
    return 0

if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 6: Testar execução básica**

Run: `python nodes/lamp/test_alexa.py --help`
Expected: Help message with all arguments

Run: `python nodes/lamp/test_alexa.py`
Expected: Interactive menu appears

- [ ] **Step 7: Commit**

```bash
git add nodes/lamp/test_alexa.py
git commit -m "feat(test): add alexa test script structure with CLI"
```

---

### Task 2: SSDP Discovery

**Files:**
- Modify: `nodes/lamp/test_alexa.py` (add discover functions)

**Interfaces:**
- Consumes: `SSDP_MULTICAST`, `SSDP_PORT`, `SSDP_TIMEOUT`
- Produces: `discover_devices() -> List[Dict]`, `AlexaDevice` dataclass

- [ ] **Step 1: Adicionar dataclass para device**

```python
@dataclass
class AlexaDevice:
    ip: str
    port: int
    location: str
    server: str
    usn: str
    lights: List[Dict] = None
```

- [ ] **Step 2: Implementar envio M-SEARCH**

```python
def ssdp_discover(timeout: int = SSDP_TIMEOUT) -> List[Dict[str, str]]:
    """Send SSDP M-SEARCH and collect responses."""
    msg = (
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:Basic:1\r\n"
        "\r\n"
    )
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(timeout)
    
    # Enable multicast
    mreq = struct.pack("4sl", socket.inet_addr(SSDP_MULTICAST), socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    
    sock.sendto(msg.encode(), (SSDP_MULTICAST, SSDP_PORT))
    
    responses = []
    start = time.time()
    
    while time.time() - start < timeout:
        try:
            data, addr = sock.recvfrom(4096)
            response = parse_ssdp_response(data.decode())
            response['ip'] = addr[0]
            response['port'] = addr[1]
            responses.append(response)
        except socket.timeout:
            break
        except Exception as e:
            print(f"[WARN] SSDP parse error: {e}")
    
    sock.close()
    return responses

def parse_ssdp_response(data: str) -> Dict[str, str]:
    """Parse SSDP response headers."""
    headers = {}
    for line in data.split('\r\n'):
        if ':' in line:
            key, value = line.split(':', 1)
            headers[key.strip().upper()] = value.strip()
    return headers
```

- [ ] **Step 3: Implementar descoberta de lights via API**

```python
def discover_lights(ip: str, port: int = 80, user: str = "agri") -> List[Dict]:
    """GET /api/lights to discover Alexa devices."""
    url = f"http://{ip}:{port}/api/lights"
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        print(f"[ERROR] Failed to discover lights at {ip}: {e}")
        return []

def get_light_state(ip: str, port: int, light_id: int, user: str = "agri") -> Optional[Dict]:
    """GET /api/{user}/lights/{id} to get current state."""
    url = f"http://{ip}:{port}/api/{user}/lights/{light_id}"
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        print(f"[ERROR] Failed to get state for light {light_id}: {e}")
        return None
```

- [ ] **Step 4: Implementar discover_devices() principal**

```python
def discover_devices(ip: Optional[str] = None, user: str = "agri") -> List[AlexaDevice]:
    """Discover Alexa devices via SSDP or direct IP."""
    devices = []
    
    if ip:
        # Direct IP mode
        print(f"[INFO] Probing {ip}...")
        lights = discover_lights(ip, user=user)
        if lights:
            device = AlexaDevice(
                ip=ip,
                port=80,
                location=f"http://{ip}",
                server="direct",
                usn="",
                lights=lights
            )
            devices.append(device)
            print(f"[OK] Found {len(lights)} light(s)")
        else:
            print(f"[WARN] No lights found at {ip}")
    else:
        # SSDP discovery
        print(f"[INFO] Sending M-SEARCH to {SSDP_MULTICAST}:{SSDP_PORT}...")
        responses = ssdp_discover()
        print(f"[INFO] Got {len(responses)} SSDP response(s)")
        
        seen_ips = set()
        for resp in responses:
            ip_addr = resp.get('ip', '')
            if ip_addr in seen_ips:
                continue
            seen_ips.add(ip_addr)
            
            location = resp.get('LOCATION', f"http://{ip_addr}")
            lights = discover_lights(ip_addr, user=user)
            if lights:
                device = AlexaDevice(
                    ip=ip_addr,
                    port=80,
                    location=location,
                    server=resp.get('SERVER', 'unknown'),
                    usn=resp.get('USN', ''),
                    lights=lights
                )
                devices.append(device)
                print(f"[OK] {ip_addr}: {len(lights)} light(s)")
    
    return devices
```

- [ ] **Step 5: Integrar no main()**

Adicionar em `main()`:
```python
if args.discover:
    print("\n--- Discovery Test ---")
    start = time.time()
    devices = discover_devices(args.ip, user=args.user)
    duration = int((time.time() - start) * 1000)
    
    result = TestResult(
        name="discovery",
        status="pass" if devices else "fail",
        duration_ms=duration,
        details={"devices_found": len(devices), "devices": [asdict(d) for d in devices]}
    )
    results.append(result)
    print(f"[RESULT] Discovery: {result.status} ({duration}ms)")
```

- [ ] **Step 6: Testar discovery com IP direto**

Run: `python nodes/lamp/test_alexa.py --ip <NODE_IP> --discover`
Expected: Lista de lights encontradas

- [ ] **Step 7: Commit**

```bash
git add nodes/lamp/test_alexa.py
git commit -m "feat(test): add SSDP discovery and API light discovery"
```

---

### Task 3: Control Test

**Files:**
- Modify: `nodes/lamp/test_alexa.py` (add control functions)

**Interfaces:**
- Consumes: `AlexaDevice`, `get_light_state()`
- Produces: `test_control()`, `set_light_state()`

- [ ] **Step 1: Implementar set_light_state()**

```python
def set_light_state(ip: str, port: int, light_id: int, state: str, user: str = "agri") -> bool:
    """PUT /api/{user}/lights/{id} to set state (ON/OFF)."""
    url = f"http://{ip}:{port}/api/{user}/lights/{light_id}"
    payload = {"state": state}
    try:
        resp = requests.put(url, json=payload, timeout=5)
        resp.raise_for_status()
        return True
    except Exception as e:
        print(f"[ERROR] Failed to set light {light_id} to {state}: {e}")
        return False
```

- [ ] **Step 2: Implementar teste de controle completo**

```python
def test_control(device: AlexaDevice, user: str = "agri") -> List[TestResult]:
    """Test on/off control for all lights on device."""
    results = []
    
    if not device.lights:
        print(f"[WARN] No lights to test on {device.ip}")
        return results
    
    for light in device.lights:
        light_id = light.get('id')
        name = light.get('name', f'Light {light_id}')
        
        print(f"\n[TEST] Testing light {light_id}: {name}")
        
        # Test ON
        start = time.time()
        success = set_light_state(device.ip, device.port, light_id, "ON", user)
        time.sleep(1)
        
        state = get_light_state(device.ip, device.port, light_id, user)
        on_ok = success and state and state.get('state') == 'ON'
        duration = int((time.time() - start) * 1000)
        
        results.append(TestResult(
            name=f"control_{light_id}_on",
            status="pass" if on_ok else "fail",
            duration_ms=duration,
            details={"light_id": light_id, "command": "ON", "expected": "ON", 
                     "actual": state.get('state') if state else None}
        ))
        print(f"  ON: {'PASS' if on_ok else 'FAIL'}")
        
        # Test OFF
        start = time.time()
        success = set_light_state(device.ip, device.port, light_id, "OFF", user)
        time.sleep(1)
        
        state = get_light_state(device.ip, device.port, light_id, user)
        off_ok = success and state and state.get('state') == 'OFF'
        duration = int((time.time() - start) * 1000)
        
        results.append(TestResult(
            name=f"control_{light_id}_off",
            status="pass" if off_ok else "fail",
            duration_ms=duration,
            details={"light_id": light_id, "command": "OFF", "expected": "OFF",
                     "actual": state.get('state') if state else None}
        ))
        print(f"  OFF: {'PASS' if off_ok else 'FAIL'}")
    
    return results
```

- [ ] **Step 3: Integrar no main()**

Adicionar em `main()`:
```python
if args.control:
    print("\n--- Control Test ---")
    if not devices:
        devices = discover_devices(args.ip, user=args.user)
    
    for device in devices:
        control_results = test_control(device, user=args.user)
        results.extend(control_results)
```

- [ ] **Step 4: Testar controle com node real**

Run: `python nodes/lamp/test_alexa.py --ip <NODE_IP> --control`
Expected: Tests ON/OFF para cada light, com pass/fail

- [ ] **Step 5: Commit**

```bash
git add nodes/lamp/test_alexa.py
git commit -m "feat(test): add on/off control test with state verification"
```

---

### Task 4: State Monitor

**Files:**
- Modify: `nodes/lamp/test_alexa.py` (add monitor functions)

**Interfaces:**
- Consumes: `AlexaDevice`, `get_light_state()`
- Produces: `monitor_state()`, `StateChange` dataclass

- [ ] **Step 1: Adicionar dataclass para mudanças de estado**

```python
@dataclass
class StateChange:
    timestamp: str
    light_id: int
    old_state: Optional[str]
    new_state: str
    delta_ms: int
```

- [ ] **Step 2: Implementar monitor_state()**

```python
def monitor_state(
    device: AlexaDevice,
    duration: int = DEFAULT_MONITOR_DURATION,
    interval: int = DEFAULT_POLL_INTERVAL,
    user: str = "agri"
) -> List[StateChange]:
    """Monitor state changes for specified duration."""
    changes = []
    prev_states: Dict[int, str] = {}
    start = time.time()
    
    print(f"[INFO] Monitoring {device.ip} for {duration}s (interval: {interval}s)")
    print("[INFO] Press Ctrl+C to stop early")
    
    try:
        while time.time() - start < duration:
            for light in (device.lights or []):
                light_id = light.get('id')
                state = get_light_state(device.ip, device.port, light_id, user)
                
                if state:
                    current = state.get('state')
                    old = prev_states.get(light_id)
                    
                    if old is not None and current != old:
                        change = StateChange(
                            timestamp=datetime.datetime.now().isoformat(),
                            light_id=light_id,
                            old_state=old,
                            new_state=current,
                            delta_ms=int((time.time() - start) * 1000)
                        )
                        changes.append(change)
                        print(f"[CHANGE] Light {light_id}: {old} -> {current}")
                    
                    prev_states[light_id] = current
            
            time.sleep(interval)
    
    except KeyboardInterrupt:
        print("\n[INFO] Monitor stopped by user")
    
    return changes
```

- [ ] **Step 3: Integrar no main()**

Adicionar em `main()`:
```python
if args.monitor:
    print("\n--- Monitor Test ---")
    if not devices:
        devices = discover_devices(args.ip, user=args.user)
    
    monitor_results = []
    for device in devices:
        changes = monitor_state(device, args.duration, args.interval, user=args.user)
        monitor_results.extend([asdict(c) for c in changes])
    
    result = TestResult(
        name="monitor",
        status="pass",
        duration_ms=args.duration * 1000,
        details={"changes": monitor_results, "total_changes": len(monitor_results)}
    )
    results.append(result)
```

- [ ] **Step 4: Testar monitoramento**

Run: `python nodes/lamp/test_alexa.py --ip <NODE_IP> --monitor --duration 30`
Expected: Polling a cada 2s, detecta mudanças manuais

- [ ] **Step 5: Commit**

```bash
git add nodes/lamp/test_alexa.py
git commit -m "feat(test): add state monitoring with delta detection"
```

---

### Task 5: Results Output + Summary

**Files:**
- Modify: `nodes/lamp/test_alexa.py` (add save/load functions)

**Interfaces:**
- Consumes: `TestResult`, `TestSuite`
- Produces: `save_results()`, `print_summary()`

- [ ] **Step 1: Implementar save_results()**

```python
def save_results(results: List[TestResult], output_file: str, node_ip: str) -> None:
    """Save test results to JSON file."""
    passed = sum(1 for r in results if r.status == "pass")
    failed = sum(1 for r in results if r.status == "fail")
    errors = sum(1 for r in results if r.status == "error")
    
    suite = TestSuite(
        timestamp=datetime.datetime.now().isoformat(),
        node_ip=node_ip or "auto-discovered",
        tests=results,
        summary={"total": len(results), "passed": passed, "failed": failed, "errors": errors}
    )
    
    with open(output_file, 'w') as f:
        json.dump(asdict(suite), f, indent=2)
    
    print(f"\n[INFO] Results saved to {output_file}")
```

- [ ] **Step 2: Implementar print_summary()**

```python
def print_summary(results: List[TestResult]) -> None:
    """Print test summary to console."""
    passed = sum(1 for r in results if r.status == "pass")
    failed = sum(1 for r in results if r.status == "fail")
    errors = sum(1 for r in results if r.status == "error")
    total = len(results)
    
    print("\n" + "="*50)
    print("TEST SUMMARY")
    print("="*50)
    print(f"Total:  {total}")
    print(f"Passed: {passed} ✓")
    print(f"Failed: {failed} ✗")
    print(f"Errors: {errors} ⚠")
    print("="*50)
    
    if failed or errors:
        print("\nFailed tests:")
        for r in results:
            if r.status != "pass":
                print(f"  - {r.name}: {r.status}")
                if r.error:
                    print(f"    Error: {r.error}")
```

- [ ] **Step 3: Atualizar main() com output final**

```python
def main() -> int:
    # ... (existing code) ...
    
    # Final output
    if results:
        print_summary(results)
        save_results(results, args.output, args.ip or "auto")
    
    return 0
```

- [ ] **Step 4: Testar output completo**

Run: `python nodes/lamp/test_alexa.py --ip <NODE_IP> --discover --control`
Expected: JSON file gerado + resumo no console

- [ ] **Step 5: Commit final**

```bash
git add nodes/lamp/test_alexa.py
git commit -m "feat(test): add JSON results output and summary"
```

---

## Testing Checklist

- [ ] Script roda sem erros com `--help`
- [ ] Menu interativo funciona (sem args)
- [ ] SSDP discovery encontra devices na rede
- [ ] Direct IP discovery funciona
- [ ] Control test liga/desliga lights corretamente
- [ ] Monitor detecta mudanças de estado
- [ ] JSON output é válido e contém todos os campos
- [ ] Ctrl+C interrompe graciosamente

## Future Enhancements (Não incluído neste plano)

- Retry automático em falhas de rede
- Teste de performance (latência média)
- Modo verbose para debug
- Export CSV para análise
