#!/bin/bash
set -e

# Test Freenove -> A252 RING delivery
echo "[TEST] Send RING from Freenove to A252..."
python3 << 'ENDPYTHON'
import serial
import time
import threading

freenove = '/dev/cu.usbmodem5AB90753301'
a252 = '/dev/cu.SLAB_USBtoUART'
results = {'freenove': [], 'a252': []}

def monitor_a252():
    try:
        ser = serial.Serial(a252, 115200, timeout=0.3)
        time.sleep(0.2)
        ser.reset_input_buffer()
        start = time.time()
        while time.time() - start < 4:
            try:
                line = ser.readline().decode('utf-8', 'ignore').strip()
                if any(x in line.upper() for x in ['RING', 'ESPNOW', 'RX']):
                    results['a252'].append(line[:100])
            except:
                pass
        ser.close()
    except:
        pass

def send_ring():
    try:
        time.sleep(0.2)
        ser = serial.Serial(freenove, 115200, timeout=0.5)
        time.sleep(0.1)
        ser.write(b'ESPNOW_ON\n')
        time.sleep(0.1)
        ser.read(1024)
        ser.write(b'ESPNOW_SEND RING\n')
        time.sleep(0.3)
        resp = ser.read(2048).decode('utf-8', 'ignore')
        for line in resp.splitlines():
            if 'ACK' in line or 'ok' in line:
                results['freenove'].append(line[:100])
        ser.close()
    except Exception as e:
        results['freenove'].append(f"ERROR: {e}")

t1 = threading.Thread(target=monitor_a252)
t2 = threading.Thread(target=send_ring)
t1.start()
t2.start()
t1.join()
t2.join()

print("=== FREENOVE ===")
for line in results['freenove']:
    print(f"  {line}")
print("\n=== A252 RX ===")
if results['a252']:
    for line in results['a252']:
        print(f"  {line}")
else:
    print("  (no RING activity detected)")
ENDPYTHON
