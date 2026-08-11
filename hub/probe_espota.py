import socket
import hashlib
import os

# Replicate espota.py serve() exactly:
# sends '%d %d %d %s\n' % (command, localPort, content_size, file_md5) over UDP to port 3232
# waits for "OK" reply

REMOTE_IP = "192.168.1.14"
REMOTE_PORT = 3232
LOCAL_PORT = 55468  # must match what we listen on? No - ESP replies to sender addr

firmware_path = ".pio/build/hub_32_ota/firmware.bin"
content_size = os.path.getsize(firmware_path)
with open(firmware_path, "rb") as f:
    file_md5 = hashlib.md5(f.read()).hexdigest()

command = 0  # FLASH
message = "%d %d %d %s\n" % (command, LOCAL_PORT, content_size, file_md5)
print("Message:", message.strip())

sock2 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock2.settimeout(5)
for attempt in range(5):
    try:
        sock2.sendto(message.encode(), (REMOTE_IP, REMOTE_PORT))
        data, addr = sock2.recvfrom(37)
        print(f"Reply from {addr}: {data!r}")
        break
    except socket.timeout:
        print(f"  attempt {attempt+1}: timeout")
    except Exception as e:
        print(f"  attempt {attempt+1}: {e}")
sock2.close()
