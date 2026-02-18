#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."  # => hardware/firmware

if [ ! -d .venv ]; then
  python3 -m venv .venv
fi

# shellcheck disable=SC1091
source .venv/bin/activate

python3 -m pip install -U pip
python3 -m pip install -U pyserial PyYAML yamale Jinja2
python3 -m pip install -e lib/zacus_story_gen_ai

cat <<'EOF'

[OK] venv ready.
- serial tooling: pyserial
- story generation tooling: PyYAML + yamale + Jinja2 + zacus_story_gen_ai

Optional (recommended once) to warm PlatformIO caches:
  export PLATFORMIO_CORE_DIR="$HOME/.platformio"
  pio platform install espressif32

Then:
  ./build_all.sh
  python3 tools/dev/serial_smoke.py --role auto --baud 19200 --wait-port 3 --allow-no-hardware
  ZACUS_SKIP_PIO=1 ./tools/dev/run_matrix_and_smoke.sh

EOF
