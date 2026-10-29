import copy
import csv
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import performance_compare
import performance_csv
from test_performance_compare import SUMMARY


class PerformanceCsvTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.before = copy.deepcopy(SUMMARY)
        self.after = copy.deepcopy(SUMMARY)
        self.after.update(commit="def456", trace="candidate.json.gz")
        self.after["frame_ms"] = {"p50": 10, "p90": 20, "p99": 30, "max": 35}
        self.baseline = self.root / "before.jsonl"
        self.candidate = self.root / "after.jsonl"

    def run_csv(self, *arguments):
        self.baseline.write_text(json.dumps(self.before) + "\n", encoding="utf-8")
        self.candidate.write_text(json.dumps(self.after) + "\n", encoding="utf-8")
        return subprocess.run([sys.executable, str(Path(performance_csv.__file__)), str(self.baseline),
                               str(self.candidate), *arguments], capture_output=True, timeout=5,
                              env={**os.environ, "PYTHONIOENCODING": "ascii:strict"})

    def rows(self, result):
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8"))
        self.assertEqual(result.stderr, b"")
        return list(csv.DictReader(io.StringIO(result.stdout.decode("utf-8"), newline="")))

    def test_numeric_results_preserve_the_existing_comparison(self):
        result = self.run_csv()
        rows = self.rows(result)
        row = next(row for row in rows if row["kind"] == "metric" and row["name"] == "frame_ms.p50")
        self.assertEqual(row["unit"], "ms")
        self.assertEqual([float(row[key]) for key in ("baseline", "candidate", "change", "change_pct")], [20, 10, -10, -50])
        report = performance_compare.compare(self.before, self.after)
        for row in (row for row in rows if row["kind"] == "metric"):
            for field in ("baseline", "candidate", "change", "change_pct"):
                expected = report["metrics_ms"][row["name"]][field]
                self.assertEqual(None if row[field] == "" else float(row[field]), expected)

    def test_unicode_commas_quotes_and_newlines_round_trip_as_csv(self):
        name = 'Driver, "phase"\r\n\u6e2c\u8a66'
        self.before["top_phases_ms_per_frame"] = {name: 12}
        self.after["top_phases_ms_per_frame"] = {name: 6}
        self.before["trace"] = 'capture, "before"\r\n\u8cc7\u6599.json.gz'
        rows = self.rows(self.run_csv())
        phase = next(row for row in rows if row["kind"] == "phase")
        self.assertEqual(phase["name"], name)
        self.assertEqual(float(phase["change"]), -6)
        trace = next(row for row in rows if row["kind"] == "capture" and row["name"] == "trace")
        self.assertEqual(trace["baseline"], self.before["trace"])

    def test_missing_phases_and_zero_baselines_are_empty_not_zero(self):
        self.before["gpu_busy_ms_per_present"] = 0
        self.before["top_phases_ms_per_frame"] = {"before": 1}
        self.after["top_phases_ms_per_frame"] = {"after": 2}
        rows = self.rows(self.run_csv())
        for row in (row for row in rows if row["kind"] == "phase"):
            self.assertEqual((row["change"], row["change_pct"]), ("", ""))
            self.assertTrue(row["baseline"] == "" or row["candidate"] == "")
        gpu = next(row for row in rows if row["name"] == "gpu_busy_ms_per_present")
        self.assertEqual(gpu["change_pct"], "")

    def test_context_differences_and_notes_are_preserved_or_explicitly_rejected(self):
        self.after["driver"] = "different driver"
        rows = self.rows(self.run_csv())
        context = next(row for row in rows if row["kind"] == "configuration" and row["name"] == "driver")
        self.assertEqual(context["candidate"], "different driver")
        self.assertTrue(any(row["kind"] == "note" and "do not isolate" in row["name"] for row in rows))
        result = self.run_csv("--require-matching-context")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertIn(b"recorded configuration differs", result.stderr)

    def test_invalid_input_does_not_emit_a_partial_csv(self):
        self.after["frame_ms"]["p50"] = -1
        result = self.run_csv()
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")
        self.assertNotIn(b"Traceback", result.stderr)

    def test_trace_selection_uses_the_existing_jsonl_selection_contract(self):
        self.baseline.write_text(json.dumps(self.before) + "\n" + json.dumps(self.after) + "\n", encoding="utf-8")
        self.candidate.write_bytes(self.baseline.read_bytes())
        result = subprocess.run([sys.executable, str(Path(performance_csv.__file__)), str(self.baseline), str(self.candidate),
                                 "--baseline-trace", self.before["trace"], "--candidate-trace", self.after["trace"]],
                                capture_output=True, timeout=5)
        rows = self.rows(result)
        commit = next(row for row in rows if row["kind"] == "capture" and row["name"] == "commit")
        self.assertEqual((commit["baseline"], commit["candidate"]), (self.before["commit"], self.after["commit"]))


if __name__ == "__main__":
    unittest.main()
