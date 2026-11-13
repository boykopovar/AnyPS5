import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import trace_digest
import trace_intervals_csv
from test_trace_digest import interval_line


class IntervalCsvTests(unittest.TestCase):
    def test_real_intervals_preserve_values_and_warning_markers(self):
        first = interval_line(0.0, 30.0, 20.0, 5.0, commit="a" * 40)
        second = interval_line(30.0, 60.0, 40.0, 10.0, commit="b" * 40, trace_dropped=True)
        result = trace_digest.digest_intervals([first, second])
        rows = list(csv.DictReader(io.StringIO(trace_intervals_csv.render(result), newline="")))
        self.assertEqual(len(rows), 2)
        self.assertEqual(float(rows[0]["p99_ms"]), first["frame_ms"]["p99"])
        self.assertEqual(float(rows[0]["gpu_busy_ms_per_present"]), 5.0)
        self.assertEqual(rows[0]["identity_changed"], "False")
        self.assertEqual(rows[1]["identity_changed"], "True")
        self.assertEqual(rows[1]["trace_dropped"], "True")
        self.assertEqual(float(rows[1]["start_s"]), 30.0)
        self.assertEqual(rows[1]["interval"], "2")

    def test_text_quoting_and_absent_maps_round_trip(self):
        name = 'phase, "界"\r\nnext'
        first = interval_line(0.0, 30.0, 20.0, 5.0, top_phases_ms_per_frame={name: 7.5}, worker_busy_pct={name: 120.0})
        second = interval_line(30.0, 60.0, 20.0, 5.0)
        second.pop("worker_busy_pct", None)
        second.pop("top_phases_ms_per_frame", None)
        rows = list(csv.DictReader(io.StringIO(trace_intervals_csv.render(trace_digest.digest_intervals([first, second])), newline="")))
        self.assertEqual(rows[0]["top_phase"], name)
        self.assertEqual(rows[0]["busiest_worker"], name)
        self.assertEqual(float(rows[0]["busiest_worker_pct"]), 120.0)
        self.assertTrue(all(rows[1][key] == "" for key in ("top_phase", "top_phase_ms_per_frame", "busiest_worker", "busiest_worker_pct")))

    def test_spreadsheet_safe_mode_escapes_formula_names_without_changing_default_csv(self):
        for name in ("=1+1", "   @SUM(1)", "\uff1d1+1", "\r\n+1"):
            with self.subTest(name=name):
                line = interval_line(0.0, 30.0, 20.0, 5.0, worker_busy_pct={name: 70.0},
                                     top_phases_ms_per_frame={name: 3.0})
                result = trace_digest.digest_intervals([line])
                raw = list(csv.DictReader(io.StringIO(trace_intervals_csv.render(result), newline="")))[0]
                self.assertEqual((raw["busiest_worker"], raw["top_phase"]), (name, name))
                safe = list(csv.DictReader(io.StringIO(trace_intervals_csv.render(result, spreadsheet_safe=True), newline="")))[0]
                self.assertEqual((safe["busiest_worker"], safe["top_phase"]), ("\t" + name, "\t" + name))
                self.assertEqual(safe["p50_ms"], raw["p50_ms"])

    def test_spreadsheet_safe_cli_writes_escaped_name(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.jsonl"
            line = interval_line(0.0, 30.0, 20.0, 5.0, worker_busy_pct={"=1+1": 70.0})
            path.write_text(json.dumps(line) + "\n", encoding="utf-8")
            completed = subprocess.run([sys.executable, str(Path(trace_intervals_csv.__file__)), str(path), "--spreadsheet-safe"],
                                       capture_output=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            row = list(csv.DictReader(io.StringIO(completed.stdout.decode(), newline="")))[0]
            self.assertEqual(row["busiest_worker"], "\t=1+1")

    def test_spreadsheet_safe_cli_with_complete_capture_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.jsonl"
            line = interval_line(0.0, 30.0, 20.0, 5.0, worker_busy_pct={"=1+1": 70.0})
            command = [sys.executable, str(Path(trace_intervals_csv.__file__)), str(path),
                       "--require-complete", "--spreadsheet-safe"]
            path.write_text(json.dumps(line) + "\n", encoding="utf-8")
            complete = subprocess.run(command, capture_output=True)
            self.assertEqual(complete.returncode, 0, complete.stderr)
            row = list(csv.DictReader(io.StringIO(complete.stdout.decode(), newline="")))[0]
            self.assertEqual(row["busiest_worker"], "\t=1+1")
            line["trace_dropped"] = True
            path.write_text(json.dumps(line) + "\n", encoding="utf-8")
            dropped = subprocess.run(command, capture_output=True)
            self.assertEqual(dropped.returncode, 2, dropped.stderr)
            self.assertEqual(dropped.stdout, b"")

    def test_actual_cli_and_empty_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "summary.jsonl"
            line = interval_line(0.0, 30.0, 20.0, 5.0)
            path.write_text(json.dumps(line) + "\n", encoding="utf-8")
            command = [sys.executable, str(Path(trace_intervals_csv.__file__)), str(path)]
            completed = subprocess.run(command, capture_output=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertNotIn(b"\r\r\n", completed.stdout)
            self.assertEqual(completed.stdout, trace_intervals_csv.render(trace_digest.digest_intervals([line])).encode())
            path.write_bytes(b"")
            empty = subprocess.run(command, capture_output=True)
            self.assertEqual(empty.returncode, 0, empty.stderr)
            self.assertEqual(len(list(csv.reader(io.StringIO(empty.stdout.decode())))), 1)

    def test_unusable_input_is_a_cli_error(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.jsonl"
            for contents in (None, b"not json", b"\xff", b'{"interval_start_s":0}'):
                if contents is not None:
                    path.write_bytes(contents)
                completed = subprocess.run([sys.executable, str(Path(trace_intervals_csv.__file__)), str(path)], capture_output=True)
                self.assertEqual(completed.returncode, 2, completed.stderr)
                self.assertEqual(completed.stdout, b"")
                self.assertNotIn(b"Traceback", completed.stderr)


if __name__ == "__main__":
    unittest.main()
