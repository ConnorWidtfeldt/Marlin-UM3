#!/usr/bin/env python3
"""Exercise the production UM3 frontend/drivers with mocked hardware and motion."""
import argparse
import json
import re
import os
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--stock-root',type=Path)
args=parser.parse_args()
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
SOURCE=ROOT/'Marlin/src/feature/ultimaker3'
STOCK=SOURCE/'stock'
# Production TWI/USART/stepper registers are replaced only at this test boundary.
excluded={'i2c_driver.cpp','i2c_capacitance_FDC1004.cpp','i2c_onewire_ds2482.cpp',
          'stepper_TMC2130.cpp','usart_spi_driver.cpp'}
sources=[p for p in sorted(STOCK.glob('*.cpp')) if p.name not in excluded]
with tempfile.TemporaryDirectory(prefix='um3-integration-') as directory:
    binary=Path(directory)/'test_integration'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++11','-g','-O1',
        '-fsanitize=address,undefined','-fno-omit-frame-pointer',
        '-Wno-narrowing','-DUM3_COMPATIBILITY','-D__AVR_ATmega2560__','-DARDUINO=105','-DF_CPU=16000000',
        '-I',str(HERE/'hardware'),'-I',str(STOCK),'-include',str(HERE/'hardware/um3_test_hal.h'),
        *map(str,sources),str(SOURCE/'power_budget.cpp'),str(SOURCE/'motion_contract.cpp'),
        str(HERE/'test_integration.cpp'),'-o',str(binary)],check=True)
    for board in (447,630,869):
        trace=Path(directory)/f'trace-{board}.txt'
        subprocess.run([str(binary),str(board),str(trace)],check=True,timeout=30)
        if args.stock_root and trace.exists():
            from run import stock_modules
            fixtures=json.loads((HERE/'fixtures.json').read_text())
            _,modules=stock_modules(args.stock_root,fixtures['source_sha256'])
            parser=modules['stock_protocol_line'].ProtocolDataLine
            responses=[line for line in trace.read_text().splitlines() if re.fullmatch('[oqr][0-9A-F]{6}',line)]
            assert len(responses)>300
            for response in responses:
                parsed=parser.parseMarlinResponse(response)
                assert parsed and parsed.valid, response
            print(f'Board {board}: {len(responses)} actual firmware responses accepted by pinned stock host parser')
