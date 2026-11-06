#!/usr/bin/env python3
"""Exercise the production UM3 frontend/drivers with mocked hardware and motion."""
import ast
import hashlib
import logging
import types
import warnings
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
            # Execute only the pinned host's temperature parser method, with local signal/log sinks.
            application=args.stock_root/'griffin/printer/drivers/marlin/applicationLayer.py'
            inventory=json.loads((ROOT/'docs/ultimaker3/inventory.json').read_text())
            expected=inventory['host_package']['member_sha256']['usr/share/griffin/griffin/printer/drivers/marlin/applicationLayer.py']
            assert hashlib.sha256(application.read_bytes()).hexdigest()==expected
            with warnings.catch_warnings():
                warnings.simplefilter("ignore",SyntaxWarning)
                tree=ast.parse(application.read_text())
            method=next(node for node in ast.walk(tree) if isinstance(node,ast.FunctionDef) and node.name=='__handleTemperatureReply')
            namespace={'re':re,'log':logging.getLogger('um3-stock-temperature-parser')}
            exec(compile(ast.Module(body=[method],type_ignores=[]),str(application),'exec'),namespace)
            rows=[];temperatures=[];bed=[]
            sink=types.SimpleNamespace(emit=lambda *args:None)
            host=types.SimpleNamespace(onHotendTemperatureUpdate=types.SimpleNamespace(emit=lambda *v:temperatures.append(v)),
                onBedTemperatureUpdate=types.SimpleNamespace(emit=lambda *v:bed.append(v)),
                onCartridgePresentChanged=sink,onCartridgeEnabledChanged=sink,
                ENCODE_JOBSTATE_AS_NUMBER_TABLE={'idle':0})
            host.__hotend_count=2;host.__hotend_present=[False,False];host.__hotend_enabled=[False,False]
            host.__data_logger=types.SimpleNamespace(addData=lambda *v:rows.append(v))
            host.__controller=types.SimpleNamespace(getPropertyValue=lambda key:'idle',getGCodeActiveHotend=lambda:0)
            reports=[line for line in trace.read_text().splitlines() if line.startswith(' T0:')]
            assert reports
            for line in reports: namespace['__handleTemperatureReply'](host,0,line)
            assert len(temperatures)==2*len(reports) and len(bed)==len(reports)
            assert rows[-1][1]==200 and rows[-1][6]==200 and rows[-1][11]==100
            print(f'Board {board}: temperature/presence/target report accepted by pinned stock application parser')
            print(f'Board {board}: {len(responses)} actual firmware responses accepted by pinned stock host parser')
