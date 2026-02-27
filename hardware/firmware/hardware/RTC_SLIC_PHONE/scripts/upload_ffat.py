"""PlatformIO custom target: create and flash FFat image for A252."""

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


def _is_true(value: str) -> bool:
    return (value or "").lower() in {"1", "true", "yes", "on"}


def _should_skip_tone_wavs(audio_dir: Path, path: Path, include_tone_wavs: bool) -> bool:
    if include_tone_wavs:
        return False

    relative_parts = path.relative_to(audio_dir).parts
    return len(relative_parts) >= 3 and tuple(relative_parts[:2]) == ("assets", "wav")


def _run(command):
    print("[upload_ffat] $ {}".format(" ".join(command)))
    result = subprocess.run(command, check=False)
    if result.returncode:
        raise RuntimeError(f"Command failed ({command[0]}): {result.returncode}")


def _find_tool(tool: str, project_dir: Path) -> str:
    path = shutil.which(tool)
    if path:
        return path

    platformio_home = Path.home() / ".platformio" / "packages"
    project_package = Path(os.environ.get("PIO_PACKAGES_DIR", str(project_dir / ".pio" / "packages")))
    if tool == "mkfatfs":
        candidates = [
            project_package / "tool-mkfatfs" / "mkfatfs",
            platformio_home / "tool-mkfatfs" / "mkfatfs",
        ]
    elif tool == "esptool.py":
        candidates = [
            project_package / "tool-esptoolpy" / "esptool.py",
            platformio_home / "tool-esptoolpy" / "esptool.py",
        ]
    else:
        candidates = [Path(tool)]

    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    raise RuntimeError(f"Unable to locate '{tool}'.")


def _find_partition(partitions_path: Path, part_name: str) -> tuple[int, int]:
    if not partitions_path.exists():
        raise FileNotFoundError(f"Partition table not found: {partitions_path}")

    for line in partitions_path.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        parts = [segment.strip() for segment in re.split(r",\s*", line) if segment.strip()]
        if len(parts) < 5:
            continue
        name, _part_type, _sub_type, offset, size = parts[:5]
        if name == part_name:
            return _parse_int(offset), _parse_int(size)

    raise RuntimeError(f"Could not find '{part_name}' partition in partition table.")


def _build_ffat_image(mkfatfs: str, webui_dir: Path, audio_dir: Path, image_path: Path, image_size: int) -> None:
    include_tone_wavs = _is_true(os.environ.get("A252_FFAT_INCLUDE_TONE_WAV", "0"))

    with tempfile.TemporaryDirectory(prefix="ffat_", dir=None) as staging_root:
        staging_dir = Path(staging_root)

        include_webui = os.environ.get("A252_FFAT_INCLUDE_WEBUI", "").lower() in {"1", "true", "yes", "on"}
        if include_webui and webui_dir.is_dir():
            shutil.copytree(webui_dir, staging_dir / "webui")

        if audio_dir.is_dir():
            # Keep same runtime paths for media playback.
            # By default, keep tone assets out of FFat (now code-synthesized) and
            # retain non-tone media only.
            # Set A252_FFAT_INCLUDE_TONE_WAV=1 to keep tone WAV assets.
            for source_file in audio_dir.rglob("*"):
                if not source_file.is_file():
                    continue
                if _should_skip_tone_wavs(audio_dir, source_file, include_tone_wavs):
                    continue
                relative = source_file.relative_to(audio_dir)
                target = staging_dir / relative
                target.parent.mkdir(parents=True, exist_ok=True);
                shutil.copy2(source_file, target)

        if image_path.exists():
            image_path.unlink()
        _run([mkfatfs, "-c", str(staging_dir), "-t", "fatfs", "-s", str(image_size), str(image_path)])


def _upload_image(
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


def _target_upload_ffat(source, target, env):
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

    partition_offset, partition_size = _find_partition(partitions_path, "ffat")
    webui_dir = project_dir / "data" / "webui"
    audio_dir = project_dir / "data" / "audio"
    image_path = build_dir / "ffat.bin"

    mkfatfs = _find_tool("mkfatfs", project_dir)
    _build_ffat_image(mkfatfs, webui_dir, audio_dir, image_path, partition_size)
    print(
        "[upload_ffat] built image "
        f"{image_path} ({image_path.stat().st_size} bytes, partition 0x{partition_offset:06x} / 0x{partition_size:06x})"
    )

    upload_port = (
        os.environ.get("PIO_UPLOAD_PORT")
        or os.environ.get("UPLOAD_PORT")
        or env.get("UPLOAD_PORT")
        or env.GetProjectOption("upload_port", None)
    )
    if not upload_port:
        print("[upload_ffat] skipped flash step (no upload port), image ready")
        return

    esptool = _find_tool("esptool.py", project_dir)
    python_exec = env.get("PYTHONEXE", shutil.which("python3") or "python3")

    board_upload = board.get("upload", {})
    board_build = board.get("build", {})

    flash_args = {
        "baud": str(board_upload.get("speed", env.get("UPLOAD_SPEED", "460800"))),
        "before": env.get("UPLOAD_BEFORE", "default_reset"),
        "after": env.get("UPLOAD_AFTER", "hard_reset"),
        "mode": env.get("FLASH_MODE", board_build.get("flash_mode", "dio")),
        "freq": _normalize_flash_freq(board_build.get("f_flash", env.get("F_FLASH", "80m"))),
        "size": board_upload.get("flash_size", env.get("FLASH_SIZE", "4MB")),
    }
    _upload_image(esptool, python_exec, upload_port, flash_args, partition_offset, image_path)
    print(f"[upload_ffat] flashed {image_path.name} at 0x{partition_offset:06x}")


Import("env")
if hasattr(env, "AddCustomTarget"):
    env.AddCustomTarget(
        name="upload_ffat",
        dependencies=[],
        actions=[_target_upload_ffat],
        title="Upload FFat",
        description="Build and flash FFat image for A252",
    )
