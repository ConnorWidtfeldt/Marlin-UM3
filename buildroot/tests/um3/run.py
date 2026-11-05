#!/usr/bin/env python3
"""Compile and test the production decoder; optionally verify fixtures with stock host code."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def stock_modules(root, expected_hashes):
    paths = {
        "griffin.math.crc16": root / "griffin/math/crc16.py",
        "griffin.math.crc8": root / "griffin/math/crc8.py",
        "stock_network_packet": root / "griffin/printer/drivers/marlin/osiHelpers/networkPacket.py",
        "stock_protocol_line": root / "griffin/printer/drivers/protocolDataLine.py",
    }
    for name, path in paths.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hashes[name]:
            raise ValueError(f"Stock source digest does not match fixtures: {path}")
    for name in ("griffin", "griffin.math"):
        sys.modules[name] = types.ModuleType(name)
    modules = {}
    for name, path in paths.items():
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
        modules[name] = module
    return paths, modules


def verify_stock(fixtures, root):
    _, modules = stock_modules(root, fixtures["source_sha256"])
    packet = modules["stock_network_packet"].NetworkPacket
    response = modules["stock_protocol_line"].ProtocolDataLine
    for item in fixtures["packets"]:
        assert packet(item["sequence"], item["payload"]).getBytes().hex() == item["hex"]
    stream = b"".join(packet(sequence % 255, "M105").getBytes() for sequence in range(256))
    assert stream.hex() == fixtures["sequence_stream_hex"]
    for item in fixtures["responses"]:
        parsed = response.parseMarlinResponse(item["wire"].strip())
        assert parsed and parsed.valid
        assert (parsed.packet_type, parsed.sequence_number, parsed.plan_buffer_size) == (
            item["type"], item["sequence"], item["planner_free"])
    print("Fixtures independently verified with unmodified UM3 5.3.0 host modules", flush=True)


def header(fixtures):
    rows = ["#include <stddef.h>", "#include <stdint.h>"]
    rows += ["struct PacketFixture { const uint8_t *wire; size_t size; uint8_t sequence; const char *payload; };",
             "struct StreamFixture { const uint8_t *wire; size_t size; };",
             "struct ResponseFixture { char type; uint8_t sequence, planner_free; const char *wire; };"]
    for group in ("packets", "invalid"):
        for i, item in enumerate(fixtures[group]):
            values = ",".join(str(byte) for byte in bytes.fromhex(item["hex"]))
            rows.append(f"static const uint8_t {group}_{i}[] = {{{values}}};")
    rows.append("static const PacketFixture PACKETS[] = {")
    for i, item in enumerate(fixtures["packets"]):
        rows.append(f"{{packets_{i}, sizeof(packets_{i}), {item['sequence']}, {json.dumps(item['payload'])}}},")
    rows.append("};\nstatic const StreamFixture INVALID[] = {")
    for i, _ in enumerate(fixtures["invalid"]):
        rows.append(f"{{invalid_{i}, sizeof(invalid_{i})}},")
    values = ",".join(str(byte) for byte in bytes.fromhex(fixtures["sequence_stream_hex"]))
    rows.append(f"}};\nstatic const uint8_t SEQUENCE_STREAM[] = {{{values}}};")
    rows.append("static const ResponseFixture RESPONSES[] = {")
    for item in fixtures["responses"]:
        rows.append(f"{{'{item['type']}', {item['sequence']}, {item['planner_free']}, {json.dumps(item['wire'])}}},")
    rows.append("};")
    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock-root", type=Path, help="Extracted /usr/share/griffin directory, containing griffin/math")
    args = parser.parse_args()
    fixtures = json.loads((HERE / "fixtures.json").read_text())
    if args.stock_root:
        verify_stock(fixtures, args.stock_root)
    source = ROOT / "Marlin/src/feature/ultimaker3"
    with tempfile.TemporaryDirectory(prefix="um3-protocol-") as directory:
        build = Path(directory)
        (build / "fixtures.h").write_text(header(fixtures))
        binary = build / "test_serial_protocol"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-pedantic", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-DUM3_PROTOCOL_TEST", "-I", str(source), "-I", str(build),
                        str(source / "serial_protocol.cpp"), str(HERE / "test_serial_protocol.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
