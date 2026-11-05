#!/usr/bin/env python3
"""Compile and test the recovered motion contracts, without operating hardware."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = ROOT / "Marlin/src/feature/ultimaker3"

with tempfile.TemporaryDirectory(prefix="um3-motion-") as directory:
    binary = Path(directory) / "test_motion_contract"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-DUM3_PROTOCOL_TEST",
        "-I", str(SOURCE), str(SOURCE / "motion_contract.cpp"),
        str(HERE / "test_motion_contract.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
