#!/usr/bin/env python3
"""Drive a running X-Plane 12 over its UDP interface (port 49000).

Usage:
  xp_udp.py cmnd <command_path>
  xp_udp.py set <dataref> <float>
  xp_udp.py get <dataref> [dataref ...]
"""

import socket
import struct
import sys

XP = ("127.0.0.1", 49000)


def sock() -> socket.socket:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(2.0)
    return s


def cmnd(path: str) -> None:
    sock().sendto(b"CMND\x00" + path.encode(), XP)


def set_dref(path: str, value: float) -> None:
    msg = b"DREF\x00" + struct.pack("<f", value) + path.encode()
    sock().sendto(msg.ljust(509, b"\x00"), XP)


def get_drefs(paths: list[str]) -> dict[str, float]:
    s = sock()
    for i, p in enumerate(paths):
        req = b"RREF\x00" + struct.pack("<ii", 5, i + 1) + p.encode().ljust(400, b"\x00")
        s.sendto(req, XP)
    values: dict[str, float] = {}
    try:
        while len(values) < len(paths):
            data, _ = s.recvfrom(4096)
            if not data.startswith(b"RREF"):
                continue
            body = data[5:]
            for off in range(0, len(body), 8):
                idx, val = struct.unpack_from("<if", body, off)
                if 1 <= idx <= len(paths):
                    values[paths[idx - 1]] = val
    except TimeoutError:
        pass
    for i, p in enumerate(paths):  # unsubscribe
        req = b"RREF\x00" + struct.pack("<ii", 0, i + 1) + p.encode().ljust(400, b"\x00")
        s.sendto(req, XP)
    return values


def main() -> None:
    mode = sys.argv[1]
    if mode == "cmnd":
        cmnd(sys.argv[2])
    elif mode == "set":
        set_dref(sys.argv[2], float(sys.argv[3]))
    elif mode == "get":
        for path, value in get_drefs(sys.argv[2:]).items():
            print(f"{path} = {value:.4f}")
    else:
        sys.exit(f"unknown mode {mode}")


if __name__ == "__main__":
    main()
