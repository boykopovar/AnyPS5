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

    def test_real_jsonl_rejects_drops_or_identity_changes_before_csv(self):
        from test_trace_digest import interval_line
        path = self.root / "summary.jsonl"
        first = interval_line(0, 30, 20, 5, commit="a" * 40)
        for second in (interval_line(30, 60, 20, 5, commit="b" * 40), interval_line(30, 60, 20, 5, commit="a" * 40, trace_dropped=True)):
            path.write_text("\n".join(map(json.dumps, [first, second])))
            ordinary = self.cli("trace_intervals_csv.py", path)
            strict = self.cli("trace_intervals_csv.py", path, "--require-complete")
            self.assertEqual(ordinary.returncode, 0, ordinary.stderr)
            self.assertEqual((strict.returncode, strict.stdout), (2, ""), strict.stderr)
        path.write_text(json.dumps(first))
        result = self.cli("trace_intervals_csv.py", path, "--require-complete")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("interval,start_s", result.stdout)

    def test_invalid_capture_is_not_exported(self):
        path = self.root / "bad.jsonl"
        path.write_text("not json")
        result = self.cli("trace_intervals_csv.py", path, "--require-complete")
        self.assertEqual((result.returncode, result.stdout), (2, ""))
        path.write_bytes(b"")
        result = self.cli("trace_intervals_csv.py", path, "--require-complete")
        self.assertEqual((result.returncode, result.stdout), (2, ""))


if __name__ == "__main__":
    unittest.main()
