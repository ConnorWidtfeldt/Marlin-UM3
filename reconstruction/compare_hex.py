#!/usr/bin/env python3
"""Compare addressed AVR flash bytes, independently of Intel HEX record layout."""
import argparse
import hashlib
import json
from pathlib import Path

STOCK_SHA256 = '448ca6fefbf9a8ed8bb3b29c444f76c7911ce0a346573e1d64a3ee882875ae65'


def decode_hex(data):
    memory, entries, base, ended = {}, [], 0, False
    for line in data.decode('ascii').splitlines():
        if not line:
            continue
        if ended or not line.startswith(':'):
            raise ValueError('Invalid Intel HEX record or data after EOF')
        row = bytes.fromhex(line[1:])
        if len(row) < 5 or len(row) != row[0] + 5 or sum(row) % 256:
            raise ValueError('Invalid Intel HEX length/checksum')
        count, offset, kind = row[0], int.from_bytes(row[1:3], 'big'), row[3]
        payload = row[4:-1]
        if kind == 0:
            if offset + count > 0x10000:
                raise ValueError('Data record crosses address boundary')
            for i, value in enumerate(payload):
                address = base + offset + i
                if address in memory:
                    raise ValueError('Overlapping Intel HEX records')
                memory[address] = value
        elif kind in (2, 4) and count == 2 and offset == 0:
            base = int.from_bytes(payload, 'big') << (4 if kind == 2 else 16)
        elif kind == 1 and count == 0 and offset == 0:
            ended = True
        elif kind in (3, 5) and count == 4 and offset == 0:
            entries.append((kind, payload.hex()))
        else:
            raise ValueError('Unsupported or malformed Intel HEX record')
    if not ended or not memory:
        raise ValueError('Intel HEX requires data and EOF')
    return memory, entries


def identity(memory):
    digest = hashlib.sha256()
    for address, value in sorted(memory.items()):
        digest.update(address.to_bytes(4, 'big') + bytes([value]))
    return {'data_bytes': len(memory), 'first': min(memory), 'last': max(memory),
            'addressed_bytes_sha256': digest.hexdigest()}


def compare(stock, candidate):
    left, left_entries = decode_hex(stock)
    right, right_entries = decode_hex(candidate)
    shared = left.keys() & right.keys()
    differing = sorted(a for a in shared if left[a] != right[a])
    return {
        'schema_version': 1,
        'hex_files_identical': stock == candidate,
        'flash_bytes_identical': left == right,
        'entry_records_identical': left_entries == right_entries,
        'stock': dict(identity(left), hex_sha256=hashlib.sha256(stock).hexdigest()),
        'candidate': dict(identity(right), hex_sha256=hashlib.sha256(candidate).hexdigest()),
        'changed_shared_addresses': len(differing),
        'stock_only_addresses': len(left.keys() - right.keys()),
        'candidate_only_addresses': len(right.keys() - left.keys()),
        'first_changed_addresses': differing[:64],
        'scope': 'Exact addressed-byte equality only; moved code is not classified as a behavioral difference.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stock', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    stock, candidate = args.stock.read_bytes(), args.candidate.read_bytes()
    if hashlib.sha256(stock).hexdigest() != STOCK_SHA256:
        raise SystemExit('Stock input is not the pinned official UM3 5.3.0 HEX')
    report = compare(stock, candidate)
    result = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.write_text(result)
    print(result, end='')
    raise SystemExit(0 if report['flash_bytes_identical'] and report['entry_records_identical'] else 1)


if __name__ == '__main__':
    main()
