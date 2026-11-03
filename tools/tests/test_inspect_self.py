import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import tracemalloc
import unittest
from pathlib import Path

SCRIPT = 'inspect_self.py'
TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import inspect_self


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

    def test_large_payload_is_not_copied_into_the_python_heap(self):
        source = self.root / "large.self"
        with source.open("wb") as output:
            output.write(self.fixture())
            for _ in range(64):
                output.write(bytes(128 * 1024))
        before = source.stat()
        printed = io.StringIO()
        started_tracing = not tracemalloc.is_tracing()
        if started_tracing:
            tracemalloc.start()
        tracemalloc.reset_peak()
        baseline, _ = tracemalloc.get_traced_memory()
        try:
            with contextlib.redirect_stdout(printed):
                code = inspect_self.main([str(source)])
            _, peak = tracemalloc.get_traced_memory()
        finally:
            if started_tracing:
                tracemalloc.stop()
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(printed.getvalue())["size"], 0x500 + 8 * 1024 * 1024)
        self.assertLess(peak - baseline, 2 * 1024 * 1024)
        self.assertEqual(source.stat().st_mtime_ns, before.st_mtime_ns)
        self.assertEqual(source.stat().st_mode, before.st_mode)
        source.rename(self.root / "closed.self")

    def test_invalid_metadata_releases_the_input_file(self):
        source = self.root / "invalid.self"
        data = self.fixture()
        data[0x184] = 1
        source.write_bytes(data)
        errors = io.StringIO()
        printed = io.StringIO()
        with contextlib.redirect_stderr(errors), contextlib.redirect_stdout(printed):
            with self.assertRaises(SystemExit) as caught:
                inspect_self.main([str(source)])
        self.assertEqual(caught.exception.code, 2)
        self.assertIn("not 64-bit little-endian", errors.getvalue())
        self.assertEqual(printed.getvalue(), "")
        self.assertEqual(source.read_bytes(), data)
        source.unlink()


if __name__ == "__main__":
    unittest.main()
