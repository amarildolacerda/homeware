import socket, time, threading, subprocess, sys

s = socket.socket()
s.settimeout(10)
s.connect(('192.168.1.14', 23))

def read_loop():
    try:
        while True:
            data = s.recv(4096)
            if not data:
                break
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
    except Exception as e:
        pass

t = threading.Thread(target=read_loop, daemon=True)
t.start()

time.sleep(1)
# trigger the OTA upload via curl
print("\n=== TRIGGERING UPLOAD ===", flush=True)
subprocess.run(['curl.exe', '-sS', '-F', 'update=@.pio/build/hub_32_ota/firmware.bin',
                'http://192.168.1.14/update'], timeout=120)
print("\n=== UPLOAD DONE ===", flush=True)
time.sleep(3)
s.close()
