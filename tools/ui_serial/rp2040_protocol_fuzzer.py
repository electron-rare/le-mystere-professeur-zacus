#!/usr/bin/env python3
import argparse
import random
import string
import time

import serial


MALFORMED = [
    "{",
    '{"t":"state","playing":true',
    '{"t":"state","title":"' + ("X" * 700) + '"}',
    "not_json_line",
    '{"t":123,"a":[]}',
    '{"t":"tick","vu":"bad"}',
    '{"t":"unknown","x":1}',
]

VALID = [
    '{"t":"hb","ms":12345}',
    '{"t":"state","playing":true,"source":"radio","title":"Fuzz Station","artist":"","station":"U-SON","pos":0,"dur":0,"vol":33,"rssi":-70,"buffer":77,"error":""}',
    '{"t":"tick","pos":1,"buffer":76,"vu":0.42}',
]


def random_noise_line(max_len: int = 256) -> str:
    n = random.randint(1, max_len)
    return "".join(random.choice(string.printable) for _ in range(n))


def main() -> int:
    parser = argparse.ArgumentParser(description="RP2040 UI protocol fuzzer")
    parser.add_argument("--port", required=True, help="Serial port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--count", type=int, default=200)
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    try:
        for i in range(args.count):
            if i % 10 == 0:
                line = random.choice(VALID)
            elif i % 3 == 0:
                line = random_noise_line()
            else:
                line = random.choice(MALFORMED)
            ser.write((line + "\n").encode("utf-8", errors="ignore"))
            time.sleep(0.01)
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
