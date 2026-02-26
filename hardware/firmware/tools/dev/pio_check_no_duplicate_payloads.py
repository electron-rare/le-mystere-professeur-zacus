Import("env")

from pathlib import Path
import subprocess
import sys

from SCons.Script import COMMAND_LINE_TARGETS, Exit


def _requires_duplicate_guard() -> bool:
    targets = {str(target) for target in COMMAND_LINE_TARGETS}
    return any(target in targets for target in ("buildfs", "uploadfs"))


if _requires_duplicate_guard():
    project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
    checker = project_dir / "tools" / "dev" / "check_no_duplicate_payload_files.py"
    cmd = [sys.executable, str(checker), "--repo-root", str(project_dir), "--scope", "data"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stdout:
        print(result.stdout.strip())
    if result.stderr:
        print(result.stderr.strip())
    if result.returncode != 0:
        print("[guard] uploadfs blocked: duplicate payload files detected.")
        Exit(1)
