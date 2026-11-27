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

    def test_markdown_escapes_thread_and_slice_names_without_changing_comparison(self):
        items = events(10000)
        items[0]["args"]["name"] = "worker|lane\nnext"
        items[2]["name"] = "draw|pass\nnext"
        result = self.diff(items, items)

        self.assertIn("worker|lane\nnext", [row["thread"] for row in result["threads"]])
        self.assertIn("draw|pass\nnext", [row["name"] for row in result["slices"]])
        rendered = trace_diff.markdown(result)
        self.assertIn("| worker&#124;lane<br>next |", rendered)
        self.assertIn("| draw&#124;pass<br>next |", rendered)

    def test_independent_windows_match_the_digest_of_the_same_scene(self):
        a = write_trace(self.tmp.name, "a.json.gz", events(10000))
        b = Path(self.tmp.name) / "b.json"
        shifted = [dict(event, ts=event["ts"] + 30000) for event in events(10000)]
        shifted += [dict(shifted[-2], ts=0, args={"present": 599}), complete(1, "warmup", 0, 25000),
                    complete(1, "crosses boundary", 25000, 10000), complete(1, "after the scene", 51000, 1000)]
        b.write_text(json.dumps(shifted), encoding="utf-8")
        done = subprocess.run([sys.executable, trace_diff.__file__, a, b, "--a-frames", "0:1", "--b-frames", "1:1", "--json"],
                              capture_output=True, text=True)
        self.assertEqual(done.returncode, 0, done.stderr)
        result = json.loads(done.stdout)
        self.assertEqual(result["frames"], {"a": 1, "b": 1})
        self.assertEqual(result["windows"], {"a": {"start": 0, "count": 1, "start_ms": 0.0, "end_ms": 20.0},
                                              "b": {"start": 1, "count": 1, "start_ms": 30.0, "end_ms": 50.0}})
        self.assertEqual(result["event_loss"], {"a": [], "b": []})
        slices = {row["name"]: row for row in result["slices"]}
        self.assertEqual(slices["Driver.Packet"]["delta_ms_per_frame"], 0.0)
        self.assertEqual(slices["crosses boundary"]["b_ms_per_frame"], 5.0)
        self.assertNotIn("warmup", slices)
        self.assertNotIn("after the scene", slices)

    def test_window_keeps_loss_markers_and_the_recorded_prefix(self):
        a_items = events(10000)
        a_items += [dict(a_items[-1], ts=40000, args={"present": 602}),
                    {"name": "trace buffer full", "ph": "i", "ts": 21000, "tid": 1, "args": {"events_lost": 3}}]
        a = write_trace(self.tmp.name, "a.json.gz", a_items)
        b = write_trace(self.tmp.name, "b.json.gz", events(10000))
        result = trace_diff.compare(a, b, a_frames=(0, 2))
        self.assertEqual(result["frames"], {"a": 2, "b": 1})
        self.assertIsNone(result["windows"]["b"])
        self.assertEqual(result["event_loss"]["b"], [])
        loss, = result["event_loss"]["a"]
        self.assertEqual(loss["events_lost"], 3)
        self.assertEqual(loss["frames_before_full"], 1)
        self.assertEqual(loss["recorded_ms"], 20.0)
        rows = {row["name"]: row for row in result["slices"]}
        self.assertEqual(rows["Driver.Packet"]["a_ms_per_frame"], 5.0)
        rendered = trace_diff.markdown(result)
        self.assertIn("A window: 0:2 (0 to 40 ms in the capture)", rendered)
        self.assertIn("B window: whole capture", rendered)
        self.assertIn("3 events lost", rendered)

    def test_whole_captures_keep_loss_markers_and_warn_for_either_side(self):
        for a_lost, b_lost in ((3, 0), (0, 300), (3, 300)):
            with self.subTest(a_lost=a_lost, b_lost=b_lost):
                captures = []
                for draw_us, lost in ((10000, a_lost), (5000, b_lost)):
                    items = events(draw_us)
                    if lost:
                        items.append({"name": "trace buffer full", "ph": "i", "ts": 5000,
                                      "tid": 1, "args": {"events_lost": lost}})
                    captures.append(items)
                result = self.diff(*captures)
                self.assertNotIn("windows", result)
                rendered = trace_diff.markdown(result)
                for side, lost in (("a", a_lost), ("b", b_lost)):
                    self.assertEqual([loss["events_lost"] for loss in result["event_loss"][side]],
                                     [lost] if lost else [])
                    if lost:
                        self.assertIn(f"- {side.upper()} capture:\n  - queue worker 0x0: {lost} events lost; "
                                      "buffer full at 5 ms in the capture.", rendered)
                self.assertEqual(rendered.count("Timings describe retained events only."), 1)
                packet = next(row for row in result["slices"] if row["name"] == "Driver.Packet")
                self.assertEqual(packet["delta_ms_per_frame"], -5.0)

    def test_whole_capture_cli_reports_loss_in_json_and_markdown(self):
        a = write_trace(self.tmp.name, "a.json.gz", events(10000))
        b = write_trace(self.tmp.name, "b.json.gz", events(5000) + [
            {"name": "trace buffer full", "ph": "i", "ts": 5000, "tid": 1, "args": {"events_lost": 300}}
        ])
        for options in ([], ["--json"]):
            with self.subTest(options=options):
                done = subprocess.run([sys.executable, trace_diff.__file__, a, b, *options],
                                      capture_output=True, text=True)
                self.assertEqual(done.returncode, 0, done.stderr)
                if options:
                    result = json.loads(done.stdout)
                    self.assertEqual(result["event_loss"]["a"], [])
                    self.assertEqual(result["event_loss"]["b"][0]["events_lost"], 300)
                else:
                    self.assertIn("B capture:", done.stdout)
                    self.assertIn("300 events lost", done.stdout)
                    self.assertIn("Timings describe retained events only.", done.stdout)

    def test_window_requires_real_present_bounds(self):
        a = write_trace(self.tmp.name, "a.json.gz", events(10000))
        b = write_trace(self.tmp.name, "b.json.gz", events(10000))
        for window, message in (("1:1", "complete frame count of 1"), ("0:0", "positive count"), ("1", "START:COUNT")):
            with self.subTest(window=window):
                done = subprocess.run([sys.executable, trace_diff.__file__, a, b, "--a-frames", window],
                                      capture_output=True, text=True)
                self.assertEqual(done.returncode, 2)
                self.assertIn(message, done.stderr)
                self.assertIn("error:", done.stderr)
                self.assertNotIn("Traceback", done.stderr)
                self.assertEqual(done.stdout, "")

    def test_windowed_comparison_needs_a_complete_frame_in_both_captures(self):
        a = write_trace(self.tmp.name, "a.json.gz", events(10000))
        b = write_trace(self.tmp.name, "b.json.gz", events(10000)[:-1])
        with self.assertRaisesRegex(ValueError, "needs at least one complete frame in each capture"):
            trace_diff.compare(a, b, a_frames=(0, 1))

    def test_whole_capture_diff_rejects_unframed_timings(self):
        items = events(8000)
        markers = [item for item in items if item["name"] == "present"]
        work = [item for item in items if item["name"] != "present"]
        framed = write_trace(self.tmp.name, "whole.json.gz", items)
        for presents in (0, 1):
            unframed = write_trace(self.tmp.name, "unframed.json.gz", work + markers[:presents])
            for a, b, options in ((unframed, framed, []), (framed, unframed, ["--json"])):
                with self.subTest(presents=presents, a=a):
                    result = subprocess.run([sys.executable, trace_diff.__file__, a, b, *options],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 2, result.stdout)
                    self.assertEqual(result.stdout, "")
                    self.assertIn(str(unframed), result.stderr)
                    self.assertIn("complete frame", result.stderr)

    def test_identical_traces_have_zero_deltas(self):
        result = self.diff(events(10000), events(10000))
        self.assertEqual(set(result), {"a", "b", "frames", "threads", "slices"})
        self.assertTrue(trace_diff.markdown(result).startswith("Frames: A 1, B 1\n\n| thread"))
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
