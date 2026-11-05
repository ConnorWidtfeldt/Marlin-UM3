"""Validate the comparison report's memory and Intel HEX rejection boundaries."""
import unittest
from report import hex_range, memory_usage


def record(kind, offset=0, data=b''):
    body = bytes([len(data), offset >> 8, offset & 255, kind]) + data
    return ':' + (body + bytes([-sum(body) & 255])).hex() + '\n'


class ReportTests(unittest.TestCase):
    def test_memory_includes_initialized_data_and_noinit(self):
        self.assertEqual(memory_usage('.text 96118 0\n.data 1310 8389120\n.bss 4449 8390430\n'), (97428, 5759))
        for size in ('.text 253952 0\n.data 1 8389120\n', '.text 1 0\n.bss 6144 8389120\n.noinit 1 8395264\n'):
            with self.assertRaises(ValueError):
                memory_usage(size)

    def test_hex_extended_addresses_and_checksum(self):
        image = record(4, data=b'\x00\x03') + record(0, 0xE000, b'\x12\x34') + record(1)
        self.assertEqual(hex_range(image.encode()), {'first': 253952, 'last': 253953, 'data_bytes': 2})
        with self.assertRaises(ValueError):
            hex_range(image.replace('1234', '1235').encode())
        for invalid in (record(0, data=b'x'), record(1) + record(0, data=b'x')):
            with self.assertRaises(ValueError):
                hex_range(invalid.encode())


if __name__ == '__main__':
    unittest.main()
