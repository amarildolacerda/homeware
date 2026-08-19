#!/usr/bin/env python3
"""
Alexa Diagnostic Script for AgriSense IoT Lamp Node
Uses only Python stdlib (no pip required).
"""

import sys
import json
import time
import socket
import struct
import datetime
import urllib.request
import urllib.error

# Constants
SSDP_MULTICAST = "239.255.255.250"
SSDP_PORT = 1900
SSDP_TIMEOUT = 5
NODE_IP = "192.168.1.14"
NODE_PORT = 80


def http_get(url, timeout=5):
    """HTTP GET using urllib."""
    try:
        req = urllib.request.Request(url)
        resp = urllib.request.urlopen(req, timeout=timeout)
        return resp.status, resp.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except Exception as e:
        raise e


def http_put(url, data, timeout=5):
    """HTTP PUT using urllib."""
    try:
        body = json.dumps(data).encode()
        req = urllib.request.Request(url, data=body, method='PUT')
        req.add_header('Content-Type', 'application/json')
        resp = urllib.request.urlopen(req, timeout=timeout)
        return resp.status, resp.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()
    except Exception as e:
        raise e


def test_http_connection(ip, port):
    """Test basic HTTP connection to node."""
    result = {"test": "http_connection", "status": "fail", "details": {}}
    
    try:
        start = time.time()
        status, body = http_get(f"http://{ip}:{port}/")
        duration = int((time.time() - start) * 1000)
        
        result["status"] = "pass"
        result["details"] = {
            "status_code": status,
            "duration_ms": duration,
            "content_length": len(body),
            "has_alexa": "alexa" in body.lower() or "espalexa" in body.lower()
        }
        print(f"[OK] HTTP connection: {status} ({duration}ms)")
        
    except Exception as e:
        result["details"]["error"] = str(e)
        print(f"[FAIL] HTTP connection: {e}")
    
    return result


def test_api_lights(ip, port, user="agri"):
    """Test /api/lights endpoint."""
    result = {"test": "api_lights", "status": "fail", "details": {}}
    
    try:
        start = time.time()
        status, body = http_get(f"http://{ip}:{port}/api/lights")
        duration = int((time.time() - start) * 1000)
        
        result["details"]["status_code"] = status
        result["details"]["duration_ms"] = duration
        
        if status == 200:
            try:
                data = json.loads(body)
                result["details"]["response"] = data
                result["details"]["device_count"] = len(data)
                result["status"] = "pass"
                print(f"[OK] /api/lights: {len(data)} device(s)")
                for k, v in data.items():
                    print(f"    ID: {k}, Name: {v.get('name', '?')}, On: {v.get('state', {}).get('on', '?')}")
            except json.JSONDecodeError:
                result["details"]["error"] = "Invalid JSON"
                print(f"[FAIL] /api/lights: invalid JSON")
        else:
            result["details"]["error"] = f"HTTP {status}"
            print(f"[FAIL] /api/lights: HTTP {status}")
            
    except Exception as e:
        result["details"]["error"] = str(e)
        print(f"[FAIL] /api/lights: {e}")
    
    return result


def test_api_light_state(ip, port, light_id, user="agri"):
    """Test /api/{user}/lights/{id} endpoint."""
    result = {"test": "api_light_state", "status": "fail", "details": {}}
    
    try:
        start = time.time()
        status, body = http_get(f"http://{ip}:{port}/api/{user}/lights/{light_id}")
        duration = int((time.time() - start) * 1000)
        
        result["details"]["status_code"] = status
        result["details"]["duration_ms"] = duration
        result["details"]["light_id"] = light_id
        
        if status == 200:
            try:
                data = json.loads(body)
                result["details"]["response"] = data
                result["status"] = "pass"
                state = data.get("state", {})
                print(f"[OK] /api/{user}/lights/{light_id}: on={state.get('on', '?')}, bri={state.get('bri', '?')}")
            except json.JSONDecodeError:
                result["details"]["error"] = "Invalid JSON"
                print(f"[FAIL] /api/{user}/lights/{light_id}: invalid JSON")
        else:
            result["details"]["error"] = f"HTTP {status}"
            print(f"[FAIL] /api/{user}/lights/{light_id}: HTTP {status}")
            
    except Exception as e:
        result["details"]["error"] = str(e)
        print(f"[FAIL] /api/{user}/lights/{light_id}: {e}")
    
    return result


def test_ssdp_discovery(timeout=SSDP_TIMEOUT):
    """Test SSDP M-SEARCH discovery."""
    result = {"test": "ssdp_discovery", "status": "fail", "details": {}}
    
    msg = (
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 3\r\n"
        "ST: urn:schemas-upnp-org:device:Basic:1\r\n"
        "\r\n"
    )
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(timeout)
        
        # Enable multicast
        mreq = struct.pack("4sl", socket.inet_aton(SSDP_MULTICAST), socket.INADDR_ANY)
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
        
        sock.sendto(msg.encode(), (SSDP_MULTICAST, SSDP_PORT))
        
        responses = []
        start = time.time()
        
        while time.time() - start < timeout:
            try:
                data, addr = sock.recvfrom(4096)
                response = data.decode()
                responses.append({
                    "ip": addr[0],
                    "port": addr[1],
                    "has_lights": "/api/lights" in response
                })
                print(f"[OK] SSDP response from {addr[0]}:{addr[1]}")
            except socket.timeout:
                break
            except Exception as e:
                print(f"[WARN] SSDP parse error: {e}")
        
        sock.close()
        
        result["details"]["responses_count"] = len(responses)
        result["details"]["responses"] = responses
        
        if responses:
            result["status"] = "pass"
            print(f"[OK] SSDP discovery: {len(responses)} response(s)")
        else:
            result["details"]["error"] = "No SSDP responses"
            print(f"[FAIL] SSDP discovery: no responses in {timeout}s")
            
    except Exception as e:
        result["details"]["error"] = str(e)
        print(f"[FAIL] SSDP discovery: {e}")
    
    return result


def test_control(ip, port, light_id, user="agri"):
    """Test light control (on/off)."""
    result = {"test": "control", "status": "fail", "details": {}}
    
    try:
        # Get current state
        status, body = http_get(f"http://{ip}:{port}/api/{user}/lights/{light_id}")
        if status != 200:
            result["details"]["error"] = f"Failed to get state: HTTP {status}"
            return result
        
        data = json.loads(body)
        initial_on = data.get("state", {}).get("on", False)
        
        # Toggle state
        new_state = not initial_on
        status, _ = http_put(
            f"http://{ip}:{port}/api/{user}/lights/{light_id}/state",
            {"on": new_state}
        )
        
        if status != 200:
            result["details"]["error"] = f"Failed to set state: HTTP {status}"
            return result
        
        # Wait and verify
        time.sleep(0.5)
        status, body = http_get(f"http://{ip}:{port}/api/{user}/lights/{light_id}")
        if status != 200:
            result["details"]["error"] = f"Failed to verify state: HTTP {status}"
            return result
        
        verify_data = json.loads(body)
        actual_on = verify_data.get("state", {}).get("on", False)
        
        result["details"]["initial_state"] = initial_on
        result["details"]["requested_state"] = new_state
        result["details"]["actual_state"] = actual_on
        result["details"]["success"] = actual_on == new_state
        
        if actual_on == new_state:
            result["status"] = "pass"
            print(f"[OK] Control: {'ON' if new_state else 'OFF'} - verified")
        else:
            result["details"]["error"] = f"State mismatch: expected {new_state}, got {actual_on}"
            print(f"[FAIL] Control: expected {new_state}, got {actual_on}")
            
    except Exception as e:
        result["details"]["error"] = str(e)
        print(f"[FAIL] Control: {e}")
    
    return result


def run_diagnostics(ip):
    """Run all diagnostic tests."""
    print(f"\n{'='*60}")
    print(f"Alexa Diagnostic Report - Node: {ip}")
    print(f"Timestamp: {datetime.datetime.now().isoformat()}")
    print(f"{'='*60}\n")
    
    results = []
    
    # Test 1: HTTP Connection
    print("1. Testing HTTP connection...")
    results.append(test_http_connection(ip, NODE_PORT))
    print()
    
    # Test 2: API Lights
    print("2. Testing /api/lights endpoint...")
    lights_result = test_api_lights(ip, NODE_PORT)
    results.append(lights_result)
    print()
    
    # Get light ID from results
    light_id = None
    if lights_result["status"] == "pass" and lights_result["details"].get("response"):
        light_id = int(list(lights_result["details"]["response"].keys())[0])
        print(f"    Discovered light ID: {light_id}")
    
    # Test 3: Light State
    if light_id:
        print("3. Testing light state endpoint...")
        results.append(test_api_light_state(ip, NODE_PORT, light_id))
        print()
        
        # Test 4: Control
        print("4. Testing light control...")
        results.append(test_control(ip, NODE_PORT, light_id))
        print()
    else:
        print("3. Skipping light state test (no lights found)")
        print("4. Skipping control test (no lights found)")
        print()
    
    # Test 5: SSDP Discovery
    print("5. Testing SSDP discovery...")
    results.append(test_ssdp_discovery())
    print()
    
    return results


def print_summary(results):
    """Print diagnostic summary."""
    print(f"\n{'='*60}")
    print("DIAGNOSTIC SUMMARY")
    print(f"{'='*60}")
    
    passed = sum(1 for r in results if r["status"] == "pass")
    failed = sum(1 for r in results if r["status"] == "fail")
    
    print(f"Total tests: {len(results)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print()
    
    if failed:
        print("Failed tests:")
        for r in results:
            if r["status"] == "fail":
                error = r["details"].get("error", "Unknown error")
                print(f"  - {r['test']}: {error}")
    
    print(f"\n{'='*60}")


def save_results(results, output_file):
    """Save results to JSON file."""
    with open(output_file, 'w') as f:
        json.dump({
            "timestamp": datetime.datetime.now().isoformat(),
            "node_ip": NODE_IP,
            "results": results
        }, f, indent=2)
    print(f"\nResults saved to {output_file}")


def main():
    import argparse
    
    parser = argparse.ArgumentParser(description="Alexa Diagnostic Script")
    parser.add_argument("--ip", default=NODE_IP, help=f"Node IP (default: {NODE_IP})")
    parser.add_argument("--output", default="alexa_diag_results.json", help="Output JSON file")
    args = parser.parse_args()
    
    results = run_diagnostics(args.ip)
    print_summary(results)
    save_results(results, args.output)
    
    # Return exit code based on results
    failed = sum(1 for r in results if r["status"] == "fail")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
