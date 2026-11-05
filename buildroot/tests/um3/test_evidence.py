"""Trust-boundary checks for package evidence and the hardware release gate."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("um3_verify", ROOT / "buildroot/share/ultimaker3/verify.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


def record(kind, offset=0, data=b""):
    body = bytes([len(data), offset >> 8, offset & 255, kind]) + data
    return ":" + (body + bytes([-sum(body) & 255])).hex() + "\n"


class EvidenceTests(unittest.TestCase):
    def test_json_comments_preserve_urls_and_quotes(self):
        source = '{"url":"https://example.com/*file*/", /* comment */ "quote":"a\\\"//b" // comment\n}'
        self.assertEqual(verify.json_with_comments(source), {"url": "https://example.com/*file*/", "quote": 'a"//b'})

    def test_intel_hex_extended_address_and_checksum(self):
        image = (record(4, data=b"\x00\x03") + record(0, 0xE000, b"\x12\x34") + record(1)).encode()
        self.assertEqual(verify.hex_range(image), {"first": 0x3E000, "last": 0x3E001, "data_bytes": 2})
        with self.assertRaisesRegex(ValueError, "checksum"):
            verify.hex_range(image.replace(b"1234", b"1235"))
        for invalid in (record(0, data=b"x"), record(1) + record(0, data=b"x"), record(4, data=b"x") + record(1)):
            with self.assertRaises(ValueError):
                verify.hex_range(invalid.encode())

    def test_wrong_package_rejected_before_extractor(self):
        with tempfile.TemporaryDirectory() as tmp:
            package = Path(tmp) / "wrong.swu"
            package.write_bytes(b"not the pinned package")
            with self.assertRaisesRegex(ValueError, "SHA256"):
                verify.inspect_host(package, "/extractor-must-never-execute")

    def test_actual_inventory_blocks_hardware_release(self):
        inventory = json.loads((verify.DOCS / "inventory.json").read_text())
        self.assertTrue(verify.release_blockers(inventory))
        # A flag alone must not bypass required incomplete work.
        inventory["release_ready"] = True
        self.assertTrue(verify.release_blockers(inventory))
        for row in inventory["entries"]:
            row["required"] = False
        self.assertTrue(verify.release_blockers(inventory))

    def test_gate_rejects_missing_duplicate_and_unevidenced_entries(self):
        inventory = {
            "schema_version": 1, "release_ready": True,
            "entries": [{"id": name, "required": True, "status": "implemented_verified", "evidence": ["test"]}
                        for name in sorted(verify.REQUIRED_IDS)],
        }
        self.assertFalse(verify.release_blockers(inventory))
        for change in ("missing", "duplicate", "evidence", "status"):
            modified = copy.deepcopy(inventory)
            if change == "missing":
                modified["entries"].pop()
            elif change == "duplicate":
                modified["entries"].append(modified["entries"][0])
            else:
                modified["entries"][0][change] = None
            self.assertTrue(verify.release_blockers(modified))
        for invalid in ([], {}, {"schema_version": 1, "release_ready": True, "entries": []}):
            self.assertTrue(verify.release_blockers(invalid))


if __name__ == "__main__":
    unittest.main()
