"""Record a linked comparison image; this never qualifies or flashes hardware."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
from compare_hex import STOCK_SHA256


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

def memory_usage(size_text):
    sections = {m[1]: int(m[2]) for m in re.finditer(r'(?m)^(\.\S+)\s+(\d+)\s+\d+', size_text)}
    flash = sections['.text'] + sections.get('.data', 0)
    sram = sum(sections.get(name, 0) for name in ('.data', '.bss', '.noinit'))
    if flash > 253952 or sram > 6144:
        raise ValueError('Comparison image exceeds flash/static SRAM development budget')
    return flash, sram


def main():
    output = Path('/out')
    flash, sram = memory_usage((output / 'size.txt').read_text())
    image_range = hex_range((output / 'build/jedi.hex').read_bytes())
    if image_range['first'] != 0 or image_range['last'] >= 253952:
        raise SystemExit('Comparison HEX overlaps bootloader or lacks reset vector')
    for name, target in [('jedi.hex', 'jedi-reconstructed-NOT-FOR-FLASHING.hex'), ('jedi.elf', 'jedi-reconstructed.elf'), ('jedi.map', 'jedi-reconstructed.map')]:
        shutil.copyfile(output / 'build' / name, output / target)
    report = {
        'source_commit': os.environ['UM3_SOURCE_COMMIT'],
        'source_dirty': os.environ['UM3_SOURCE_DIRTY'] == 'true',
        'source_date_epoch': int(os.environ['SOURCE_DATE_EPOCH']),
        'arduino': '1.0.5',
        'compiler': (output / 'toolchain.txt').read_text().splitlines()[0],
        'toolchain_packages': (output / 'toolchain-packages.txt').read_text().splitlines(),
        'toolchain_lock_sha256': hashlib.sha256(Path('/opt/um3-toolchain/toolchain.sha256').read_bytes()).hexdigest(),
        'docker_image': os.environ['UM3_IMAGE_ID'],
        'docker_platform': 'linux/amd64',
        'sdk_sha256': '3047aa2a0482aea9eeafbea0c0a43756c8bda0772aaeff97856e1d70ad16cfaf',
        'flash_bytes': flash, 'static_sram_bytes': sram,
        'application_address_range': image_range,
        'stack_interrupt_headroom_bytes': 8192 - sram,
        'stack_high_water_measured': False,
        'stock_hex_sha256': STOCK_SHA256,
        'stock_hex_identical': hashlib.sha256((output / 'build/jedi.hex').read_bytes()).hexdigest() == STOCK_SHA256,
        'hardware_release': False,
        'sha256': {},
    }
    sources = {}
    for folder in ['Marlin', 'reconstruction']:
        for path in sorted((Path('/source') / folder).rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts:
                sources[str(path.relative_to('/source'))] = hashlib.sha256(path.read_bytes()).hexdigest()
    (output / 'sources.json').write_text(json.dumps(sources, indent=2) + '\n')
    for name in ['jedi-reconstructed-NOT-FOR-FLASHING.hex', 'jedi-reconstructed.elf', 'jedi-reconstructed.map', 'assembly.txt', 'toolchain.txt', 'toolchain-packages.txt', 'size.txt', 'sources.json']:
        report['sha256'][name] = hashlib.sha256((output / name).read_bytes()).hexdigest()
    (output / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    (output / 'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name, digest in report['sha256'].items()))
    print(f'Experimental comparison image linked: flash {flash}, static SRAM {sram}; NOT FOR FLASHING')


if __name__ == '__main__':
    main()
