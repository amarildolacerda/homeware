import socket
import sys
import os
import hashlib
import time

# espota protocol - based on framework-arduinoespressif32 espota.py serve()
# FLASH = 0
REMOTE_IP = "192.168.1.14"
REMOTE_PORT = 3232

firmware_path = ".pio/build/hub_32_ota/firmware.bin"
content_size = os.path.getsize(firmware_path)
with open(firmware_path, "rb") as f:
    file_md5 = hashlib.md5(f.read()).hexdigest()

def try_upload(local_port):
    # TCP listener
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", local_port))
    sock.listen(1)
    sock.settimeout(10)

    # UDP invitation
    message = "0 %d %d %s\n" % (local_port, content_size, file_md5)
    sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock2.settimeout(3)

    for attempt in range(8):
        try:
            sock2.sendto(message.encode(), (REMOTE_IP, REMOTE_PORT))
            data, addr = sock2.recvfrom(37)
            if data == b"OK":
                print(f"[+] invitation accepted (attempt {attempt+1})")
                break
        except socket.timeout:
            pass
        except Exception as e:
            print("UDP error:", e)
            sock2.close()
            sock.close()
            return False
    else:
        print("[-] no OK reply after 8 attempts")
        sock2.close()
        sock.close()
        return False
    sock2.close()

    print("[*] waiting for TCP connection...")
    try:
        connection, client_address = sock.accept()
    except socket.timeout:
        print("[-] TCP connect timeout")
        sock.close()
        return False
    connection.settimeout(30)
    print(f"[+] TCP connected from {client_address}")

    with open(firmware_path, "rb") as f:
        data = f.read()

    # send size
    connection.send(str(content_size).encode() + b"\n")
    # receive ack
    ack = connection.recv(8)
    print("[*] ack:", ack)

    # send firmware in chunks
    sent = 0
    offset = 0
    while offset < len(data):
        chunk = data[offset:offset+4096]
        connection.sendall(chunk)
        sent += len(chunk)
        offset += len(chunk)
        if sent % (content_size // 10 or 1) == 0 or offset >= len(data):
            print(f"[*] sent {offset}/{content_size} bytes")
    connection.close()
    sock.close()
    print("[+] upload done")
    return True

for lp in range(50100, 50100 + 5):
    print(f"=== trying local port {lp} ===")
    if try_upload(lp):
        break
    time.sleep(2)
