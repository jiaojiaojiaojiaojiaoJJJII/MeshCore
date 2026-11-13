#!/usr/bin/env python3
"""Query L2 identity fingerprint, storage and display/audio diagnostics. No RF transmission."""
import argparse
import hashlib
import json
import struct
import time
import serial


def exact(port, size):
    result = b""
    deadline = time.monotonic() + 5
    while len(result) < size and time.monotonic() < deadline:
        result += port.read(size - len(result))
    if len(result) != size:
        raise TimeoutError("Incomplete serial response")
    return result


def query(port, payload, code):
    port.write(b"<" + struct.pack("<H", len(payload)) + payload)
    deadline = time.monotonic() + 7
    while time.monotonic() < deadline:
        if port.read(1) != b">":
            continue
        size = struct.unpack("<H", exact(port, 2))[0]
        if size > 1024:
            continue
        body = exact(port, size)
        if body and body[0] == code:
            return body
    raise TimeoutError(f"No response {code}")


def status(port_name):
    with serial.Serial(port_name, 115200, timeout=0.1) as port:
        port.reset_input_buffer()
        identity = query(port, bytes([1]) + bytes(7), 5)[4:36]
        storage = query(port, bytes([20]), 12)
        mv, used, total = struct.unpack_from("<HII", storage, 1)
        display = query(port, bytes([56, 128]), 24)[2:].decode("ascii", "replace")
        audio = query(port, bytes([56, 130]), 24)[2:].decode("ascii", "replace")
        return {"identity_sha256": hashlib.sha256(identity).hexdigest(), "battery_mv": mv, "internal_used_kib": used, "internal_total_kib": total, "display": display, "audio": audio}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM61")
    parser.add_argument("--output")
    args = parser.parse_args()
    result = json.dumps(status(args.port), indent=2)
    print(result)
    if args.output:
        from pathlib import Path
        Path(args.output).write_text(result + "\n", encoding="utf-8")
