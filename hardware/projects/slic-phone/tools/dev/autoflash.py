#!/usr/bin/env python3
import argparse, json, os, platform, re, shutil, subprocess, sys, time
from datetime import datetime
from pathlib import Path

def which(cmd: str) -> str | None:
    return shutil.which(cmd)

def run(cmd, *, cwd=None, capture=False, check=True):
    if capture:
        return subprocess.check_output(cmd, cwd=cwd, text=True, stderr=subprocess.STDOUT)
    p = subprocess.run(cmd, cwd=cwd, text=True)
    if check and p.returncode != 0:
        raise SystemExit(p.returncode)
    return ""

def load_config(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)

def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)

def now_tag():
    return datetime.now().strftime("%Y%m%d_%H%M%S")

def pio_device_list_json():
    if not which("pio"):
        raise SystemExit("ERROR: 'pio' introuvable. Installe PlatformIO Core (ex: pipx install platformio).")
    out = run(["pio", "device", "list", "--json-output"], capture=True)
    return json.loads(out)

def normalize_ports_macos_prefer_cu(devs, prefer_cu: bool):
    if platform.system() != "Darwin" or not prefer_cu:
        return devs
    by_key = {}
    for d in devs:
        hwid = d.get("hwid","")
        desc = d.get("description","")
        port = d.get("port","")
        key = (hwid, desc)
        cur = by_key.get(key)
        if cur is None:
            by_key[key] = d
        else:
            if "/dev/cu." in port and "/dev/cu." not in cur.get("port",""):
                by_key[key] = d
    return list(by_key.values())

def match_serial_port(devs, pattern: str):
    rx = re.compile(pattern, re.I)
    matches = []
    for d in devs:
        blob = f"{d.get('port','')} {d.get('description','')} {d.get('hwid','')}"
        if rx.search(blob):
            matches.append(d)
    return matches

def mount_roots():
    sysname = platform.system()
    roots = []
    if sysname == "Darwin":
        roots.append(Path("/Volumes"))
    elif sysname == "Linux":
        user = os.environ.get("USER") or "user"
        roots += [Path("/media")/user, Path("/run/media")/user, Path("/mnt")]
    elif sysname == "Windows":
        import string
        for letter in string.ascii_uppercase:
            roots.append(Path(f"{letter}:/"))
    return [r for r in roots if r.exists()]

def find_uf2_volume(volume_match: str):
    rx = re.compile(volume_match, re.I)
    for root in mount_roots():
        try:
            for p in root.iterdir():
                name = p.name
                if p.is_dir() and rx.search(name):
                    return p
        except Exception:
            pass
    return None

def log_path(base_dir: Path, role: str):
    return base_dir / f"{now_tag()}_{role}"

def write_text(p: Path, s: str):
    p.write_text(s, encoding="utf-8")

def do_platformio(role_cfg: dict, port: str | None, logdir: Path):
    env = role_cfg["pio_env"]
    cmd = ["pio", "run", "-e", env, "-t", "upload"]
    if port:
        cmd += ["--upload-port", port]
    out = run(cmd, capture=True, check=True)
    write_text(logdir / "upload.log", out)
    return cmd

def do_arduino_cli(role_cfg: dict, port: str, logdir: Path):
    if not which("arduino-cli"):
        raise SystemExit("ERROR: 'arduino-cli' introuvable. Installe via brew/apt/choco.")
    fqbn = role_cfg["fqbn"]
    sketch = role_cfg["sketch_dir"]
    out1 = run(["arduino-cli", "compile", "--fqbn", fqbn, sketch], capture=True, check=True)
    out2 = run(["arduino-cli", "upload", "-p", port, "--fqbn", fqbn, sketch], capture=True, check=True)
    write_text(logdir / "compile.log", out1)
    write_text(logdir / "upload.log", out2)
    return ["arduino-cli ..."]

def do_uf2_copy(role_cfg: dict, logdir: Path):
    uf2_path = Path(role_cfg["uf2_path"])
    if not uf2_path.exists():
        raise SystemExit(f"ERROR: UF2 introuvable: {uf2_path}")
    vol = find_uf2_volume(role_cfg["volume_match"])
    if not vol:
        raise SystemExit("ERROR: volume UF2 non trouvé. Mets la carte RP2040 en BOOTSEL (disque RPI-RP2).")
    dst = vol / uf2_path.name
    shutil.copy2(uf2_path, dst)
    write_text(logdir / "uf2_copy.txt", f"Copied {uf2_path} -> {dst}\n")
    return ["uf2_copy", str(uf2_path), "->", str(dst)]

def pick_one_or_fail(items, what: str, hints: str):
    if len(items) == 1:
        return items[0]
    if len(items) == 0:
        raise SystemExit(f"ERROR: aucun {what} trouvé.\n{hints}")
    msg = [f"ERROR: plusieurs {what} possibles:"]
    for it in items:
        msg.append(f"- {it}")
    msg.append(hints)
    raise SystemExit("\n".join(msg))

def main():
    ap = argparse.ArgumentParser(description="Auto-detect + build + upload (PlatformIO / Arduino CLI / UF2)")
    ap.add_argument("cmd", choices=["list", "flash"])
    ap.add_argument("--config", default="tools/dev/flash_config.json")
    ap.add_argument("--role", help="Nom du rôle (ex: esp32, esp8266, rp2040)")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    cfg = load_config(Path(args.config))
    defaults = cfg.get("defaults", {})
    artifacts_dir = Path(defaults.get("artifacts_dir", "artifacts/flash"))
    prefer_cu = bool(defaults.get("prefer_cu_macos", True))

    roles = {r["role"]: r for r in cfg.get("roles", [])}

    if args.cmd == "list":
        devs = normalize_ports_macos_prefer_cu(pio_device_list_json(), prefer_cu)
        print("== Serial devices (pio device list) ==")
        for d in devs:
            print(f"- {d.get('port')} | {d.get('description')} | {d.get('hwid')}")
        print("\n== UF2 volumes ==")
        roots = mount_roots()
        found_any = False
        for root in roots:
            try:
                for p in root.iterdir():
                    if p.is_dir():
                        if any(x in p.name.upper() for x in ["RPI-RP2", "PICOBOOT", "CIRCUITPY"]):
                            print(f"- {p}")
                            found_any = True
            except Exception:
                pass
        if not found_any:
            print("- (none detected)")
        print("\n== Roles ==")
        for name, r in roles.items():
            print(f"- {name}: {r['method']}")
        return

    if args.cmd == "flash":
        if not args.role:
            raise SystemExit("ERROR: --role requis (ex: --role esp32).")
        if args.role not in roles:
            raise SystemExit(f"ERROR: rôle inconnu: {args.role} (connus: {', '.join(roles.keys())})")
        role_cfg = roles[args.role]
        method = role_cfg["method"]

        ensure_dir(artifacts_dir)
        logdir = log_path(artifacts_dir, args.role)
        ensure_dir(logdir)

        cmdline = None

        if method in ("platformio", "arduino_cli"):
            devs = normalize_ports_macos_prefer_cu(pio_device_list_json(), prefer_cu)
            matches = match_serial_port(devs, role_cfg.get("match", ".*"))
            pretty = [f"{m.get('port')} | {m.get('description')} | {m.get('hwid')}" for m in matches]
            port = None
            if role_cfg.get("need_serial_port", True):
                chosen = pick_one_or_fail(
                    pretty,
                    "port série",
                    "Astuce: débranche les autres cartes, ou précise un match plus strict (VID:PID / description / serial)."
                )
                port = chosen.split(" | ")[0].strip()

            if args.dry_run:
                print(f"[dry-run] role={args.role} method={method} port={port}")
                return

            if method == "platformio":
                cmdline = do_platformio(role_cfg, port, logdir)
            else:
                cmdline = do_arduino_cli(role_cfg, port, logdir)

        elif method == "uf2_copy":
            if args.dry_run:
                print(f"[dry-run] role={args.role} method=uf2_copy uf2={role_cfg.get('uf2_path')}")
                return
            cmdline = do_uf2_copy(role_cfg, logdir)

        else:
            raise SystemExit(f"ERROR: method non supportée: {method}")

        write_text(logdir / "meta.json", json.dumps({
            "role": args.role,
            "method": method,
            "cmdline": cmdline,
            "time": datetime.now().isoformat()
        }, indent=2))

        print(f"OK ✅  logs: {logdir}")

if __name__ == "__main__":
    main()
