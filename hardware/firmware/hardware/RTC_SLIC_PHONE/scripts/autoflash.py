#!/usr/bin/env python3
"""
Orchestrateur universel d'upload PlatformIO/Arduino/STM32/RP2040
- Mapping rôle → regex signature (VID:PID, description, serial, etc)
- Détection auto via pio device list --json-output
- Support RP2040 (UF2), STM32 (st-link), etc.
- Force release du port série (lsof/kill) avant upload
"""
import argparse, json, os, re, subprocess, sys, time

# --- Mapping rôle → regex signature ---
ROLE_MAP = {
    "esp32_audio": r"10c4:ea60|SLAB_USBtoUART|CP210",
    "esp8266_oled": r"1a86:7523|wchusbserial|CH340",
    "s3": r"esp32s3|S3|usbmodem|USB JTAG",
    "rp2040": r"RPI-RP2|RP2040|2e8a:"
    # Ajoute d'autres rôles ici
}

# --- Utilitaires shell ---
def sh(*cmd):
    return subprocess.check_output(cmd, text=True)

def pio_devices():
    raw = sh("pio", "device", "list", "--json-output")
    return json.loads(raw)

def pick_port(devs, pattern):
    rx = re.compile(pattern, re.I)
    for d in devs:
        blob = f"{d.get('port','')} {d.get('description','')} {d.get('hwid','')} {d.get('serial_number','')}"
        if rx.search(blob):
            return d["port"]
    return None

def force_release_port(port):
    # Tuer tout process tenant le port ouvert (macOS/Linux)
    try:
        out = sh("lsof", port)
        for line in out.splitlines()[1:]:
            pid = int(line.split()[1])
            print(f"[FORCE RELEASE] kill -9 {pid} sur {port}")
            os.kill(pid, 9)
        time.sleep(2)
    except subprocess.CalledProcessError:
        pass  # Rien ne tient le port
    except Exception as e:
        print(f"[WARN] force_release_port: {e}")

def upload_pio(env, port, target="upload"):
    force_release_port(port)
    print(f"[UPLOAD] PlatformIO env={env} port={port}")
    subprocess.check_call(["pio", "run", "-e", env, "-t", target, "--upload-port", port])

def upload_rp2040(build_dir=".pio/build/rp2040/firmware.uf2"):
    # Cherche le disque UF2
    for vol in os.listdir("/Volumes"):
        if "RPI-RP2" in vol or "RP2040" in vol:
            dest = f"/Volumes/{vol}/firmware.uf2"
            print(f"[UPLOAD] Copie UF2 vers {dest}")
            subprocess.check_call(["cp", build_dir, dest])
            return
    print("[ERROR] Aucun disque UF2 RP2040 détecté.")
    sys.exit(3)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--role", required=True, help="Nom logique du device (clé du mapping)")
    ap.add_argument("--env", help="Nom de l'env PlatformIO (si différent du rôle)")
    ap.add_argument("--target", default="upload", help="Target PlatformIO (upload, program, etc)")
    ap.add_argument("--build-dir", default=".pio/build/rp2040/firmware.uf2", help="Chemin UF2 pour RP2040")
    args = ap.parse_args()

    role = args.role
    env = args.env or role
    pattern = ROLE_MAP.get(role)
    if not pattern:
        print(f"[ERROR] Rôle inconnu : {role}")
        sys.exit(1)

    if role.startswith("rp2040"):
        upload_rp2040(args.build_dir)
        return

    devs = pio_devices()
    port = pick_port(devs, pattern)
    if not port:
        print("[ERROR] Aucun port ne matche. Devices vus:", file=sys.stderr)
        for d in devs:
            print(f"- {d.get('port')} | {d.get('description')} | {d.get('hwid')} | {d.get('serial_number')}", file=sys.stderr)
        sys.exit(2)

    upload_pio(env, port, args.target)

if __name__ == "__main__":
    main()
