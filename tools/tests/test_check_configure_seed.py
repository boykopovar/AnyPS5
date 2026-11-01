import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = 'check_configure_seed.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PYTHONIOENCODING": "ascii"})

    def test_exact_stale_and_missing_are_read_only(self):
        cache, seed = self.root / "缓存.txt", self.root / "seed.cmake"
        cache.write_text("//Have include stdint.h\nHAVE_STDINT_H:INTERNAL=1\n//User option\nOPT:BOOL=ON\n", encoding="utf-8-sig")
        expected = b'set("HAVE_STDINT_H" "1" CACHE INTERNAL "Have include stdint.h")\n'
        seed.write_bytes(expected)
        before = seed.stat().st_mtime_ns
        result = self.run_tool(cache, seed)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(seed.stat().st_mtime_ns, before)
        seed.write_bytes(expected.replace(b'"1"', b'"0"'))
        before = seed.read_bytes()
        self.assertEqual(self.run_tool(cache, seed).returncode, 1)
        self.assertEqual(seed.read_bytes(), before)
        missing = self.root / "absent" / "seed.cmake"
        self.assertEqual(self.run_tool(cache, missing).returncode, 1)
        self.assertFalse(missing.parent.exists())

    def test_invalid_cache_and_empty_seed(self):
        cache, seed = self.root / "cache", self.root / "seed"
        cache.write_bytes(b"\xff")
        self.assertEqual(self.run_tool(cache, seed).returncode, 2)
        self.assertFalse(seed.exists())
        cache.write_text("OPT:BOOL=ON\n", encoding="utf-8")
        seed.write_bytes(b"")
        self.assertEqual(self.run_tool(cache, seed).returncode, 0)


if __name__ == "__main__":
    unittest.main()
