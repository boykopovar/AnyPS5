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
        result = self.run_tool(cache, seed)
        self.assertEqual(result.returncode, 2)
        self.assertIn(str(cache), result.stderr)
        self.assertFalse(seed.exists())
        cache.write_text("OPT:BOOL=ON\n", encoding="utf-8")
        seed.write_bytes(b"")
        self.assertEqual(self.run_tool(cache, seed).returncode, 0)

    def test_reports_messages_on_stdout(self):
        cache, seed = self.root / "cache", self.root / "seed"
        cache.write_text("//Have include a.h\nHAVE_A_H:INTERNAL=1\n//Have include b.h\nHAVE_B_H:INTERNAL=0\n", encoding="utf-8")
        seed.write_bytes(b'set("HAVE_A_H" "1" CACHE INTERNAL "Have include a.h")\nset("HAVE_B_H" "0" CACHE INTERNAL "Have include b.h")\n')
        result = self.run_tool(cache, seed)
        self.assertEqual((result.returncode, result.stdout.strip()), (0, "2 check results match"))
        seed.write_bytes(b"x")
        result = self.run_tool(cache, seed)
        self.assertEqual((result.returncode, result.stdout.strip()), (1, f"stale seed: {seed}"))
        seed.unlink()
        result = self.run_tool(cache, seed)
        self.assertEqual((result.returncode, result.stdout.strip()), (1, f"missing seed: {seed}"))

    def test_seed_entries_must_be_sorted_by_name(self):
        cache, seed = self.root / "cache", self.root / "seed"
        cache.write_text("//Have include a.h\nHAVE_A_H:INTERNAL=1\n//Have include b.h\nHAVE_B_H:INTERNAL=0\n", encoding="utf-8")
        seed.write_bytes(b'set("HAVE_B_H" "0" CACHE INTERNAL "Have include b.h")\nset("HAVE_A_H" "1" CACHE INTERNAL "Have include a.h")\n')
        self.assertEqual(self.run_tool(cache, seed).returncode, 1)

    def test_missing_cache_is_usage_error(self):
        result = self.run_tool(self.root / "nocache", self.root / "seed")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
