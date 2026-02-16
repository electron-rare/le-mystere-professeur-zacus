#!/usr/bin/env python3
"""
Diagnostic script for UI link handshake and frame transmission.
This script:
1. Monitors both ESP32 and OLED serial ports
2. Tracks PING, HELLO, ACK, and STAT frame exchanges
3. Logs frame content to identify what's being sent
"""

import serial
import re
import sys
import time
from threading import Thread, Lock
from collections import defaultdict

class UiLinkDiagnostics:
    def __init__(self):
        self.running = True
        self.lock = Lock()
        self.stats = {
            "esp32": {"ping_tx": 0, "stat_tx": 0, "keyframe_tx": 0, "ping_fail": 0},
            "oled": {"hello_tx": 0, "pong_tx": 0, "hello_rx": 0},
        }
        self.frame_log = []
        
    def parse_esp32_line(self, line):
        """Extract UI link messages from ESP32 logs."""
        # Look for PING, STAT, KEYFRAME being sent
        if "[UI_LINK" in line:
            if "PING" in line:
                self.stats["esp32"]["ping_tx"] += 1
                return ("ESP32_PING", line)
            elif "STAT" in line:
                self.stats["esp32"]["stat_tx"] += 1
                # Try to extract ui_page value
                match = re.search(r'ui_page=(\d+)', line)
                if match:
                    return ("ESP32_STAT", f"ui_page={match.group(1)}")
            elif "KEYFRAME" in line:
                self.stats["esp32"]["keyframe_tx"] += 1
                
    def parse_oled_line(self, line):
        """Extract UI link messages from OLED logs."""
        if "HELLO" in line:
            self.stats["oled"]["hello_tx"] += 1
            return ("OLED_HELLO", line)
        elif "PONG" in line:
            self.stats["oled"]["pong_tx"] += 1
            return ("OLED_PONG", line)
            
    def monitor_esp32(self, ser):
        """Monitor ESP32 main board serial."""
        print("📡 [ESP32] Connected")
        while self.running:
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        timestamp = time.strftime("%H:%M:%S")
                        print(f"[{timestamp}] ESP32 → {line}")
                        self.parse_esp32_line(line)
            except Exception as e:
                if self.running:
                    print(f"ESP32 read error: {e}")
                break
            time.sleep(0.01)
                
    def monitor_oled(self, ser):
        """Monitor OLED UI board serial."""
        print("📡 [OLED]  Connected")
        while self.running:
            try:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        timestamp = time.strftime("%H:%M:%S")
                        print(f"[{timestamp}] OLED  → {line}")
                        self.parse_oled_line(line)
            except Exception as e:
                if self.running:
                    print(f"OLED read error: {e}")
                break
            time.sleep(0.01)
                
    def print_summary(self):
        """Print periodic summary."""
        while self.running:
            time.sleep(5)
            with self.lock:
                print("\n" + "="*60)
                print("UI LINK DIAGNOSTICS SUMMARY")
                print("="*60)
                print(f"ESP32 → PING sent: {self.stats['esp32']['ping_tx']}")
                print(f"ESP32 → STAT sent: {self.stats['esp32']['stat_tx']}")
                print(f"ESP32 → KEYFRAME sent: {self.stats['esp32']['keyframe_tx']}")
                print(f"OLED  → HELLO sent: {self.stats['oled']['hello_tx']}")
                print(f"OLED  → PONG sent: {self.stats['oled']['pong_tx']}")
                print("="*60 + "\n")
                
    def run(self):
        print("\n" + "="*60)
        print("UI LINK DIAGNOSTIC MONITOR")
        print("="*60)
        
        try:
            ser_esp32 = serial.Serial("/dev/cu.SLAB_USBtoUART7", 115200, timeout=1)
            ser_oled = serial.Serial("/dev/cu.SLAB_USBtoUART", 115200, timeout=1)
        except Exception as e:
            print(f"❌ Failed to open ports: {e}")
            return
            
        threads = [
            Thread(target=self.monitor_esp32, args=(ser_esp32,), daemon=True),
            Thread(target=self.monitor_oled, args=(ser_oled,), daemon=True),
            Thread(target=self.print_summary, daemon=True),
        ]
        
        for t in threads:
            t.start()
            
        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n✓ Shutting down...")
            self.running = False
            time.sleep(1)
        finally:
            ser_esp32.close()
            ser_oled.close()

if __name__ == "__main__":
    diag = UiLinkDiagnostics()
    diag.run()
