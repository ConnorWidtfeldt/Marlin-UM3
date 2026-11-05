#!/usr/bin/env python3
"""Test production thermal interlocks against controlled stock ARM replay."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SIMULATOR_SHA256 = "bcbf7de08b16ad0c8c38b5a182a1b85ed49c81f87459f088d30fa385587965ee"


def number(value):
    if not math.isfinite(value):
        raise ValueError("stock fixture must contain finite temperatures")
    return repr(float(value)) + "f"


def boolean(value):
    return "true" if value else "false"


def oracle_header(path):
    fixture = json.loads(path.read_text())
    if fixture["schema_version"] != 1 or fixture["simulator_sha256"] != SIMULATOR_SHA256:
        raise ValueError("unrecognized stock oracle identity/schema")
    if not fixture["removal_cases"] or not fixture["watchdog_cases"]:
        raise ValueError("stock oracle must include removal and watchdog cases")
    lines = ["// Generated from stock_oracle.json; do not edit.", "static void test_stock_oracle() {"]
    for index, case in enumerate(fixture["removal_cases"]):
        lines += ["  {", "    HotendRemovalInterlock core;"]
        if not case["initial_allowed"]:
            lines.append("    core.request(false, 0, 0);")
        lines.append(f"    core.request({boolean(case['requested'])}, "
                     f"{number(case['temperature'])}, {number(case['target'])});")
        label = json.dumps(f"stock ARM removal request {index}")
        lines.append(f"    require(core.removalAllowed() == {boolean(case['allowed'])}, {label});")
        lines.append("  }")
    for case in fixture["watchdog_cases"]:
        lines += ["  {", "    HeatingProgressWatchdog watchdog;"]
        for index, sample in enumerate(case["samples"]):
            now = int(sample["now_ms"])
            pwm = int(sample["pwm"])
            if not 0 <= now <= 0xFFFFFFFF or not 0 <= pwm <= 255:
                raise ValueError("stock watchdog sample exceeds uint32_t/uint8_t")
            label = json.dumps(f"stock ARM watchdog {case['name']} sample {index}")
            lines.append(f"    require(watchdog.update({pwm}, {number(sample['temperature'])}, "
                         f"uint32_t({now}UL)) == {boolean(sample['fault'])}, {label});")
        lines.append("  }")
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oracle", type=Path, default=HERE / "stock_oracle.json")
    args = parser.parse_args()
    source = ROOT / "Marlin/src/feature/ultimaker3"
    with tempfile.TemporaryDirectory(prefix="um3-interlock-") as directory:
        directory = Path(directory)
        (directory / "interlock_oracle.h").write_text(oracle_header(args.oracle))
        binary = directory / "test_interlock"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-pedantic", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-ffp-contract=off", "-DUM3_PROTOCOL_TEST", "-I", str(directory), "-I", str(source),
                        str(source / "thermal_interlock.cpp"), str(HERE / "test_interlock.cpp"),
                        "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
