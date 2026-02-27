#!/usr/bin/env python3
"""
Diagnostic interactif des ports série ESP32/ESP32-S3 (sans commande ID)
- Tente un reset auto bootloader (DTR/RTS)
- Utilise esptool pour identifier le chip
- Affiche le résultat détaillé pour chaque port
"""
import glob
import time
import sys

PORT_PATTERNS = [
    '/dev/tty.usbserial*', '/dev/cu.usbserial*', '/dev/ttyUSB*', '/dev/ttyACM*',
    '/dev/cu.wchusbserial*', '/dev/cu.SLAB_USBtoUART*', '/dev/cu.usbmodem*', '/dev/tty.usbmodem*',
]

def detect_serial_ports():
    ports = []
    for pat in PORT_PATTERNS:
        ports.extend(glob.glob(pat))
    return ports

def reset_to_bootloader(port):
    try:
        import serial
        with serial.Serial(port, baudrate=115200) as ser:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            ser.dtr = True
            ser.rts = False
            time.sleep(0.1)
            ser.dtr = False
            ser.rts = False
            time.sleep(0.1)
    except Exception as e:
        print(f"  [WARN] Reset bootloader échoué: {e}")

def try_esptool(port):
    try:
        import esptool
        # 1. esptool >=3.3: detect_chip
        if hasattr(esptool.ESPLoader, 'detect_chip'):
            chip = esptool.ESPLoader.detect_chip(port=port, baud=115200)
            desc = chip.get_chip_description()
            mac = chip.read_mac()
            return f"OK: {desc}, MAC: {':'.join(f'{b:02X}' for b in mac)}"
        # 2. esptool >=4.0: get_default_connected_device (API très changeante)
        elif hasattr(esptool, 'get_default_connected_device'):
            # Version qui exige port, serial_list, connect_attempts, initial_baud
            dev = esptool.get_default_connected_device(
                port=port, serial_list=[port], connect_attempts=1, initial_baud=115200
            )
            desc = dev.get_chip_description()
            mac = dev.read_mac()
            return f"OK: {desc}, MAC: {':'.join(f'{b:02X}' for b in mac)}"
        # 3. Fallback: instanciation manuelle (pour anciennes versions)
        else:
            # Peut échouer si API trop ancienne
            loader = esptool.ESPLoader
            with open(port, 'rb+') as ser:
                esp = loader(ser, False)
                esp.connect()
                desc = esp.get_chip_description()
                mac = esp.read_mac()
                return f"OK: {desc}, MAC: {':'.join(f'{b:02X}' for b in mac)}"
    except Exception as e:
        return f"ECHEC: {e}"

def main():
    ports = detect_serial_ports()
    if not ports:
        print("Aucun port série détecté.")
        sys.exit(1)
    print(f"Ports détectés: {ports}")
    for port in ports:
        print(f"\nTest du port {port} ...")
        reset_to_bootloader(port)
        time.sleep(0.2)
        result = try_esptool(port)
        print(f"  Résultat: {result}")

if __name__ == "__main__":
    main()
