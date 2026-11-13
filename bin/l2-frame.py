#!/usr/bin/env python3
"""Read the L2 framebuffer over USB without changing settings or transmitting RF."""
import argparse
import struct
import time
import zlib
from pathlib import Path
import serial


def exact(port, size):
    data = bytearray()
    deadline = time.monotonic() + 4
    while len(data) < size and time.monotonic() < deadline:
        data.extend(port.read(size - len(data)))
    if len(data) != size:
        raise TimeoutError("Incomplete framebuffer response")
    return bytes(data)


def capture(port_name):
    pixels = bytearray()
    with serial.Serial(port_name, 115200, timeout=0.1) as port:
        port.reset_input_buffer()
        for offset in range(0, 320 * 240 * 2, 32):
            request = bytes([56, 129]) + struct.pack("<I", offset) + bytes([32])
            port.write(b"<" + struct.pack("<H", len(request)) + request)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if port.read(1) != b">":
                    continue
                size = struct.unpack("<H", exact(port, 2))[0]
                if size > 1024:
                    continue
                body = exact(port, size)
                if len(body) >= 6 and body[:2] == bytes([24, 129]) and struct.unpack_from("<I", body, 2)[0] == offset:
                    if len(body) != 38:
                        raise ValueError("Unexpected framebuffer chunk length")
                    pixels.extend(body[6:])
                    break
            else:
                raise TimeoutError("No L2 framebuffer response; requires UI 0.3.0 or later")
    return pixels


def png(pixels):
    # LovyanGFX sprites store swapped RGB565 bytes, most significant first.
    rows = bytearray()
    for y in range(240):
        rows.append(0)
        for x in range(320):
            value = struct.unpack_from(">H", pixels, 2 * (y * 320 + x))[0]
            rows.extend(((value >> 11) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31))
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    return bytes.fromhex("89504e470d0a1a0a") + chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 240, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM61")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    pixels = capture(args.port)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(png(pixels))
    print(f"Saved actual 320x240 framebuffer: {args.output}; first pixel {pixels[:2].hex()}")
