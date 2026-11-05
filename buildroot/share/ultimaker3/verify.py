#!/usr/bin/env python3
"""Offline evidence and release checks for the UM3 port; never flashes hardware."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
DOCS = ROOT / "docs/ultimaker3"
UPSTREAM = "7d300b42acadb3e7467e9f4b4fc03ebea592c2d0"
VENDOR = "4545818112027387f09e87f930117ab8f3804327"
PACKAGE_SHA256 = "5599f1108837fc365b4f5fd567d1561db749a7b28f7befad1e3117d75b182df2"
GRIFFIN = "usr/share/griffin/griffin/"
BOOTLOADER = "lib/firmware/avr/bootloader/bootloader-atmega2560-uart250000-2.hex"
MEMBERS = [
    "etc/jedi_version", "etc/ultimaker_firmware", "var/lib/dpkg/status",
    GRIFFIN + "machines/um3.json", GRIFFIN + "machines/9511.json",
    GRIFFIN + "machines/jedi.hex",
    GRIFFIN + "printer/drivers/marlin/applicationLayer.py",
    GRIFFIN + "printer/drivers/marlin/transportLayer.py",
    GRIFFIN + "printer/drivers/marlin/dataLinkLayer.py",
    GRIFFIN + "printer/drivers/marlin/osiHelpers/networkPacket.py",
    GRIFFIN + "printer/drivers/protocolDataLine.py",
    GRIFFIN + "printer/procedures/initialization/initiateHardwareInitialization.py",
    GRIFFIN + "printer/heatableObject/heatableObject.py",
    GRIFFIN + "machines/9066.json",
    GRIFFIN + "math/crc16.py", GRIFFIN + "math/crc8.py", BOOTLOADER,
    "usr/lib/python3/dist-packages/ucFirmwareUpgrade/avr/multi/multiStageProgrammer.py",
    "usr/lib/python3/dist-packages/ucFirmwareUpgrade/common/firmwareUpdateBase.py",
    "usr/lib/python3/dist-packages/ucFirmwareUpgrade/common/chipDB.json",
]
STALE_FILES = [
    "Configuration.h", "Configuration_adv.h", "pins.h", "Marlin.h",
    "MarlinSerial.cpp", "MarlinSerial.h", "planner.cpp", "planner.h",
    "stepper.cpp", "stepper.h", "temperature.cpp", "temperature.h",
    "thermistortables.h", "watchdog.cpp", "watchdog.h",
]
REQUIRED_IDS = set("""
source_completeness transport_primitive transport_integration board_pins
thermal_conversion thermal_control safety motion probe print_core_eeprom
i2c_fans_lighting host_runtime_configuration updater_compatibility
avr_build_memory offline_acceptance commands_motion_core commands_probe
commands_power_and_safety commands_peripherals commands_status commands_quickstop
commands_flow commands_runtime_geometry commands_temperature_compensation commands_board_debug
""".split())


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def json_with_comments(data):
    # Preserve quoted URLs and escaped quotes while removing the vendor's comments.
    pattern = r'"(?:\\.|[^"\\])*"|//[^\r\n]*|/\*[\s\S]*?\*/'
    return json.loads(re.sub(pattern, lambda m: "" if m[0].startswith(("//", "/*")) else m[0], data))


def hex_range(data):
    """Validate Intel HEX records and return the occupied data address range."""
    base, addresses, ended = 0, [], False
    for line in data.decode("ascii").splitlines():
        if not line:
            continue
        if ended or not line.startswith(":"):
            raise ValueError("Invalid Intel HEX record or data after EOF")
        row = bytes.fromhex(line[1:])
        if len(row) < 5 or len(row) != row[0] + 5 or sum(row) % 256:
            raise ValueError("Invalid Intel HEX length/checksum")
        count, offset, kind = row[0], int.from_bytes(row[1:3], "big"), row[3]
        payload = row[4:-1]
        if kind == 0:
            addresses.extend(range(base + offset, base + offset + count))
        elif kind in (2, 4) and count == 2 and offset == 0:
            base = int.from_bytes(payload, "big") << (4 if kind == 2 else 16)
        elif kind == 1 and count == 0 and offset == 0:
            ended = True
        elif kind not in (3, 5) or count != 4:
            raise ValueError("Unsupported or malformed Intel HEX record")
    if not ended or not addresses:
        raise ValueError("Intel HEX requires data and an EOF record")
    return {"first": min(addresses), "last": max(addresses), "data_bytes": len(addresses)}


def inspect_host(package, extractor):
    with package.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != PACKAGE_SHA256:
        raise ValueError(f"Wrong host package SHA256: {digest}")
    members = {
        name: subprocess.check_output([extractor, "e", "-so", str(package.resolve()), name])
        for name in MEMBERS
    }
    base = json_with_comments(members[GRIFFIN + "machines/um3.json"].decode())
    extended = json_with_comments(members[GRIFFIN + "machines/9511.json"].decode())
    settings = base["properties"]
    selected = [
        "hotend_count", "feature_bed_leveling_sensor", "feature_filament_flow_sensor",
        "default_steps_per_mm", "default_axis_inverted", "default_motor_current",
        "default_power_budget_management", "default_hotend_pid", "default_bed_pid",
        "temperature_compensation_hotends", "temperature_compensation_build_plate",
    ]
    version = re.search(rb"Package: jedi-marlin\n(?:(?!\n\n).)*?\nVersion: ([^\n]+)",
                        members["var/lib/dpkg/status"], re.S)
    if not version:
        raise ValueError("Missing jedi-marlin package version")
    return {
        "package_sha256": digest,
        "host_version": members["etc/jedi_version"].decode().strip(),
        "avr_package_version": version[1].decode(),
        "members": {name: {"sha256": sha256(data), "size": len(data)} for name, data in members.items()},
        "host_defaults": {name: settings[name] for name in selected},
        "extended_overlay": extended,
        "motion_controller": settings["hardware"]["motion_controller"],
        "stock_application": hex_range(members[GRIFFIN + "machines/jedi.hex"]),
        "stock_bootloader": hex_range(members[BOOTLOADER]),
    }


def audit_vendor(git_dir):
    def git(*args):
        return subprocess.check_output(["git", f"--git-dir={git_dir}", *args]).decode().strip()
    rows = {}
    for name in STALE_FILES:
        path = "Marlin/" + name
        current = git("rev-parse", f"{VENDOR}:{path}")
        parent = git("rev-parse", f"{VENDOR}^:{path}")
        rows[path] = {"vendor_blob": current, "parent_blob": parent, "unchanged_from_um2": current == parent}
    return {"vendor_revision": VENDOR, "um2_parent": git("rev-parse", VENDOR + "^"), "files": rows}


def release_blockers(inventory):
    if not isinstance(inventory, dict) or inventory.get("schema_version") != 1:
        return ["Invalid compatibility inventory schema"]
    blockers = []
    if inventory.get("release_ready") is not True:
        blockers.append("Release has not been qualified")
    entries = inventory.get("entries")
    if not isinstance(entries, list) or not entries:
        return blockers + ["Compatibility inventory is missing"]
    ids = set()
    for row in entries:
        if not isinstance(row, dict) or not row.get("id") or row["id"] in ids:
            blockers.append("Invalid or duplicate inventory entry")
            continue
        ids.add(row["id"])
        if row["id"] in REQUIRED_IDS or row.get("required") is not False:
            if row.get("status") not in ("implemented_verified", "not_applicable") or not row.get("evidence"):
                blockers.append(f"{row['id']}: {row.get('reason', 'not verified')}")
    blockers.extend(f"Missing required inventory entry: {name}" for name in sorted(REQUIRED_IDS - ids))
    return blockers


def check_release():
    blockers = release_blockers(json.loads((DOCS / "inventory.json").read_text()))
    if blockers:
        raise ValueError("UM3_RELEASE_BLOCKED\n" + "\n".join(blockers))


def avr_protocol(core):
    compiler = core / "packages/toolchain-atmelavr/bin/avr-g++"
    with tempfile.TemporaryDirectory(prefix="um3-avr-") as tmp:
        subprocess.run([
            str(compiler), "-mmcu=atmega2560", "-DF_CPU=16000000L", "-DUM3_COMPATIBILITY",
            "-std=gnu++11", "-Os", "-Wall", "-Wextra", "-Werror", "-fstack-usage", "-c",
            str(ROOT / "Marlin/src/feature/ultimaker3/serial_protocol.cpp"),
            "-o", str(Path(tmp) / "serial_protocol.o"),
        ], check=True)
    print("UM3 transport compiles for ATmega2560 (object only; no firmware image)")


def build_report(core, environment, output):
    if environment == "um3_extended":
        check_release()
    build = ROOT / ".pio/build" / environment
    size_output = subprocess.check_output([
        str(core / "packages/toolchain-atmelavr/bin/avr-size"), "-A", str(build / "firmware.elf")
    ]).decode()
    sections = {m[1]: int(m[2]) for m in re.finditer(r"(?m)^(\.\S+)\s+(\d+)\s+\d+", size_output)}
    flash = sections[".text"] + sections.get(".data", 0)
    static_sram = sum(sections.get(name, 0) for name in (".data", ".bss", ".noinit"))
    if flash > 253952 or static_sram > 6144:
        raise ValueError(f"Memory budget exceeded: flash={flash}, static SRAM={static_sram}")
    image_range = hex_range((build / "firmware.hex").read_bytes())
    if image_range["first"] != 0 or image_range["last"] >= 253952:
        raise ValueError("Application HEX overlaps the bootloader reservation or lacks the reset vector")
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT))
    output.mkdir(parents=True, exist_ok=True)
    artifacts = {}
    for name in ("firmware.hex", "firmware.elf", "firmware.map"):
        data = (build / name).read_bytes()
        (output / name).write_bytes(data)
        artifacts[name] = sha256(data)
    report = {
        "environment": environment, "um3_firmware": environment == "um3_extended",
        "commit": head, "worktree_dirty": dirty, "upstream_base": UPSTREAM,
        "flash_bytes": flash, "static_sram_bytes": static_sram,
        "stack_interrupt_headroom_bytes": 8192 - static_sram,
        "stack_high_water_measured": False, "sections": sections,
        "application_address_range": image_range,
        "artifacts": artifacts,
        "source_date_epoch": int(subprocess.check_output(["git", "show", "-s", "--format=%ct", "HEAD"], cwd=ROOT)),
        "packages": {
            name: json.loads((core / location / "package.json").read_text())["version"]
            for name, location in (("compiler", "packages/toolchain-atmelavr"),
                                   ("arduino_framework", "packages/framework-arduino-avr"))
        },
        "platform_version": json.loads((core / "platforms/atmelavr/platform.json").read_text())["version"],
        "configuration_sha256": {
            name: sha256((ROOT / name).read_bytes())
            for name in ("Marlin/Configuration.h", "Marlin/Configuration_adv.h", "ini/ultimaker3.ini",
                         "buildroot/share/ultimaker3/requirements.txt")
        },
        "compiler": subprocess.check_output([str(core / "packages/toolchain-atmelavr/bin/avr-g++"), "--version"]).decode().splitlines()[0],
    }
    (output / "size.txt").write_text(size_output)
    (output / "provenance.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "SHA256SUMS").write_text("".join(f"{digest}  {name}\n" for name, digest in artifacts.items()))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    host = sub.add_parser("inspect-host")
    host.add_argument("package", type=Path)
    host.add_argument("--extractor", default="7zz", help="7zz on macOS, 7z on Ubuntu")
    host.add_argument("--check", action="store_true", help="Compare with committed host-baseline.json")
    vendor = sub.add_parser("audit-vendor")
    vendor.add_argument("git_dir", type=Path)
    sub.add_parser("check-release")
    avr = sub.add_parser("avr-protocol")
    avr.add_argument("--core", type=Path, default=Path(os.environ.get("PLATFORMIO_CORE_DIR", Path.home() / ".platformio")))
    report = sub.add_parser("build-report")
    report.add_argument("--core", type=Path, required=True)
    report.add_argument("--environment", choices=("um3_upstream_check", "um3_extended"), required=True)
    report.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "inspect-host":
            result = inspect_host(args.package, args.extractor)
            if args.check:
                if result != json.loads((DOCS / "host-baseline.json").read_text()):
                    raise ValueError("Host baseline differs from the recorded evidence")
                print("Pinned host package and extracted baseline match")
                return
        elif args.command == "audit-vendor":
            result = audit_vendor(args.git_dir)
        elif args.command == "check-release":
            check_release()
            print("UM3 release inventory is qualified")
            return
        elif args.command == "avr-protocol":
            avr_protocol(args.core)
            return
        else:
            result = build_report(args.core, args.environment, args.output)
        print(json.dumps(result, indent=2, sort_keys=True))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
