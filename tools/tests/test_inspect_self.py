import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'inspect_self.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PYTHONIOENCODING": "ascii"})

    def fixture(self):
        import struct
        data = bytearray(0x500)
        struct.pack_into("<I", data, 0, 0x1d3d154f)
        struct.pack_into("<QQQQ", data, 0x40, 0x800, 0x400, 16, 16)
        data[0x180:0x187] = b"\x7fELF\x02\x01\x01"
        struct.pack_into("<HHIQQQIHHHHHH", data, 0x190, 0xfe10, 62, 1, 0, 64, 0, 0, 64, 56, 1, 0, 0, 0)
        struct.pack_into("<IIQQQQQQ", data, 0x1c0, 1, 5, 0x9000, 0x1000, 0, 16, 32, 0x1000)
        return data

    def test_reports_original_offsets_and_records_without_writes(self):
        source = self.root / "模块.self"
        data = self.fixture()
        source.write_bytes(data)
        before = source.stat().st_mtime_ns
        result = self.run_tool(source)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report, {"size": 0x500, "magic": 0x1d3d154f, "inner_elf": 0x180,
                         "records": [{"flags": 0x800, "offset": 0x400, "size": 16}],
                         "program_headers": [{"index": 0, "type": 1, "flags": 5, "offset": 0x9000,
                                              "vaddr": 0x1000, "size": 16, "memory": 32, "align": 0x1000}]})
        self.assertEqual(source.read_bytes(), data)
        self.assertEqual(source.stat().st_mtime_ns, before)
        self.assertEqual(list(self.root.iterdir()), [source])

    def test_rejects_malformed_metadata(self):
        source = self.root / "bad.self"
        valid = self.fixture()
        for data in (b"", bytes(0x500), valid[:0x300], valid[:0x184] + bytes([1]) + valid[0x185:]):
            source.write_bytes(data)
            result = self.run_tool(source)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertEqual(result.stdout, "")
            self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
