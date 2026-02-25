#!/usr/bin/env python3
"""Generate Zacus QR payloads with CRC16/CCITT-FALSE and optional PNG export.

Format:
  <DATA><sep><CRC16_HEX>
"""

from __future__ import annotations

import argparse
import sys


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) & 0xFFFF) ^ 0x1021
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def ascii_upper(value: str) -> str:
    chars = []
    for ch in value:
        code = ord(ch)
        if 97 <= code <= 122:
            chars.append(chr(code - 32))
        else:
            chars.append(ch)
    return "".join(chars)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate QR payload DATA+CRC16 for SCENE_CAMERA_SCAN validation"
    )
    parser.add_argument("data", help="DATA part without CRC suffix")
    parser.add_argument(
        "--sep",
        default="*",
        help="Separator between DATA and CRC (first character used, default: *)",
    )
    parser.add_argument(
        "--ci",
        action="store_true",
        help="Case-insensitive CRC mode (CRC computed on ASCII uppercased DATA)",
    )
    parser.add_argument(
        "--scope",
        default="data",
        choices=["data", "suffix"],
        help="CRC scope: full DATA or suffix after --prefix",
    )
    parser.add_argument(
        "--prefix",
        default="",
        help="Prefix used when --scope suffix",
    )
    parser.add_argument(
        "--png",
        default="",
        help="Optional output PNG path (requires qrcode[pil])",
    )
    return parser


def export_png(payload: str, output_path: str) -> None:
    try:
        import qrcode
        from qrcode.constants import ERROR_CORRECT_Q
    except Exception as exc:
        raise RuntimeError(
            "qrcode dependency missing; install with: pip install 'qrcode[pil]'"
        ) from exc

    qr = qrcode.QRCode(
        version=None,
        error_correction=ERROR_CORRECT_Q,
        box_size=10,
        border=4,
    )
    qr.add_data(payload)
    qr.make(fit=True)
    image = qr.make_image(fill_color="black", back_color="white")
    image.save(output_path)


def main() -> int:
    args = build_parser().parse_args()

    sep = args.sep[0] if args.sep else ""
    if not sep:
        print("ERROR: --sep cannot be empty", file=sys.stderr)
        return 2

    data = args.data.strip()
    if not data:
        print("ERROR: data is empty", file=sys.stderr)
        return 2

    crc_view = data
    if args.scope == "suffix":
        if not args.prefix:
            print("ERROR: --prefix is required when --scope suffix", file=sys.stderr)
            return 2
        if args.ci:
            if not ascii_upper(data).startswith(ascii_upper(args.prefix)):
                print("ERROR: data does not start with prefix (ci)", file=sys.stderr)
                return 2
        else:
            if not data.startswith(args.prefix):
                print("ERROR: data does not start with prefix", file=sys.stderr)
                return 2
        crc_view = data[len(args.prefix) :]
        if not crc_view:
            print("ERROR: empty CRC scope after prefix", file=sys.stderr)
            return 2

    crc_input = ascii_upper(crc_view) if args.ci else crc_view
    crc = crc16_ccitt_false(crc_input.encode("utf-8"))
    payload = f"{data}{sep}{crc:04X}"
    print(payload)

    if args.png:
        try:
            export_png(payload, args.png)
        except RuntimeError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2
        print(f"PNG saved: {args.png}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
