#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."  # => hardware/firmware

if [ ! -d .venv ]; then
  python3 -m venv .venv
fi

# shellcheck disable=SC1091
source .venv/bin/activate

python3 -m pip install -U pip
python3 -m pip install -U pyserial

cat <<'EOF'

[OK] venv ready + pyserial installed.

Optional (recommended once) to warm PlatformIO caches:
  export PLATFORMIO_CORE_DIR="$HOME/.platformio"
  pio platform install espressif32

Then:
  ./build_all.sh
  python3 tools/dev/serial_smoke.py --role auto --baud 19200 --wait-port 3 --allow-no-hardware

EOF
