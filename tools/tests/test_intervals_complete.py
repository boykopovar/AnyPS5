import json
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

    def test_strict_export_requires_adjacent_intervals(self):
        from test_trace_digest import interval_line
        path = self.root / "aps5-summary.jsonl"
        first = interval_line(0.0, 30.012, 20, 5)
        for start, end, first_present in ((30.012, 60.024, 1800), (0.0, 30.008, 0), (60.024, 90.036, 3600)):
            with self.subTest(start=start):
                second = interval_line(start, end, 20, 5)
                second["window"]["first_present"] = first_present
                path.write_text("\n".join(map(json.dumps, (first, second))) + "\n", encoding="utf-8")
                ordinary = self.cli("trace_intervals_csv.py", path)
                strict = self.cli("trace_intervals_csv.py", path, "--require-complete")
                self.assertEqual(ordinary.returncode, 0, ordinary.stderr)
                if start == first["interval_end_s"]:
                    self.assertEqual(strict.returncode, 0, strict.stderr)
                    self.assertEqual(strict.stdout, ordinary.stdout)
                else:
                    self.assertEqual((strict.returncode, strict.stdout), (2, ""), strict.stderr)
                    self.assertIn("interval 2 starts at", strict.stderr)

    def test_strict_export_rejects_duplicate_zero_length_intervals(self):
        from test_trace_digest import interval_line
        path = self.root / "aps5-summary.jsonl"
        first = interval_line(0.0, 30.0, 20, 5)
        point = interval_line(30.0, 30.0, 20, 5, presents=0)
        last = interval_line(30.0, 60.0, 20, 5)
        point["window"]["first_present"] = last["window"]["first_present"] = first["presents"]
        for points in ([point], [point, point]):
            with self.subTest(point_count=len(points)):
                path.write_text("\n".join(map(json.dumps, [first, *points, last])) + "\n", encoding="utf-8")
                ordinary = self.cli("trace_intervals_csv.py", path)
                strict = self.cli("trace_intervals_csv.py", path, "--require-complete")
                self.assertEqual(ordinary.returncode, 0, ordinary.stderr)
                if len(points) == 1:
                    self.assertEqual(strict.returncode, 0, strict.stderr)
                    self.assertEqual(strict.stdout, ordinary.stdout)
                else:
                    self.assertEqual((strict.returncode, strict.stdout), (2, ""), strict.stderr)
                    self.assertIn("interval 3 duplicates the previous interval", strict.stderr)

    def test_strict_export_verifies_present_ranges(self):
        from test_trace_digest import interval_line
        path = self.root / "aps5-summary.jsonl"
        first = interval_line(10, 40, 20, 5)
        first["window"]["first_present"] = 600
        second = interval_line(40, 70, 20, 5)
        expected = first["window"]["first_present"] + first["presents"]
        for actual, diagnostic in ((expected, None), (expected - 1, f"expected present {expected}"),
                                   (expected + 1, f"expected present {expected}"), (True, "window.first_present"),
                                   (None, "window.first_present")):
            with self.subTest(first_present=actual):
                if actual is None:
                    second["window"].pop("first_present", None)
                else:
                    second["window"]["first_present"] = actual
                path.write_text("\n".join(map(json.dumps, (first, second))) + "\n", encoding="utf-8")
                ordinary = self.cli("trace_intervals_csv.py", path)
                strict = self.cli("trace_intervals_csv.py", path, "--require-complete")
                self.assertEqual(ordinary.returncode, 0, ordinary.stderr)
                if diagnostic is None:
                    self.assertEqual(strict.returncode, 0, strict.stderr)
                    self.assertEqual(strict.stdout, ordinary.stdout)
                else:
                    self.assertEqual((strict.returncode, strict.stdout), (2, ""), strict.stderr)
                    self.assertIn(diagnostic, strict.stderr)


if __name__ == "__main__":
    unittest.main()
