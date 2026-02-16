#!/usr/bin/env python3
"""Dual-port serial monitor for debugging UI link communication."""

import serial
import sys
import time
from threading import Thread
from collections import deque

class DualMonitor:
    def __init__(self, port1, port2, baud=115200):
        self.port1 = port1
        self.port2 = port2
        self.baud = baud
        self.ser1 = None
        self.ser2 = None
        self.running = True
        self.buffer1 = deque(maxlen=100)
        self.buffer2 = deque(maxlen=100)
        
    def open_ports(self):
        try:
            self.ser1 = serial.Serial(self.port1, self.baud, timeout=1)
            print(f"✓ Connected to {self.port1}")
        except Exception as e:
            print(f"✗ Failed to open {self.port1}: {e}")
            self.ser1 = None
            
        try:
            self.ser2 = serial.Serial(self.port2, self.baud, timeout=1)
            print(f"✓ Connected to {self.port2}")
        except Exception as e:
            print(f"✗ Failed to open {self.port2}: {e}")
            self.ser2 = None
    
    def monitor_port(self, ser, port_label, is_port1):
        """Read from serial port continuously."""
        if not ser:
            return
            
        while self.running:
            try:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
                    for line in data.split('\n'):
                        if line.strip():
                            timestamp = time.strftime("%H:%M:%S")
                            print(f"[{timestamp}] {port_label} | {line}")
                            if is_port1:
                                self.buffer1.append(f"{timestamp} {line}")
                            else:
                                self.buffer2.append(f"{timestamp} {line}")
            except Exception as e:
                print(f"Error reading {port_label}: {e}")
                break
                
            time.sleep(0.01)
    
    def run(self):
        print("\n" + "="*80)
        print("DUAL PORT MONITOR - Press Ctrl+C to exit")
        print("="*80)
        print()
        
        self.open_ports()
        if not self.ser1 and not self.ser2:
            print("✗ No ports available!")
            return
        
        try:
            threads = []
            if self.ser1:
                t1 = Thread(target=self.monitor_port, args=(self.ser1, "ESP32_MAIN", True), daemon=True)
                threads.append(t1)
                t1.start()
            
            if self.ser2:
                t2 = Thread(target=self.monitor_port, args=(self.ser2, "OLED_UI  ", False), daemon=True)
                threads.append(t2)
                t2.start()
            
            # Keep main thread alive
            while self.running:
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\n\n✓ Shutting down...")
            self.running = False
            time.sleep(0.5)
        finally:
            if self.ser1:
                self.ser1.close()
            if self.ser2:
                self.ser2.close()

if __name__ == "__main__":
    monitor = DualMonitor(
        port1="/dev/cu.SLAB_USBtoUART7",  # ESP32 main
        port2="/dev/cu.SLAB_USBtoUART"    # OLED UI
    )
    monitor.run()
