#!/usr/bin/env python3
"""Validate and package the two L2 Pro PlatformIO builds without flashing a device."""
import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def validate_image(data, label):
    require(len(data) >= 24 and data[0] == 0xE9, f"{label}: invalid ESP image")
    require(struct.unpack_from("<H", data, 12)[0] == 9, f"{label}: expected ESP32-S3")
    require(data[3] >> 4 == 4, f"{label}: expected 16 MB Flash")
    require(0 < data[1] <= 16, f"{label}: invalid segment count")
    offset, checksum = 24, 0xEF
    for _ in range(data[1]):
        require(offset + 8 <= len(data), f"{label}: truncated segment header")
        length = struct.unpack_from("<I", data, offset + 4)[0]
        offset += 8
        require(offset + length <= len(data), f"{label}: truncated segment")
        for value in data[offset:offset + length]:
            checksum ^= value
        offset += length
    end = offset + (15 - offset % 16)
    require(end < len(data) and data[end] == checksum, f"{label}: checksum mismatch")
    require(data[23] == 1, f"{label}: missing validation hash")
    require(data[end + 1:end + 33] == hashlib.sha256(data[:end + 1]).digest(),
            f"{label}: image validation hash mismatch")


def package(project, output, framework):
    artifacts = []
    for variant in ("usb", "ble"):
        env = "WioTrackerL2Pro_companion_radio_" + variant
        build = project / ".pio" / "build" / env
        app = (build / "firmware.bin").read_bytes()
        boot = (build / "bootloader.bin").read_bytes()
        partitions = (build / "partitions.bin").read_bytes()
        merged = (build / "firmware-merged.bin").read_bytes()
        boot_app = (framework / "tools" / "partitions" / "boot_app0.bin").read_bytes()
        validate_image(app, env + " application")
        validate_image(boot, env + " bootloader")
        require(boot[2:4] == app[2:4], env + ": boot/app Flash settings differ")
        entries = []
        for offset in range(0, len(partitions) - 31, 32):
            if partitions[offset:offset + 2] != bytes.fromhex("aa50"):
                break
            _, kind, subtype, address, size, name, _ = struct.unpack_from("<2sBBII16sI", partitions, offset)
            entries.append((kind, subtype, address, size, name.rstrip(bytes([0])).decode()))
        app0 = next(entry for entry in entries if entry[0] == 0 and entry[1] == 0x10)
        require(app0[2] == 0x10000 and len(app) <= app0[3], env + ": app partition overflow")
        for entry in entries:
            require(entry[2] + entry[3] <= 16 * 1024 * 1024, env + ": partition exceeds Flash")
        for offset, component, label in ((0, boot, "bootloader"), (0x8000, partitions, "partitions"),
                                          (0xE000, boot_app, "boot_app0"), (app0[2], app, "application")):
            require(merged[offset:offset + len(component)] == component,
                    env + ": merged " + label + " mismatch")
        require(len(merged) == app0[2] + len(app), env + ": unexpected merged size")
        artifacts.append((env, variant.upper(), build / "firmware.bin", "app", 0x10000))
        artifacts.append((env, variant.upper(), build / "firmware-merged.bin", "merged", 0))
    # Validate both builds before exporting anything.
    output.mkdir(parents=True, exist_ok=True)
    manifest = {"board": "Wio Tracker L2 Pro", "ui": "development 0.5.1", "flash_bytes": 16777216,
                "validation": {"images": "passed", "full_hardware_acceptance": "pending"}, "artifacts": []}
    for env, variant, source, kind, address in artifacts:
        dest = output / f"Wio-Tracker-L2-Pro-{variant}-{kind}.bin"
        shutil.copyfile(source, dest)
        data = dest.read_bytes()
        record = {"file": dest.name, "environment": env, "size": len(data),
                  "flash_address": hex(address), "sha256": hashlib.sha256(data).hexdigest()}
        validation = project / "out/l2-debug/COM61-0.5.1-validation.json"
        if kind == "app" and validation.exists():
            verified = json.loads(validation.read_text(encoding="utf-8"))
            if verified.get("app_sha256") == record["sha256"]:
                record["device_validation"] = {"port": "COM61", "boot": "passed"}
                if "identity_preserved" in verified:
                    record["device_validation"]["identity_preserved"] = verified["identity_preserved"]
                else:
                    record["device_validation"]["identity_verification"] = verified.get("identity_verification", "not rechecked")
                if "user_confirmed" in verified:
                    record["device_validation"]["user_confirmed"] = verified["user_confirmed"]
        manifest["artifacts"].append(record)
        print(f"{dest}: {len(data)} bytes, flash at {hex(address)}, SHA256 {record['sha256']}")
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (output / "SHA256SUMS.txt").write_text("".join(f"{a['sha256']}  {a['file']}\n" for a in manifest["artifacts"]), encoding="ascii")
    (output / "README.txt").write_text(
        "Wio Tracker L2 Pro / Seeed UI development 0.5.1\n"
        "USB: local UI + USB companion. BLE: local UI + USB + optional Bluetooth.\n"
        "Flash *-merged.bin at 0x0 for a complete installation.\n"
        "The merged image includes the NVS configuration area and resets saved settings.\n"
        "*-app.bin contains only the application (app0: 0x10000 in this partition layout).\n"
        "Bluetooth starts off; enable in Settings, pairing PIN in About.\n"
        "First boot requires confirming a radio preset on the device.\n"
        "Body: 16px; bold titles/buttons; secondary text: 14px.\n"
        "Swipe left: next page, last wraps to first. Swipe right: previous. Tap: select.\n"
        "Swipe up from the bottom edge: navigation menu; swipe down to close.\n"
        "Menu auto-hides after 5 seconds idle. Audio is a sixth main page.\n"
        "Messages: Public / Private / Contacts; add channels from +.\n"
        "Contacts: tap to expand, star to favourite. Green: heard within 15 min.\n"
        "Audio: /record/*.wav; /music/*.mp3. Tap a file for player/seek/volume.\n"
        "Battery estimate uses user-approved linear endpoints: 3300mV=0%, 4200mV=100%.\n"
        "Unlock: drag the left thumb to the right end, then release.\n"
        "Lock screen: two unread cards, latest ten; swipe down for older cards.\n"
        "Map: pan/zoom, Track/Waypoint/Tile menu; grid fallback; real GPS marker.\n"
        "Map: SD XYZ PNG tiles at /map/z/x/y.png; tracks at /tracks/*.csv.\n"
        "Home: animated seven-segment clock. Apps: GPS travel compass (requires movement).\n"
        "Screenshots: enable in Apps; hold upper-left screen 1.2s; /screenshots/*.bmp.\n"
        "Build/image validation passed. See manifest for hash-matched device validation.\n"
        "0.5.1 hardware acceptance pending; previous 0.5.0 GPS/audio acceptance does not verify new UI.\n"
        "See docs/l2-pro-bringup.md for implemented features and remaining PRD work.\n",
        encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--framework", type=Path, default=Path.home() / ".platformio/packages/framework-arduinoespressif32")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    package(project, args.output or project / "out/l2-pro", args.framework)
