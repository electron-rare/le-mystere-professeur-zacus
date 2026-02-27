Import("env")

from pathlib import Path

project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
build_dir = Path(env.subst("$BUILD_DIR")).resolve()

# Ensure SCons can always open .sconsign under the env build directory.
build_dir.mkdir(parents=True, exist_ok=True)
(project_dir / ".pio" / "build").mkdir(parents=True, exist_ok=True)
