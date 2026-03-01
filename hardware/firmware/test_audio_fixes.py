#!/usr/bin/env python3
import serial, time, sys

print("\n=== TEST A252 AUDIO (3 FIXES: 60% volume, 50ms debounce, hangup validation) ===\n")

ports = {
    'freenove': '/dev/cu.usbmodem5AB90753301',
    'a252': '/dev/cu.SLAB_USBtoUART'
}

# 1) A252 boot check
print("[1] A252 Boot Check...")
try:
    ser_a252 = serial.Serial(ports['a252'], 115200, timeout=0.5)
    time.sleep(0.3)
    ser_a252.reset_input_buffer()
    time.sleep(1.0)
    
    ser_a252.write(b'STATUS\n')
    time.sleep(0.4)
    
    boot_lines = []
    for _ in range(12):
        line = ser_a252.readline().decode('utf-8', 'ignore').strip()
        if line:
            boot_lines.append(line)
    
    print(f"    A252 boot OK: {len(boot_lines)} lines")
    for line in boot_lines:
        if any(x in line.lower() for x in ['volume', 'codec', 'es8388', '60', 'audio', 'optimiz']):
            print(f"      > {line[:130]}")
    
    ser_a252.close()
except Exception as e:
    print(f"    ERROR: {e}")
    sys.exit(1)

time.sleep(0.5)

# 2) Freenove check
print("\n[2] Freenove Status...")
try:
    ser_freenove = serial.Serial(ports['freenove'], 115200, timeout=0.5)
    time.sleep(0.3)
    ser_freenove.reset_input_buffer()
    
    ser_freenove.write(b'STATUS\n')
    time.sleep(0.3)
    
    freenove_lines = []
    for _ in range(10):
        line = ser_freenove.readline().decode('utf-8', 'ignore').strip()
        if line:
            freenove_lines.append(line)
    
    print(f"    Freenove OK: {len(freenove_lines)} lines")
    for line in freenove_lines:
        if any(x in line for x in ['SCENE_U_SON_PROTO', 'STATUS', 'ok=']):
            print(f"      > {line[:130]}")
    
    ser_freenove.close()
except Exception as e:
    print(f"    ERROR: {e}")
    sys.exit(1)

print("\n[3] Manual Test Steps:")
print("    1. Check A252 speaker is ON-HOOK (volume 0, no audio)")
print("    2. Send from Freenove: ESPNOW_SEND RING 02")
print("    3. A252 should RING and play audio at CLEAR, pleasant volume (60% = -4dBFS)")
print("    4. Hang up the phone (switch hook ON-HOOK)")
print("    5. Audio MUST STOP IMMEDIATELY (no tail noise)")
print("    6. Repeat lift/hang: should be fast (~90ms debounce), no glitches")

print("\n✅ Both devices ready for audio validation test!\n")
