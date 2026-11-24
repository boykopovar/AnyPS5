import argparse
import gzip
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import trace_diff


def complete(tid, name, ts, dur):
    return {"name": name, "ph": "X", "ts": ts, "dur": dur, "pid": 7, "tid": tid}


def events(draw_us, extra_thread=False):
    items = [
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {"name": "queue worker 0x0"}},
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 2, "args": {"name": "presenter"}},
        complete(1, "Driver.Packet", 0, draw_us),
        complete(2, "Driver.Present", 0, 2000),
        {"name": "present", "ph": "i", "ts": 0, "pid": 7, "tid": 2, "s": "g", "args": {"present": 600}},
        {"name": "present", "ph": "i", "ts": 20000, "pid": 7, "tid": 2, "s": "g", "args": {"present": 601}},
    ]
    if extra_thread:
        items.insert(2, {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 3, "args": {"name": "gpu"}})
        items.append(complete(3, "Vulkan.Submit", 0, 1000))
    return items


def stepped(overrides):
    items = [
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {"name": "queue worker 0x0"}},
        {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 2, "args": {"name": "presenter"}},
        {"name": "present", "ph": "i", "ts": 0, "pid": 7, "tid": 2, "s": "g", "args": {"present": 600}},
        {"name": "present", "ph": "i", "ts": 100000, "pid": 7, "tid": 2, "s": "g", "args": {"present": 601}},
    ]
    ts = 0
    for index in range(11):
        dur_us = round(overrides.get(index, 10.0 - 0.5 * index) * 1000)
        items.append(complete(1, f"S{index:02d}", ts, dur_us))
        ts += dur_us
    return items


def write_trace(directory, name, items):
    path = Path(directory) / name
    with gzip.open(path, "wt", encoding="utf-8") as handle:
        json.dump({"traceEvents": items}, handle)
    return path


class DiffTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def diff(self, a_items, b_items):
        a = write_trace(self.tmp.name, "a.json.gz", a_items)
        b = write_trace(self.tmp.name, "b.json.gz", b_items)
        return trace_diff.compare(a, b, top=10)

    def test_identical_traces_have_zero_deltas(self):
        result = self.diff(events(10000), events(10000))
        for row in result["threads"]:
            self.assertAlmostEqual(row["busy_delta_ms_per_frame"], 0.0)
            self.assertAlmostEqual(row["wait_delta_ms_per_frame"], 0.0)
        for row in result["slices"]:
            self.assertAlmostEqual(row["delta_ms_per_frame"], 0.0)

    def test_busier_side_shows_a_positive_delta(self):
        result = self.diff(events(10000), events(16000))
        packet = next(row for row in result["slices"] if row["name"] == "Driver.Packet")
        self.assertGreater(packet["delta_ms_per_frame"], 0.0)
        self.assertAlmostEqual(packet["b_ms_per_frame"] - packet["a_ms_per_frame"], packet["delta_ms_per_frame"])

    def test_a_thread_present_on_one_side_counts_as_zero_on_the_other(self):
        result = self.diff(events(10000), events(10000, extra_thread=True))
        gpu = next(row for row in result["threads"] if row["thread"] == "gpu")
        self.assertEqual(gpu["a_busy_ms_per_frame"], 0.0)
        self.assertGreater(gpu["b_busy_ms_per_frame"], 0.0)
        self.assertEqual(gpu["busy_delta_ms_per_frame"], gpu["b_busy_ms_per_frame"])

    def test_a_slice_below_the_top_cut_on_one_side_keeps_its_real_delta(self):
        result = self.diff(stepped({}), stepped({9: 4.0, 10: 9.8}))
        rows = {row["name"]: row for row in result["slices"]}
        self.assertAlmostEqual(rows["S10"]["a_ms_per_frame"], 5.0)
        self.assertAlmostEqual(rows["S10"]["b_ms_per_frame"], 9.8)
        self.assertAlmostEqual(rows["S10"]["delta_ms_per_frame"], 4.8)
        self.assertAlmostEqual(rows["S09"]["a_ms_per_frame"], 5.5)
        self.assertAlmostEqual(rows["S09"]["b_ms_per_frame"], 4.0)
        self.assertAlmostEqual(rows["S09"]["delta_ms_per_frame"], -1.5)

    def test_the_cli_writes_json_and_rejects_a_missing_file(self):
        a = write_trace(self.tmp.name, "a.json.gz", events(10000))
        b = write_trace(self.tmp.name, "b.json.gz", events(12000))
        tool = Path(trace_diff.__file__)
        done = subprocess.run([sys.executable, str(tool), str(a), str(b), "--json"], capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        self.assertIn("threads", json.loads(done.stdout))
        missing = subprocess.run([sys.executable, str(tool), str(a), str(Path(self.tmp.name) / "none.json.gz")], capture_output=True, text=True)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("FAIL", missing.stderr)


class MarkdownAndIntegerTests(unittest.TestCase):
    def result(self):
        return {
            "frames": {"a": 3, "b": 4},
            "threads": [{"thread": "presenter", "a_busy_ms_per_frame": 1.5, "b_busy_ms_per_frame": 0.25,
                         "busy_delta_ms_per_frame": -1.25, "a_wait_ms_per_frame": 0.0,
                         "b_wait_ms_per_frame": 2.0, "wait_delta_ms_per_frame": 2.0}],
            "slices": [{"name": "Driver.Present", "a_ms_per_frame": 0.5, "b_ms_per_frame": 0.75, "delta_ms_per_frame": 0.25}],
        }

    def test_markdown_renders_frames_threads_and_slices(self):
        text = trace_diff.markdown(self.result())
        self.assertEqual(text.splitlines()[0], "Frames: A 3, B 4")
        self.assertIn("| presenter | 1.500 | 0.250 | -1.250 | 0.000 | 2.000 | +2.000 |", text.splitlines())
        self.assertIn("| Driver.Present | 0.500 | 0.750 | +0.250 |", text.splitlines())
        self.assertTrue(text.endswith("\n"))

    def test_markdown_header_rows_match_the_column_count(self):
        lines = trace_diff.markdown(self.result()).splitlines()
        self.assertEqual(lines[2].count("|"), lines[3].count("|"))
        self.assertEqual(lines[6].count("|"), lines[7].count("|"))

    def test_nonnegative_integer_accepts_zero_and_positive_values(self):
        self.assertEqual(trace_diff.nonnegative_integer("0"), 0)
        self.assertEqual(trace_diff.nonnegative_integer("42"), 42)

    def test_nonnegative_integer_rejects_negative_and_non_numbers(self):
        for value in ("-1", "abc", "1.5", ""):
            with self.subTest(value=value):
                with self.assertRaises(argparse.ArgumentTypeError):
                    trace_diff.nonnegative_integer(value)


if __name__ == "__main__":
    unittest.main()
