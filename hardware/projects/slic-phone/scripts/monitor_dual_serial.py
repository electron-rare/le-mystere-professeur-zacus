import serial
import threading
import time

# Ports à adapter selon votre configuration
PORTS = [
    ('/dev/cu.SLAB_USBtoUART', 'logs_esp32_audio_kit.txt'),
    ('/dev/cu.usbmodem5B5E0508431', 'logs_esp32s3.txt'),
]
BAUDRATE = 115200


def monitor_port(port, logfile):
    with serial.Serial(port, BAUDRATE, timeout=2) as ser, open(logfile, 'w') as log:
        print(f"[MONITOR] {port} -> {logfile}")
        while True:
            try:
                line = ser.readline().decode(errors='replace')
                if line:
                    print(f"[{port}] {line.strip()}")
                    log.write(line)
                    log.flush()
            except Exception as e:
                print(f"[ERROR] {port}: {e}")
                break

def main():
    threads = []
    for port, logfile in PORTS:
        t = threading.Thread(target=monitor_port, args=(port, logfile), daemon=True)
        t.start()
        threads.append(t)
    print("Capture des logs en cours. Ctrl+C pour arrêter.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Arrêt de la capture des logs.")

if __name__ == '__main__':
    main()
