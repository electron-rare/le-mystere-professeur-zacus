#!/usr/bin/env python3
import argparse
import json
import math
import time

import serial


def send_line(ser: serial.Serial, payload: dict) -> None:
    ser.write((json.dumps(payload, ensure_ascii=False) + "\n").encode("utf-8"))


def main() -> int:
    parser = argparse.ArgumentParser(description="RP2040 UI JSONL state/tick simulator")
    parser.add_argument("--port", required=True, help="Serial port (ex: /dev/tty.usbmodemXXXX)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--source", choices=["sd", "radio"], default="radio")
    parser.add_argument("--title", default="Station Zacus Live")
    parser.add_argument("--station", default="U-SON FM")
    parser.add_argument("--artist", default="")
    parser.add_argument("--dur", type=int, default=0, help="seconds; 0 for radio/live")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    start = time.time()
    next_hb = 0.0
    next_state = 0.0
    next_tick = 0.0
    pos = 0

    try:
        while True:
            now = time.time() - start
            if now >= next_state:
                send_line(
                    ser,
                    {
                        "t": "state",
                        "playing": True,
                        "source": args.source,
                        "title": args.title,
                        "artist": args.artist,
                        "station": args.station,
                        "pos": pos,
                        "dur": args.dur,
                        "vol": 42,
                        "rssi": -61,
                        "buffer": 88 if args.source == "radio" else -1,
                        "error": "",
                    },
                )
                next_state += 1.0

            if now >= next_tick:
                vu = 0.2 + 0.7 * abs(math.sin(now * 2.1))
                send_line(
                    ser,
                    {
                        "t": "tick",
                        "pos": pos,
                        "buffer": 85 if args.source == "radio" else -1,
                        "vu": round(vu, 3),
                    },
                )
                next_tick += 0.2
                if args.dur > 0:
                    pos = min(args.dur, pos + 1)

            if now >= next_hb:
                send_line(ser, {"t": "hb", "ms": int(now * 1000)})
                next_hb += 1.0

            time.sleep(0.02)
    except KeyboardInterrupt:
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
