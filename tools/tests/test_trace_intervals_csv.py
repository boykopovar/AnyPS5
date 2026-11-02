import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
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
