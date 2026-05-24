"""Generate a random bearer token for the Zacus Hub gateway.

Usage:
    python gen_token.py            # print token to stdout
    python gen_token.py --env      # write/update .env with ZACUS_HUB_TOKEN=...
"""
from __future__ import annotations

import argparse
import secrets
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--env", action="store_true", help="write .env file alongside this script")
    args = parser.parse_args()

    token = secrets.token_urlsafe(32)

    if args.env:
        env_path = Path(__file__).with_name(".env")
        env_path.write_text(f"ZACUS_HUB_TOKEN={token}\n", encoding="utf-8")
        print(f"wrote {env_path}", file=sys.stderr)

    print(token)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
