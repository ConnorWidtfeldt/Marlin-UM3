#!/usr/bin/env python3
"""Test production UM3 thermal math against stock-instruction replay fixtures."""
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
        raise ValueError("oracle numeric fixtures must be finite")
    return repr(float(value)) + "f"


def oracle_header(path):
    fixture = json.loads(path.read_text())
    if fixture["schema_version"] != 1 or fixture["simulator_sha256"] != SIMULATOR_SHA256:
        raise ValueError("unrecognized stock oracle identity/schema")
    if not fixture["pid_cases"] or not fixture["conversion_cases"]:
        raise ValueError("stock oracle must include PID and conversion fixtures")
    lines = ["// Generated from stock_oracle.json; do not edit.", "static void test_stock_oracle() {"]
    setters = {"target": "setTargetTemperature", "kff": "setKff", "kp": "setKp", "ki": "setKi",
               "kd": "setKd", "ki_max": "setKiMax", "kpcf": "setKpcf", "functional_range": "setFunctionalRange"}
    for case in fixture["pid_cases"]:
        settings = case["settings"]
        lines += ["  {", "    TemperaturePID pid;"]
        for key, setter in setters.items():
            lines.append(f"    pid.{setter}({number(settings[key])});")
        lines.append(f"    pid.setControlRange({number(settings['min'])}, {number(settings['max'])});")
        for index, sample in enumerate(case["samples"]):
            label = json.dumps(f"stock ARM PID {case['name']} sample {index}")
            lines.append(f"    expect_pwm(pid.update({number(sample['temperature'])}, {int(sample['fan'])}), "
                         f"{int(sample['output'])}, {label});")
        lines.append("  }")
    for case in fixture["conversion_cases"]:
        kind = case["kind"]
        if kind not in ("hotend", "bed"):
            raise ValueError("unknown stock conversion table")
        raw = int(case["raw_sum"])
        if not -32768 <= raw <= 32767:
            raise ValueError("stock raw sample exceeds int16_t")
        label = json.dumps(f"stock ARM {kind} conversion raw {raw}")
        lines.append(f"  expect_float({kind}_temperature_from_raw_sum({raw}), "
                     f"{number(case['celsius'])}, {label});")
    lines.append("}")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--oracle", type=Path, default=HERE / "stock_oracle.json")
    args = parser.parse_args()
    source = ROOT / "Marlin/src/feature/ultimaker3"
    with tempfile.TemporaryDirectory(prefix="um3-thermal-") as directory:
        directory = Path(directory)
        (directory / "thermal_oracle.h").write_text(oracle_header(args.oracle))
        binary = directory / "test_thermal"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-pedantic", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-ffp-contract=off", "-DUM3_PROTOCOL_TEST", "-I", str(directory), "-I", str(source),
                        str(source / "temperature_pid.cpp"), str(source / "temperature_conversion.cpp"),
                        str(HERE / "test_thermal.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
