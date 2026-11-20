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

    def test_cli_thread_filter_preserves_the_default_window_end(self):
        path = Path(self.tmp.name) / "capture.json"
        path.write_text(json.dumps(document(self.items)), encoding="utf-8")
        for start, expected in ((0, "window 0.000 to 9.000 ms, 1 threads, 2 bars"),
                                (8, "window 8.000 to 9.000 ms, 0 threads, 0 bars")):
            with self.subTest(start=start):
                out = path.with_name(f"capture-{start}.html")
                done = subprocess.run([sys.executable, trace_timeline.__file__, str(path), "-o", str(out),
                                       "--thread", "queue worker", "--from-ms", str(start)],
                                      capture_output=True, text=True, timeout=5)
                self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
                page = out.read_text(encoding="utf-8")
                self.assertIn(expected, page)
                self.assertNotIn("Driver.Present", page)

    def test_cli_default_window_includes_events_after_the_last_slice(self):
        trailing_events = [
            {"name": "present", "ph": "i", "ts": 12000, "tid": 2, "args": {"present": 1}},
            {"name": "trace buffer full", "ph": "i", "ts": 13000, "tid": 2, "args": {"events_lost": 8}},
            {"name": "presenter folded", "ph": "C", "ts": 14000, "tid": 2, "args": {"ms": 2}},
        ]
        path = Path(self.tmp.name) / "capture.json"
        out = path.with_suffix(".html")
        for event in trailing_events:
            with self.subTest(event=event["name"]):
                path.write_text(json.dumps(document(self.items + [event])), encoding="utf-8")
                done = subprocess.run([sys.executable, trace_timeline.__file__, str(path), "-o", str(out),
                                       "--thread", "queue worker", "--from-ms", "10"],
                                      capture_output=True, text=True, timeout=5)
                self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
                window_end_ms = event["ts"] / 1000
                self.assertIn(f"window 10.000 to {window_end_ms:.3f} ms, 0 threads, 0 bars", out.read_text(encoding="utf-8"))

    def test_cli_reports_event_loss_in_plain_and_compressed_captures(self):
        capture = document(self.items + [
            {"name": "thread_name", "ph": "M", "tid": 3, "args": {"name": "<worker&>"}},
            {"name": "trace buffer full", "ph": "i", "ts": 1500, "tid": 1, "args": {"events_lost": 8}},
            {"name": "trace buffer full", "ph": "i", "ts": 8000, "tid": 3, "args": {"events_lost": 3}},
        ])
        for compressed in (False, True):
            with self.subTest(compressed=compressed):
                path = Path(self.tmp.name) / ("capture.json.gz" if compressed else "capture.json")
                opener = gzip.open if compressed else open
                with opener(path, "wt", encoding="utf-8") as handle:
                    json.dump(capture, handle)
                before = path.read_bytes()
                out = Path(self.tmp.name) / "capture.html"
                done = subprocess.run([sys.executable, trace_timeline.__file__, str(path), "-o", str(out), "--max-slices", "1"],
                                      capture_output=True, text=True, timeout=5)
                self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
                text = out.read_text(encoding="utf-8")
                self.assertIn("Incomplete trace: 11 events lost", text)
                self.assertIn("Gaps may contain unrecorded work", text)
                self.assertIn("queue worker (tid 1): buffer full at 1.500 ms; 8 events lost across the capture", text)
                self.assertIn("&lt;worker&amp;&gt; (tid 3): buffer full at 8.000 ms; 3 events lost across the capture", text)
                self.assertNotIn("<worker&>", text)
                self.assertIn("2 shorter slices left out by --max-slices", text)
                self.assertEqual(path.read_bytes(), before)

    def test_cli_loss_warning_respects_threads_and_covers_the_capture(self):
        for lost, thread, bounds, expected in ((8, "queue worker", [], True), (8, "presenter", [], False),
                                                (0, None, [], False), (8, None, ["--from-ms", "2", "--to-ms", "6"], True)):
            with self.subTest(lost=lost, thread=thread, bounds=bounds):
                path = Path(self.tmp.name) / "capture.json"
                path.write_text(json.dumps(document(self.items + [
                    {"name": "trace buffer full", "ph": "i", "ts": 1500, "tid": 1, "args": {"events_lost": lost}},
                ])), encoding="utf-8")
                out = Path(self.tmp.name) / "capture.html"
                options = ["--thread", thread] if thread else []
                done = subprocess.run([sys.executable, trace_timeline.__file__, str(path), "-o", str(out), *options, *bounds],
                                      capture_output=True, text=True, timeout=5)
                self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
                text = out.read_text(encoding="utf-8")
                self.assertEqual("Incomplete trace:" in text, expected)
                self.assertEqual("events lost across the capture" in text, expected)

    def test_cli_reports_loss_on_a_thread_without_retained_slices(self):
        path = Path(self.tmp.name) / "capture.json"
        path.write_text(json.dumps([
            {"name": "trace buffer full", "ph": "i", "ts": 1500, "tid": 9, "args": {"events_lost": 8}},
        ]), encoding="utf-8")
        out = Path(self.tmp.name) / "capture.html"
        done = subprocess.run([sys.executable, trace_timeline.__file__, str(path), "-o", str(out), "--thread", "tid 9"],
                              capture_output=True, text=True, timeout=5)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        text = out.read_text(encoding="utf-8")
        self.assertIn("Incomplete trace: 8 events lost", text)
        self.assertIn("tid 9: buffer full at 1.500 ms; 8 events lost across the capture", text)
        self.assertIn("window 0.000 to 1.500 ms, 0 threads, 0 bars", text)

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

    def test_threads_that_share_a_name_get_their_own_rows(self):
        names = [{"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": tid, "args": {"name": "shader prepare"}} for tid in (1, 2)]
        items = names + [complete(tid, "Shader.Prepare", 0, 8000) for tid in (1, 2)]
        laid = trace_timeline.layout(trace_timeline.window_slices(trace_digest.Trace(items), 0, 10000), 0, 10000, 1000)
        self.assertEqual(laid["threads"], ["shader prepare", "shader prepare"])
        self.assertEqual(sorted(bar["y"] for bar in laid["bars"]), [0, trace_timeline.ROW_HEIGHT])
        path = Path(self.tmp.name) / "a.json"
        path.write_text(json.dumps(items), encoding="utf-8")
        out = Path(self.tmp.name) / "a.html"
        done = subprocess.run([sys.executable, str(Path(trace_timeline.__file__)), str(path), "-o", str(out), "--thread", "shader prepare"],
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        text = out.read_text(encoding="utf-8")
        self.assertIn("2 threads, 2 bars", text)
        self.assertEqual(text.count(">shader prepare</text>"), 2)

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

    def test_the_cli_rejects_a_window_ending_before_it_starts(self):
        path = Path(self.tmp.name) / "capture.json"
        path.write_text(json.dumps(document(self.items)), encoding="utf-8")
        tool = Path(trace_timeline.__file__)
        for label, bounds in (
            ("explicit", ("--from-ms=6", "--to-ms=2")),
            ("default", ("--from-ms=10",)),
        ):
            with self.subTest(label=label):
                out = path.with_name(f"{label}.html")
                done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out), *bounds],
                                      capture_output=True, text=True)
                self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
                self.assertIn("window end must be at or after its start", done.stderr)
                self.assertFalse(out.exists())

        out = path.with_name("equal.html")
        done = subprocess.run([sys.executable, str(tool), str(path), "-o", str(out), "--from-ms=9", "--to-ms=9"],
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("window 9.000 to 9.000 ms", out.read_text(encoding="utf-8"))

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

    def test_the_cli_reports_a_non_ascii_output_path_as_utf8(self):
        directory = Path(self.tmp.name) / "界"
        directory.mkdir()
        path = directory / "a.json"
        path.write_text(json.dumps(document(self.items)), encoding="utf-8")
        out = directory / "a.html"
        environment = dict(os.environ, PYTHONIOENCODING="cp1252")
        done = subprocess.run([sys.executable, str(Path(trace_timeline.__file__)), str(path), "-o", str(out)],
                              capture_output=True, env=environment)
        self.assertEqual(done.returncode, 0, done.stderr.decode("utf-8", "replace"))
        self.assertEqual(done.stdout.decode("utf-8"), f"wrote {out}\n".replace("\n", os.linesep))
        self.assertIn("<svg", out.read_text(encoding="utf-8"))


    def check_input_alias(self, path, output=None, cwd=None):
        before = path.read_bytes()
        arguments = [sys.executable, str(Path(trace_timeline.__file__)), str(path)]
        if output is not None:
            arguments += ["-o", str(output)]
        done = subprocess.run(arguments, capture_output=True, text=True, cwd=cwd)
        self.assertEqual(done.returncode, 2, done.stdout + done.stderr)
        self.assertIn("output must not overwrite the input trace", done.stderr)
        self.assertNotIn("wrote ", done.stdout)
        self.assertEqual(path.read_bytes(), before)

    def test_cli_preserves_input_with_explicit_or_default_same_output(self):
        for suffix in (".json", ".html"):
            with self.subTest(suffix=suffix):
                path = Path(self.tmp.name) / ("capture" + suffix)
                path.write_text(json.dumps(document(self.items)), encoding="utf-8")
                self.check_input_alias(path, path)
                self.check_input_alias(path, Path(os.path.relpath(path, path.parent)), cwd=path.parent)
                if suffix == ".html":
                    self.check_input_alias(path)

    def test_cli_preserves_compressed_input_with_hardlink_output(self):
        path = Path(self.tmp.name) / "capture.json.gz"
        with gzip.open(path, "wt", encoding="utf-8") as handle:
            json.dump(document(self.items), handle)
        output = Path(self.tmp.name) / "capture.html"
        try:
            os.link(path, output)
        except OSError as error:
            self.skipTest(f"hard links unavailable: {error}")
        self.check_input_alias(path, output)

    def test_cli_preserves_input_with_symlink_alias_in_either_direction(self):
        path = Path(self.tmp.name) / "capture.json"
        path.write_text(json.dumps(document(self.items)), encoding="utf-8")
        alias = Path(self.tmp.name) / "capture.html"
        try:
            alias.symlink_to(path)
        except OSError as error:
            self.skipTest(f"symbolic links unavailable: {error}")
        self.check_input_alias(path, alias)
        self.check_input_alias(alias, path)

    def test_cli_allows_distinct_existing_output_with_identical_contents(self):
        path = Path(self.tmp.name) / "capture.json"
        path.write_text(json.dumps(document(self.items)), encoding="utf-8")
        before = path.read_bytes()
        output = path.with_suffix(".html")
        output.write_bytes(before)
        done = subprocess.run([sys.executable, str(Path(trace_timeline.__file__)), str(path), "-o", str(output)],
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertEqual(path.read_bytes(), before)
        self.assertIn("<svg", output.read_text(encoding="utf-8"))


def trace_timeline_slices(items):
    return trace_timeline.window_slices(trace_of(items), 0, 10**12)


if __name__ == "__main__":
    unittest.main()
