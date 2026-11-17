import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


class FeatureTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tool feature 界 ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def cli(self, script, *args):
        return subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / script), *map(str, args)],
                              capture_output=True, encoding="utf-8", timeout=15)

    def test_real_outputs_are_checked_byte_for_byte_without_writes(self):
        source, body, full = [self.root / name for name in ("response.json", "body.txt", "full.txt")]
        notes = "Release 界\r\n## What's Changed\r\n- entry\r\n"
        source.write_text(json.dumps({"body": notes}), encoding="utf-8")
        args = ["--input", source, "--body", body, "--full", full]
        self.assertEqual(self.cli("prepare_release_notes.py", *args).returncode, 0)
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()}
        self.assertEqual(self.cli("prepare_release_notes.py", *args, "--check").returncode, 0)
        self.assertEqual(before, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()})
        body.write_text("stale")
        self.assertEqual(self.cli("prepare_release_notes.py", *args, "--check").returncode, 1)
        self.assertEqual(body.read_text(), "stale")
        body.write_bytes(before[body.name][0])
        full.write_bytes(b"stale full output")
        stale = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()}
        self.assertEqual(self.cli("prepare_release_notes.py", *args, "--check").returncode, 1)
        self.assertEqual(stale, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()})
        full.unlink()
        missing = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()}
        self.assertEqual(self.cli("prepare_release_notes.py", *args, "--check").returncode, 1)
        self.assertEqual(missing, {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.iterdir()})

    def test_missing_outputs_are_not_created_and_bad_json_is_an_error(self):
        source = self.root / "response.json"
        source.write_text('{"body":"notes"}')
        args = ["--input", source, "--body", self.root / "missing" / "body", "--full", self.root / "full", "--check"]
        self.assertEqual(self.cli("prepare_release_notes.py", *args).returncode, 1)
        self.assertEqual(list(self.root.iterdir()), [source])
        source.write_text("{")
        self.assertEqual(self.cli("prepare_release_notes.py", *args).returncode, 2)


if __name__ == "__main__":
    unittest.main()
