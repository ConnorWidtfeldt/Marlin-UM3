#!/usr/bin/env python3
"""Exercise actual historical serial code with mocked UART registers; no hardware."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="um3-stock-serial-") as directory:
    binary = Path(directory) / "test_serial"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++11", "-Wall", "-Wextra", "-Werror",
        "-Wno-expansion-to-defined", "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer", str(HERE / "test_serial.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
