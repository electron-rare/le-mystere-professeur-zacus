"""PlatformIO custom target: create and flash FFat USB-MSC image."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path


def _parse_int(value: str) -> int:
    value = value.strip()
    if value.lower().startswith("0x"):
        return int(value, 16)
    return int(value)


def _normalize_flash_freq(value: str) -> str:
    if not value:
        return "80m"
    normalized = value.strip().lower()
    if normalized.endswith("l"):
        normalized = normalized[:-1]
    if normalized.isdigit():
        freq_hz = int(normalized)
        if freq_hz % 1_000_000 == 0:
            return f"{freq_hz // 1_000_000}m"
        return str(freq_hz)
    return normalized


def _run(command):
    print("[upload_usbmsc] $ {}".format(" ".join(command)))
    result = subprocess.run(command, check=False)
    if result.returncode:
        raise RuntimeError(f"Command failed ({command[0]}): {result.returncode}")


def _find_tool(tool: str, project_dir: Path, env) -> str:
    path = shutil.which(tool)
    if path:
        return path

    platformio_home = Path.home() / ".platformio" / "packages"
    project_package = Path(os.environ.get("PIO_PACKAGES_DIR", str(project_dir / ".pio" / "packages")))
    candidates = []

    if tool == "mkfatfs":
        candidates.extend(
            [
                project_package / "tool-mkfatfs" / "mkfatfs",
                platformio_home / "tool-mkfatfs" / "mkfatfs",
            ]
        )
    elif tool == "esptool.py":
        candidates.extend(
            [
                project_package / "tool-esptoolpy" / "esptool.py",
                platformio_home / "tool-esptoolpy" / "esptool.py",
            ]
        )
    else:
        candidates.append(Path(tool))

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)

    raise RuntimeError(f"Unable to locate '{tool}'. Install PlatformIO package dependencies first.")


def _find_usbmsc_partition(partitions_path: Path) -> tuple[int, int]:
    if not partitions_path.exists():
        raise FileNotFoundError(f"Partition table not found: {partitions_path}")

    for line in partitions_path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = [segment.strip() for segment in re.split(r",\s*", line) if segment.strip()]
        if len(parts) < 5:
            continue
        name, _part_type, _sub_type, offset, size = parts[:5]
        if name == "usbmsc":
            return _parse_int(offset), _parse_int(size)

    raise RuntimeError("Could not find 'usbmsc' partition in partition table.")


def _build_usbmsc_image(
    mkfatfs: str, webui_dir: Path, audio_dir: Path, image_path: Path, image_size: int
) -> None:
    with tempfile.TemporaryDirectory(prefix="usbmsc_", dir=None) as staging_root:
        staging_dir = Path(staging_root)

        if webui_dir.is_dir():
            shutil.copytree(webui_dir, staging_dir / "webui")
        else:
            raise RuntimeError(f"WebUI folder not found: {webui_dir}")

        if audio_dir.is_dir():
            for item in audio_dir.iterdir():
                target = staging_dir / item.name
                if item.is_dir():
                    if target.exists():
                        # Merge directories when both roots share a path.
                        for sub in item.rglob("*"):
                            rel = sub.relative_to(item)
                            if sub.is_dir():
                                (target / rel).mkdir(parents=True, exist_ok=True)
                            else:
                                target.parent.mkdir(parents=True, exist_ok=True)
                                shutil.copy2(sub, target / rel)
                    else:
                        shutil.copytree(item, target)
                elif item.is_file():
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(item, target)

        if image_path.exists():
            image_path.unlink()
        _run([mkfatfs, "-c", str(staging_dir), "-t", "fatfs", "-s", str(image_size), str(image_path)])


def _upload_usbmsc_image(
    esptool: str,
    python_exec: str,
    port: str,
    flash_args: dict,
    offset: int,
    image_path: Path,
) -> None:
    command = [
        python_exec,
        esptool,
        "--port",
        port,
        "--baud",
        flash_args["baud"],
        "--before",
        flash_args["before"],
        "--after",
        flash_args["after"],
        "write_flash",
        "-z",
        "--flash_mode",
        flash_args["mode"],
        "--flash_freq",
        flash_args["freq"],
        "--flash_size",
        flash_args["size"],
        hex(offset),
        str(image_path),
    ]
    _run(command)


def _target_upload_usbmsc(source, target, env):
    project_dir = Path(env.subst("${PROJECT_DIR}"))
    build_dir = Path(env.subst("${BUILD_DIR}"))
    build_dir.mkdir(parents=True, exist_ok=True)

    board = env.BoardConfig()
    partition_path_raw = env.GetProjectConfig().get("env:%s" % env["PIOENV"], "board_build.partitions")
    if not partition_path_raw:
        partition_path_raw = board.get("build", {}).get("partitions")
    if not partition_path_raw:
        raise RuntimeError("No partition table configured for this environment.")

    partitions_path = Path(partition_path_raw)
    if not partitions_path.is_absolute():
        partitions_path = project_dir / partitions_path

    partition_offset, partition_size = _find_usbmsc_partition(partitions_path)
    if partition_size <= 0:
        raise RuntimeError("USB-MSC partition has invalid size.")

    webui_dir = project_dir / "data" / "webui"
    audio_dir = project_dir / "data" / "audio"
    image_path = build_dir / "usbmsc.bin"

    mkfatfs = _find_tool("mkfatfs", project_dir, env)
    _build_usbmsc_image(mkfatfs, webui_dir, audio_dir, image_path, partition_size)
    print(
        "[upload_usbmsc] built image "
        f"{image_path} ({image_path.stat().st_size} bytes, partition 0x{partition_offset:06x} / 0x{partition_size:06x})"
    )

    dry_run = os.environ.get("USB_MSC_DRY_RUN", "").lower() in {"1", "true", "yes", "on"}
    if dry_run:
        print("[upload_usbmsc] dry run enabled, skipping flash step")
        return

    upload_port = (
        env.GetProjectOption("upload_port", None)
        or env.get("UPLOAD_PORT")
        or os.environ.get("PIO_UPLOAD_PORT")
        or os.environ.get("UPLOAD_PORT")
    )
    if not upload_port:
        print("[upload_usbmsc] skipped flash step (no upload port), image ready")
        return

    esptool = _find_tool("esptool.py", project_dir, env)
    python_exec = env.get("PYTHONEXE", shutil.which("python3") or "python3")

    board_upload = board.get("upload", {})
    board_build = board.get("build", {})

    flash_args = {
        "baud": str(board_upload.get("speed", env.get("UPLOAD_SPEED", "460800"))),
        "before": env.get("UPLOAD_BEFORE", "default_reset"),
        "after": env.get("UPLOAD_AFTER", "hard_reset"),
        "mode": env.get("FLASH_MODE", board_build.get("flash_mode", "dio")),
        "freq": _normalize_flash_freq(board_build.get("f_flash", env.get("F_FLASH", "80m"))),
        "size": board_upload.get("flash_size", env.get("FLASH_SIZE", "8MB")),
    }
    _upload_usbmsc_image(esptool, python_exec, upload_port, flash_args, partition_offset, image_path)
    print(f"[upload_usbmsc] flashed {image_path.name} at 0x{partition_offset:06x}")


def _target_upload_usbmsc_dryrun(source, target, env):
    previous = os.environ.get("USB_MSC_DRY_RUN")
    os.environ["USB_MSC_DRY_RUN"] = "1"
    try:
        _target_upload_usbmsc(source, target, env)
    finally:
        if previous is None:
            os.environ.pop("USB_MSC_DRY_RUN", None)
        else:
            os.environ["USB_MSC_DRY_RUN"] = previous


Import("env")
if hasattr(env, "AddCustomTarget"):
    env.AddCustomTarget(
        name="upload_usbmsc",
        dependencies=[],
        actions=[_target_upload_usbmsc],
        title="Upload USB-MSC",
        description="Build and flash FFat USB-MSC webui image",
    )
    env.AddCustomTarget(
        name="upload_usbmsc_dryrun",
        dependencies=[],
        actions=[_target_upload_usbmsc_dryrun],
        title="Upload USB-MSC (dry run)",
        description="Build FFat USB-MSC webui image without flashing",
    )
