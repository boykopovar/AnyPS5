import gzip
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import trace_digest
import trace_timeline


def complete(tid, name, ts, dur):
    return {"name": name, "ph": "X", "ts": ts, "dur": dur, "pid": 7, "tid": tid}


def document(items):
    names = [
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {"name": "queue worker"}},
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 2, "args": {"name": "presenter"}},
    ]
    return {"traceEvents": names + items}


def trace_of(items):
    return trace_digest.Trace(document(items))


class TimelineTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.items = [
            complete(1, "Driver.Packet", 0, 4000),
            complete(1, "Driver.Packet", 5000, 2000),
            complete(2, "Driver.Present", 3000, 6000),
        ]

    def test_window_clips_slices_to_the_range(self):
        slices = trace_timeline.window_slices(trace_of(self.items), 2000, 6000)
        durations = sorted(slice_["dur"] for slice_ in slices)
        self.assertEqual(durations, [1000, 2000, 3000])

    def test_window_keeps_only_the_selected_threads(self):
        slices = trace_timeline.window_slices(trace_of(self.items), 0, 10000, threads=["presenter"])
        self.assertEqual([slice_["name"] for slice_ in slices], ["Driver.Present"])

    def test_layout_places_bars_and_gives_tiny_slices_a_pixel(self):
        items = [complete(1, "Driver.Packet", 0, 5000), complete(1, "Tiny", 6000, 1)]
        laid = trace_timeline.layout(trace_timeline_slices(items), 0, 10000, 1000)
        first = next(bar for bar in laid["bars"] if bar["name"] == "Driver.Packet")
        tiny = next(bar for bar in laid["bars"] if bar["name"] == "Tiny")
        self.assertAlmostEqual(first["x"], trace_timeline.LABEL_WIDTH)
        self.assertAlmostEqual(first["w"], 500.0)
        self.assertEqual(tiny["w"], 1.0)

    def test_layout_orders_rows_by_thread_name(self):
        laid = trace_timeline.layout(trace_timeline_slices(self.items), 0, 10000, 1000)
        self.assertEqual(laid["threads"], ["presenter", "queue worker"])

    def test_layout_keeps_the_longest_slices_and_counts_the_rest(self):
        laid = trace_timeline.layout(trace_timeline_slices(self.items), 0, 10000, 1000, max_slices=1)
        self.assertEqual(len(laid["bars"]), 1)
        self.assertEqual(laid["bars"][0]["ms"], 6.0)
        self.assertEqual(laid["left_out"], 2)

    def test_svg_escapes_slice_names(self):
        laid = trace_timeline.layout(trace_timeline_slices([complete(1, "<script>", 0, 1000)]), 0, 2000, 1000)
        text = trace_timeline.svg(laid)
        self.assertIn("&lt;script&gt;", text)
        self.assertNotIn("<script>", text)

    def test_the_cli_writes_html_and_rejects_a_missing_file(self):
        path = Path(self.tmp.name) / "a.json.gz"
        with gzip.open(path, "wt", encoding="utf-8") as handle:
            json.dump(document(self.items), handle)
        out = Path(self.tmp.name) / "a.html"
        tool = Path(trace_timeline.__file__)
        done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out)], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("<svg", out.read_text(encoding="utf-8"))
        missing = subprocess.run([sys.executable, str(tool), str(Path(self.tmp.name) / "none.json.gz")], capture_output=True, text=True)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("FAIL", missing.stderr)

    def test_the_cli_names_a_non_ascii_output_on_a_cp1252_console(self):
        path = Path(self.tmp.name) / "a.json.gz"
        with gzip.open(path, "wt", encoding="utf-8") as handle:
            json.dump(document(self.items), handle)
        out = Path(self.tmp.name) / "日本.html"
        env = {**os.environ, "PYTHONIOENCODING": "cp1252", "PYTHONUTF8": "0"}
        done = subprocess.run([sys.executable, "-X", "utf8=0", trace_timeline.__file__, str(path), "-o", str(out)],
                              env=env, capture_output=True)
        self.assertEqual(done.returncode, 0, done.stderr.decode("utf-8", errors="replace"))
        self.assertIn(f"wrote {out}", done.stdout.decode("utf-8"))

    def test_the_cli_accepts_only_finite_nonnegative_window_bounds(self):
        path = Path(self.tmp.name) / "a.json.gz"
        with gzip.open(path, "wt", encoding="utf-8") as handle:
            json.dump(document(self.items), handle)
        out = Path(self.tmp.name) / "a.html"
        tool = Path(trace_timeline.__file__)
        done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out), "--from-ms=1", "--to-ms=6.5"],
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("window 1.000 to 6.500 ms", out.read_text(encoding="utf-8"))
        for option in ("--from-ms", "--to-ms"):
            for value in ("-1", "nan", "inf", "-inf", "1e308"):
                with self.subTest(option=option, value=value):
                    out.unlink(missing_ok=True)
                    done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out), option + "=" + value],
                                          capture_output=True, text=True)
                    self.assertEqual(done.returncode, 2, done.stderr)
                    self.assertIn(option, done.stderr)
                    self.assertFalse(out.exists())

    def test_the_cli_rejects_sizes_that_are_not_positive(self):
        path = Path(self.tmp.name) / "a.json.gz"
        with gzip.open(path, "wt", encoding="utf-8") as handle:
            json.dump(document(self.items), handle)
        out = Path(self.tmp.name) / "a.html"
        tool = Path(trace_timeline.__file__)
        for option, value in (("--max-slices", "-1"), ("--max-slices", "0"), ("--width", "-500"), ("--width", "0"), ("--width", "x")):
            with self.subTest(option=option, value=value):
                out.unlink(missing_ok=True)
                done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out), option + "=" + value],
                                      capture_output=True, text=True)
                self.assertEqual(done.returncode, 2, done.stderr)
                self.assertIn(option, done.stderr)
                self.assertFalse(out.exists())


def trace_timeline_slices(items):
    return trace_timeline.window_slices(trace_of(items), 0, 10**12)


if __name__ == "__main__":
    unittest.main()
