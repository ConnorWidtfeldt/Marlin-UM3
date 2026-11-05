#!/usr/bin/env python3
"""Compile and exercise the production UM3 power-budget model."""
import os
import json
from pathlib import Path
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def main():
    source = ROOT / "Marlin/src/feature/ultimaker3"
    with tempfile.TemporaryDirectory(prefix="um3-power-") as directory:
        build = Path(directory)
        oracle = json.loads((HERE / "stock_oracle.json").read_text())
        rows = ["struct StockBedFixture { float temperature, remaining; unsigned char expected; };",
                "static const StockBedFixture STOCK_BED_FIXTURES[] = {"]
        for case in oracle["bed_budget_cases"]:
            rows.append("{%s, %s, %s}," % (case["temperature"], case["remaining_watts"], case["output"]))
        (build / "power_fixtures.h").write_text("\n".join(rows + ["};"]))
        binary = build / "test_power_budget"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
                        "-pedantic", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-DUM3_PROTOCOL_TEST", "-I", str(source), "-I", str(build), str(source / "power_budget.cpp"),
                        str(HERE / "test_power_budget.cpp"), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
