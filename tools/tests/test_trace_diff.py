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

    def test_threads_with_the_same_name_contribute_to_one_total(self):
        baseline = events(10000)
        candidate = baseline + [
            {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 3,
             "args": {"name": "queue worker 0x0"}},
            complete(3, "Driver.Packet", 0, 4000),
        ]
        result = self.diff(baseline, candidate)
        worker = next(row for row in result["threads"] if row["thread"] == "queue worker 0x0")
        self.assertEqual(worker["a_busy_ms_per_frame"], 10.0)
        self.assertEqual(worker["b_busy_ms_per_frame"], 14.0)

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


if __name__ == "__main__":
    unittest.main()
