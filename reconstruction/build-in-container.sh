#!/bin/sh
set -eu
rm -f /out/jedi-reconstructed-NOT-FOR-FLASHING.hex /out/jedi-reconstructed.elf /out/jedi-reconstructed.map /out/provenance.json /out/SHA256SUMS
avr-g++ --version > /out/toolchain.txt
cmake --version >> /out/toolchain.txt
avr-ld --version >> /out/toolchain.txt
cmake -S /source/Marlin -B /out/build \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_C_COMPILER=/usr/share/arduino/hardware/tools/avr/bin/avr-gcc \
  -DCMAKE_CXX_COMPILER=/usr/share/arduino/hardware/tools/avr/bin/avr-g++ \
  -DCMAKE_C_FLAGS=-mmcu=atmega2560 \
  -DCMAKE_CXX_FLAGS=-mmcu=atmega2560 \
  -DCMAKE_EXE_LINKER_FLAGS=-Wl,-Map,/out/build/jedi.map \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build /out/build -- -j2
avr-size -A /out/build/jedi.elf > /out/size.txt
avr-objdump -d -C /out/build/jedi.elf > /out/assembly.txt
python3 /source/reconstruction/report.py
