import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ab_split
import ab_split_csv
from test_ab_split import log


class AbCsvTests(unittest.TestCase):
    def test_metrics_and_noise_reuse_the_existing_producer(self):
        result = ab_split.split(log(), ["0x1000"], ["staging-in"])
        rows = list(csv.DictReader(io.StringIO(ab_split_csv.render(result), newline="")))
        self.assertEqual(len(rows), len(result["metrics"]))
        for row, metric in zip(rows, result["metrics"]):
            self.assertEqual(row["metric"], metric["metric"])
            for key in ("a", "b", "change", "change_pct"):
                self.assertEqual(row[key], "" if metric[key] is None else str(metric[key]))
        busy = next(row for row in rows if row["metric"] == "GPU busy ms/frame")
        self.assertEqual(float(busy["a"]), 105.0)
        self.assertEqual(float(busy["b"]), 130.0)
        self.assertEqual(float(busy["change"]), 25.0)
        self.assertEqual(float(busy["noise_mean"]), 25.0)
        self.assertEqual(busy["noise_n"], "1")
        self.assertEqual(busy["noise_se"], "")
        self.assertEqual(busy["noise_sd"], "")

    def test_csv_round_trip_preserves_text_and_missing_noise(self):
        result = ab_split.split(log())
        result["config"] = 'run, "界"\r\nsecond line'
        result["metrics"][0]["noise"] = None
        rows = list(csv.DictReader(io.StringIO(ab_split_csv.render(result), newline="")))
        self.assertEqual(rows[0]["config"], result["config"])
        self.assertEqual(rows[0]["phases"], "4")
        self.assertTrue(all(rows[0]["noise_" + key] == "" for key in ("mean", "se", "sd", "n")))

    def test_real_cli_matches_original_json_and_has_portable_newlines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.txt"
            path.write_text("".join(log()), encoding="utf-8")
            tool = Path(ab_split_csv.__file__)
            command = [sys.executable, str(tool), str(path), "--program", "0x1000", "--class", "staging-in"]
            completed = subprocess.run(command, capture_output=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertNotIn(b"\r\r\n", completed.stdout)
            original = subprocess.run([sys.executable, str(Path(ab_split.__file__)), str(path), "--json", "--program", "0x1000", "--class", "staging-in"], capture_output=True)
            self.assertEqual(original.returncode, 0, original.stderr)
            expected = ab_split_csv.render(json.loads(original.stdout)).encode("utf-8")
            self.assertEqual(completed.stdout, expected)

    def test_bom_before_configuration_is_removed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.txt"
            lines = log()[1:]
            self.assertTrue(lines[0].startswith("[ab] in-run A/B:"))
            path.write_bytes(b"\xef\xbb\xbf" + "".join(lines).encode("utf-8"))
            completed = subprocess.run([sys.executable, str(Path(ab_split_csv.__file__)), str(path)], capture_output=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            expected = ab_split_csv.render(ab_split.split(lines)).encode("utf-8")
            self.assertEqual(completed.stdout, expected)

    def test_cli_preserves_producer_decoding_and_line_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.txt"
            for prefix in (b"guest stderr \xff\n", b"guest stderr \x0c", "guest stderr \u2028".encode("utf-8")):
                with self.subTest(prefix=prefix):
                    path.write_bytes(prefix + "".join(log()[1:]).encode("utf-8"))
                    completed = subprocess.run([sys.executable, str(Path(ab_split_csv.__file__)), str(path)], capture_output=True)
                    original = subprocess.run([sys.executable, str(Path(ab_split.__file__)), str(path), "--json"], capture_output=True)
                    self.assertEqual(original.returncode, 0, original.stderr)
                    self.assertEqual(completed.returncode, 0, completed.stderr)
                    expected = ab_split_csv.render(json.loads(original.stdout)).encode("utf-8")
                    self.assertEqual(completed.stdout, expected)

    def test_invalid_inputs_emit_no_csv(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "log.txt"
            for contents, arguments in ((None, []), (b"no phases\n", []), (b"unused", ["--program", "bad"])):
                if contents is not None:
                    path.write_bytes(contents)
                completed = subprocess.run([sys.executable, str(Path(ab_split_csv.__file__)), str(path), *arguments], capture_output=True)
                self.assertEqual(completed.returncode, 2, completed.stderr)
                self.assertEqual(completed.stdout, b"")
                self.assertNotIn(b"Traceback", completed.stderr)


if __name__ == "__main__":
    unittest.main()
