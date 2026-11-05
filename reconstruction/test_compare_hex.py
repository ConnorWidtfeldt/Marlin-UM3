import unittest
from compare_hex import compare, decode_hex
from test_report import record


class HexComparisonTests(unittest.TestCase):
    def test_packaging_does_not_change_programmed_bytes(self):
        a = (record(0, data=b'abcd') + record(1)).encode()
        b = (record(0, data=b'ab') + record(0, 2, b'cd') + record(1)).replace('\n', '\r\n').encode()
        result = compare(a, b)
        self.assertFalse(result['hex_files_identical'])
        self.assertTrue(result['flash_bytes_identical'])

    def test_payload_and_missing_addresses_are_not_masked(self):
        a = (record(0, data=b'abcd') + record(1)).encode()
        b = (record(0, data=b'axc') + record(1)).encode()
        r = compare(a, b)
        self.assertFalse(r['flash_bytes_identical'])
        self.assertEqual((r['changed_shared_addresses'], r['stock_only_addresses']), (1, 1))
        self.assertEqual(r['first_changed_addresses'], [1])

    def test_extended_base_and_empty_holes_are_significant(self):
        a = (record(4, data=b'\x00\x01') + record(0, 2, b'x') + record(1)).encode()
        b = (record(0, 2, b'x') + record(1)).encode()
        self.assertEqual(decode_hex(a)[0], {65538: ord('x')})
        self.assertFalse(compare(a, b)['flash_bytes_identical'])

    def test_entry_point_records_and_malformed_inputs(self):
        a = (record(0, data=b'x') + record(1)).encode()
        b = (record(0, data=b'x') + record(5, data=b'\x00\x00\x00\x02') + record(1)).encode()
        self.assertTrue(compare(a, b)['flash_bytes_identical'])
        self.assertFalse(compare(a, b)['entry_records_identical'])
        for s in [record(0, data=b'x'), record(1) + record(0, data=b'x'),
                  record(0, data=b'x') + record(0, data=b'y') + record(1),
                  record(0, data=b'x').replace('78', '79') + record(1),
                  record(7, data=b'x') + record(1)]:
            with self.assertRaises(ValueError):
                decode_hex(s.encode())


if __name__ == '__main__':
    unittest.main()
