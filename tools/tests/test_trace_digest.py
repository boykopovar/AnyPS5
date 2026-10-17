import contextlib
import gzip
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import trace_digest


def complete(tid, name, ts, dur):
    return {"name": name, "ph": "X", "ts": ts, "dur": dur, "pid": 7, "tid": tid}


EVENTS = [
    {"name": "process_name", "ph": "M", "ts": 0, "pid": 7, "tid": 0, "args": {"name": "AnyPS5"}},
    {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {"name": "queue worker 0x0"}},
    {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 2, "args": {"name": "presenter"}},
    complete(1, "Driver.WorkerWait", 0, 4000),
    complete(1, "Driver.Packet", 4000, 5000),
    complete(1, "Driver.Packet.memory_wait", 5000, 2000),
    complete(1, "Driver.Packet", 12000, 16000),
    complete(1, "Driver.Packet.draw", 13000, 7000),
    complete(2, "Driver.Present", 0, 2000),
    complete(2, "Driver.Present.inflight_wait", 500, 1000),
    complete(2, "Driver.Present", 10000, 16000),
    complete(2, "Driver.Present.inflight_wait", 10500, 14500),
    {"name": "queue worker 0x0 folded", "ph": "C", "ts": 0, "pid": 7, "tid": 1, "args": {"ms": 1.5}},
    {"name": "present", "ph": "i", "ts": 0, "pid": 7, "tid": 2, "s": "g", "args": {"present": 600}},
    {"name": "present", "ph": "i", "ts": 10000, "pid": 7, "tid": 2, "s": "g", "args": {"present": 601}},
    {"name": "present", "ph": "i", "ts": 30000, "pid": 7, "tid": 2, "s": "g", "args": {"present": 602}},
]

SUMMARY = {"commit": "abc", "dirty": False, "env": {"APS5_TRACE_FRAMES": "600:3"}, "gpu": "GPU", "driver": "Driver 1", "os": "OS 1",
           "resolution": "1920x1080", "window": {"spec": "frames 600:3", "first_present": 600, "ms": 40.0}, "presents": 3,
           "frame_ms": {"p50": 10.0, "p90": 20.0, "p99": 20.0, "max": 20.0}, "gpu_busy_ms_per_present": 5.0,
           "worker_busy_pct": {"queue worker 0x0": 50.0}, "top_phases_ms_per_frame": {"Driver.Packet": 7.0}, "trace_dropped": False,
           "trace": "aps5-trace-abc-7.json.gz"}

STACKS = "# samples=5 depth=64 truncated=1\n# module a.prx 0x1000 0x1\nntdll.dll+0x1;a.prx+0x10;a.prx+0x20 3\nntdll.dll+0x1;a.prx+0x10;a.prx+0x30 1\n[truncated];a.prx+0x40;a.prx+0x30 1\n"


class TraceDigestTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        root = Path(self.directory.name)
        self.trace = root / "aps5-trace-abc-7.json.gz"
        with gzip.open(self.trace, "wt", encoding="utf-8") as handle:
            json.dump({"traceEvents": EVENTS}, handle)
        (root / "aps5-summary.jsonl").write_text(json.dumps(dict(SUMMARY, trace="other.json.gz")) + "\n" + json.dumps(SUMMARY) + "\n", encoding="utf-8")
        self.stacks = root / "worker.collapsed.txt"
        self.stacks.write_text(STACKS, encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def test_folded_only_threads_are_reported_in_json_and_markdown(self):
        baseline = {thread["thread"]: thread for thread in self.result()["threads"]}
        for name, total in (("shader compiler", 6.0), ("tid 3", 6.0), ("shader compiler", 0.0)):
            with self.subTest(name=name, total=total):
                events = EVENTS + [
                    {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 3, "args": {"name": name}},
                    {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 4, "args": {"name": "idle"}},
                    {"name": f"{name} folded", "ph": "C", "ts": 0, "pid": 7, "tid": 3, "args": {"ms": total / 2}},
                    {"name": f"{name} folded", "ph": "C", "ts": 20000, "pid": 7, "tid": 3, "args": {"ms": total / 2}},
                ]
                with gzip.open(self.trace, "wt", encoding="utf-8") as handle:
                    json.dump({"traceEvents": events}, handle)
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(trace_digest.main([str(self.trace), "--json"]), 0)
                result = json.loads(output.getvalue())
                threads = {thread["thread"]: thread for thread in result["threads"]}
                self.assertEqual(set(threads), set(baseline) | {name})
                self.assertEqual(threads[name]["folded_ms_per_frame"], total / 3)
                self.assertEqual(threads[name]["busy_ms_per_frame"], 0)
                self.assertEqual(threads[name]["wait_ms_per_frame"], 0)
                self.assertEqual(threads[name]["top"], [])
                for thread_name, thread in baseline.items():
                    self.assertEqual(threads[thread_name], thread)
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    trace_digest.main([str(self.trace)])
                self.assertIn(f"| {name} | 0.0 (0 %) | 0.0 (0 %) |", output.getvalue())

    def test_folded_only_thread_without_frame_boundaries(self):
        trace = trace_digest.Trace({"traceEvents": [
            {"name": "compiler folded", "ph": "C", "ts": 0, "tid": 3, "args": {"ms": 1.5}},
        ]})
        thread = trace_digest.digest(trace)["threads"][0]
        self.assertEqual(thread["thread"], "tid 3")
        self.assertEqual(thread["folded_ms_per_frame"], 1.5)
        self.assertEqual(thread["untraced_ms_per_frame"], 0)

    def result(self):
        return trace_digest.digest(trace_digest.load_trace(self.trace), trace_digest.load_summary(None, self.trace),
                                   trace_digest.load_stacks(self.stacks), worst=2, top=3)

    def test_summary_is_the_line_of_this_trace(self):
        self.assertEqual(self.result()["summary"]["trace"], "aps5-trace-abc-7.json.gz")

    def test_decompressing_trace_preserves_the_complete_command_line_digest(self):
        uncompressed = self.trace.with_suffix("")
        uncompressed.write_bytes(gzip.decompress(self.trace.read_bytes()))
        results = []
        for trace in (self.trace, uncompressed):
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(trace_digest.main([str(trace), "--json"]), 0)
            results.append(json.loads(output.getvalue()))
        self.assertEqual((results[0]["frames"], results[0]["window_ms"]), (3, 40.0))
        self.assertEqual(results[1], results[0])

    def test_compressed_trace_can_use_a_summary_named_for_its_plain_copy(self):
        summary = dict(SUMMARY, trace=self.trace.with_suffix("").name)
        path = self.trace.with_name("named-summary.jsonl")
        path.write_text(json.dumps(summary) + "\n", encoding="utf-8")
        self.assertEqual(trace_digest.load_summary(path, self.trace), summary)

    def test_exact_trace_filename_wins_over_a_later_compression_alias(self):
        uncompressed = self.trace.with_suffix("")
        exact = dict(SUMMARY, trace=uncompressed.name, commit="exact")
        alias = dict(SUMMARY, commit="alias")
        path = self.trace.with_name("aps5-summary.jsonl")
        path.write_text(json.dumps(exact) + "\n" + json.dumps(alias) + "\n", encoding="utf-8")
        self.assertEqual(trace_digest.load_summary(None, uncompressed), exact)
        self.assertEqual(trace_digest.load_summary(None, self.trace), alias)

    def test_decompressed_trace_uses_the_latest_matching_alias_and_skips_other_traces(self):
        uncompressed = self.trace.with_suffix("")
        latest = dict(SUMMARY, commit="latest")
        unrelated = dict(SUMMARY, trace="other.json.gz", commit="other")
        path = self.trace.with_name("aps5-summary.jsonl")
        path.write_text("\n".join(json.dumps(line) for line in (SUMMARY, latest, unrelated)) + "\n", encoding="utf-8")
        self.assertEqual(trace_digest.load_summary(None, uncompressed), latest)

    def test_explicit_summary_selects_the_matching_trace(self):
        summary = Path(self.directory.name) / "relocated-summary.jsonl"
        other = dict(SUMMARY, trace="other.json.gz", presents=99)
        summary.write_text(json.dumps(SUMMARY) + "\n" + json.dumps(other) + "\n", encoding="utf-8")
        selected = trace_digest.load_summary(summary, self.trace)
        self.assertEqual(selected["trace"], self.trace.name)
        self.assertEqual(selected["presents"], 3)

    def test_explicit_summary_without_matching_trace_is_not_used(self):
        summary = Path(self.directory.name) / "unrelated-summary.jsonl"
        summary.write_text(json.dumps(dict(SUMMARY, trace="other.json.gz")) + "\n", encoding="utf-8")
        self.assertIsNone(trace_digest.load_summary(summary, self.trace))

    def test_frames_end_at_the_window(self):
        result = self.result()
        self.assertEqual(result["frames"], 3)
        self.assertAlmostEqual(result["window_ms"], 40.0)

    def test_command_line_prints_non_ascii_names_on_a_legacy_console(self):
        events = [dict(event, args={"name": "メイン"}) if event["name"] == "thread_name" and event["tid"] == 1 else event for event in EVENTS]
        with gzip.open(self.trace, "wt", encoding="utf-8") as handle:
            json.dump({"traceEvents": events}, handle)
        tool = Path(trace_digest.__file__).resolve()
        result = subprocess.run([sys.executable, str(tool), str(self.trace)], capture_output=True,
                                env=dict(os.environ, PYTHONIOENCODING="cp1252"), timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", "replace"))
        self.assertIn("メイン", result.stdout.decode("utf-8"))

    def test_slowest_frame_lists_its_slices(self):
        slowest = self.result()["slowest_frames"]
        self.assertEqual([frame["present"] for frame in slowest], [601, 600])
        self.assertAlmostEqual(slowest[0]["ms"], 20.0)
        largest = {(entry["name"], entry["thread"]): entry["ms"] for entry in slowest[0]["slices"]}
        self.assertAlmostEqual(largest[("Driver.Packet", "queue worker 0x0")], 16.0)
        self.assertAlmostEqual(largest[("Driver.Present", "presenter")], 16.0)
        self.assertAlmostEqual(largest[("Driver.Present.inflight_wait", "presenter")], 14.5)

    def test_threads_split_busy_and_wait(self):
        threads = {thread["thread"]: thread for thread in self.result()["threads"]}
        worker = threads["queue worker 0x0"]
        self.assertAlmostEqual(worker["wait_ms_per_frame"], 6.0 / 3)
        self.assertAlmostEqual(worker["busy_ms_per_frame"], 19.0 / 3)
        self.assertAlmostEqual(worker["untraced_ms_per_frame"], 15.0 / 3)
        self.assertAlmostEqual(worker["folded_ms_per_frame"], 1.5 / 3)
        self.assertEqual(worker["top"][0]["name"], "Driver.Packet")
        presenter = threads["presenter"]
        self.assertAlmostEqual(presenter["wait_ms_per_frame"], 15.5 / 3)
        self.assertAlmostEqual(presenter["busy_ms_per_frame"], 2.5 / 3)

    def test_largest_slices_per_frame(self):
        largest = self.result()["top_slices_ms_per_frame"]
        self.assertEqual(largest[0]["name"], "Driver.Packet")
        self.assertAlmostEqual(largest[0]["ms"], 21.0 / 3)

    def test_longest_waits(self):
        waits = self.result()["longest_waits"]
        self.assertEqual((waits[0]["name"], waits[0]["thread"], waits[0]["present"]), ("Driver.Present.inflight_wait", "presenter", 601))
        self.assertAlmostEqual(waits[0]["ms"], 14.5)
        self.assertEqual(waits[1]["name"], "Driver.WorkerWait")

    def test_gpu_mutex_waits_with_site_names(self):
        for site in ("other", "dispatch", "indirect", "draw", "hook", "wait", "label", "flush", "present", "fill", "copy", "end", "try"):
            with self.subTest(site=site):
                name = f"GpuMutex.wait.{site}"
                events = [
                    complete(1, "Driver.Packet", 0, 10000),
                    complete(1, name, 1000, 4000),
                    complete(1, "Driver.Packet.memory_wait", 2000, 1000),
                    {"name": "present", "ph": "i", "ts": 0, "tid": 1, "args": {"present": 600}},
                    {"name": "present", "ph": "i", "ts": 10000, "tid": 1, "args": {"present": 601}},
                ]
                result = trace_digest.digest(trace_digest.Trace({"traceEvents": events}))
                thread = result["threads"][0]
                self.assertAlmostEqual(thread["wait_ms_per_frame"], 4.0)
                self.assertAlmostEqual(thread["busy_ms_per_frame"], 6.0)
                self.assertAlmostEqual(thread["wait_pct"], 40.0)
                self.assertAlmostEqual(thread["busy_pct"], 60.0)
                self.assertEqual(result["longest_waits"], [
                    {"name": name, "thread": "tid 1", "ms": 4.0, "at_ms": 1.0, "present": 600},
                ])
                self.assertIn(f"- {name} (tid 1) 4.0 ms", trace_digest.markdown(result))

    def test_top_level_gpu_mutex_wait_is_not_busy_work(self):
        events = [
            complete(1, "GpuMutex.wait.draw", 0, 4000),
            complete(1, "GpuMutex.hold.draw", 4000, 6000),
            {"name": "present", "ph": "i", "ts": 0, "tid": 1, "args": {"present": 600}},
            {"name": "present", "ph": "i", "ts": 10000, "tid": 1, "args": {"present": 601}},
        ]
        result = trace_digest.digest(trace_digest.Trace({"traceEvents": events}))
        thread = result["threads"][0]
        self.assertAlmostEqual(thread["busy_ms_per_frame"], 6.0)
        self.assertAlmostEqual(thread["wait_ms_per_frame"], 4.0)
        self.assertEqual(thread["top"], [{"name": "GpuMutex.hold.draw", "ms_per_frame": 6.0}])

    def test_stacks_self_and_total(self):
        stacks = self.result()["stacks"]
        self.assertEqual(stacks["samples"], 5)
        self.assertAlmostEqual(stacks["truncated_pct"], 20.0)
        self.assertEqual(stacks["self"][0], {"frame": "a.prx+0x20", "pct": 60.0})
        total = {entry["frame"]: entry["pct"] for entry in stacks["total"]}
        self.assertEqual(stacks["common_frames"], 2)
        self.assertNotIn("a.prx+0x10", total)
        self.assertEqual(total, {"a.prx+0x20": 60.0, "a.prx+0x30": 40.0, "a.prx+0x40": 20.0})

    def test_largest_gpu_batches_with_their_recording(self):
        def tagged(tid, name, ts, dur, batch):
            return dict(complete(tid, name, ts, dur), args={"batch": batch})
        events = EVENTS + [
            {"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 3, "args": {"name": "gpu"}},
            tagged(1, "Driver.Packet.dispatch", 4500, 2000, 11),
            tagged(1, "Driver.Packet.dispatch", 13000, 3000, 12),
            tagged(3, "batch", 6000, 9000, 11),
            tagged(3, "draw", 7000, 4000, 11),
            tagged(3, "batch", 16000, 3000, 12),
        ]
        with gzip.open(self.trace, "wt", encoding="utf-8") as handle:
            json.dump({"traceEvents": events}, handle)
        batches = self.result()["largest_gpu_batches"]
        self.assertEqual([batch["batch"] for batch in batches], [11, 12])
        self.assertAlmostEqual(batches[0]["ms"], 9.0)
        self.assertEqual(batches[0]["present"], 600)
        self.assertEqual(batches[0]["gpu_ranges"], [{"name": "draw", "ms": 4.0}])
        self.assertEqual(batches[0]["recorded_by"], [{"name": "Driver.Packet.dispatch", "thread": "queue worker 0x0", "ms": 2.0}])
        self.assertEqual(batches[1]["present"], 601)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            trace_digest.main([str(self.trace)])
        self.assertIn("**Largest GPU batches**", output.getvalue())

    def test_no_batch_section_without_batch_ids(self):
        self.assertNotIn("largest_gpu_batches", self.result())

    def test_command_line_markdown_and_json(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(trace_digest.main([str(self.trace), "--stacks", str(self.stacks)]), 0)
        text = output.getvalue()
        for heading in ("**Slowest frames**", "**Threads**", "**Largest slices**", "**Longest waits**", "**Worker stacks**"):
            self.assertIn(heading, text)
        self.assertLess(len(text.encode("utf-8")), 5000)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            trace_digest.main([str(self.trace), "--json"])
        self.assertEqual(json.loads(output.getvalue())["frames"], 3)

    def test_fractional_adjacent_slices_remain_separate_in_command_output(self):
        for first, second in (("Driver.Packet", "Driver.Completion"),
                              ("Driver.WorkerWait", "Driver.CompletionWait")):
            with self.subTest(first=first):
                events = [EVENTS[1], EVENTS[-3],
                          complete(1, first, 10000.1, 20000.2),
                          complete(1, second, 30000.3, 20000.0)]
                with gzip.open(self.trace, "wt", encoding="utf-8") as handle:
                    json.dump({"traceEvents": events}, handle)
                summary = dict(SUMMARY, window=dict(SUMMARY["window"], ms=50.0003))
                self.trace.with_name("aps5-summary.jsonl").write_text(json.dumps(summary), encoding="utf-8")
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(trace_digest.main([str(self.trace), "--json"]), 0)
                result = json.loads(output.getvalue())
                thread = next(thread for thread in result["threads"] if thread["thread"] == "queue worker 0x0")
                field = "wait" if trace_digest.is_wait(first) else "busy"
                self.assertAlmostEqual(thread[field + "_ms_per_frame"], 40.0002)
                self.assertAlmostEqual(thread[field + "_pct"], 100 * 40.0002 / 50.0003)
                self.assertAlmostEqual(thread["untraced_ms_per_frame"], 10.0001)
                if field == "busy":
                    self.assertEqual({entry["name"] for entry in thread["top"]}, {first, second})
                else:
                    self.assertEqual({entry["name"] for entry in result["longest_waits"]}, {first, second})

    def test_one_nanosecond_nested_wait_remains_nested(self):
        trace = trace_digest.Trace({"traceEvents": [
            complete(1, "Driver.WorkerWait", 10000.1, 20000.2),
            complete(1, "Driver.InnerWait", 30000.299, 0.001),
            complete(1, "Driver.Completion", 30000.3, 20000.0)]})
        self.assertEqual([(entry[3], entry[4]) for entry in trace.slices[1]],
                         [(0, False), (1, True), (0, False)])

    def test_missing_summary_file_is_an_error(self):
        missing = Path(self.directory.name) / "missing.jsonl"
        error = io.StringIO()
        with contextlib.redirect_stderr(error), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                trace_digest.main([str(self.trace), "--summary", str(missing)])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("summary file not found", error.getvalue())


def interval_line(start, end, p99, gpu_ms, **changes):
    line = dict(SUMMARY, window={"spec": "summary_every 30", "first_present": 0, "ms": (end - start) * 1000.0}, interval_start_s=start, interval_end_s=end,
                presents=1800, frame_ms={"p50": 16.6, "p90": 18.0, "p99": p99, "max": p99 + 5.0}, gpu_busy_ms_per_present=gpu_ms, trace="")
    line.update(changes)
    return line


class IntervalDigestTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / "aps5-summary.jsonl"
        lines = [SUMMARY, interval_line(0.0, 30.0, 20.0, 5.0), interval_line(30.0, 60.5, 40.0, 10.0, trace_dropped=True),
                 interval_line(60.5, 90.5, 0.0, 0.0, worker_busy_pct={}, top_phases_ms_per_frame={}), interval_line(90.5, 120.5, 20.0, 5.0, gpu="Other GPU")]
        self.path.write_text("\n".join(json.dumps(line) for line in lines) + "\n", encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def test_window_lines_are_skipped(self):
        self.assertEqual(len(trace_digest.load_intervals(self.path)), 4)

    def test_rows(self):
        result = trace_digest.digest_intervals(trace_digest.load_intervals(self.path))
        first, second, third, _ = result["intervals"]
        self.assertEqual((first["start_s"], first["end_s"], first["presents"]), (0.0, 30.0, 1800))
        self.assertEqual(first["busiest_worker"], {"thread": "queue worker 0x0", "pct": 50.0})
        self.assertEqual(first["top_phase"], {"name": "Driver.Packet", "ms_per_frame": 7.0})
        self.assertTrue(second["trace_dropped"])
        self.assertIsNone(third["busiest_worker"])
        self.assertIsNone(third["top_phase"])
        self.assertEqual(result["identity_changed_at"], [3])

    def test_sparkline_scales_to_the_largest_value(self):
        self.assertEqual(trace_digest.sparkline([0.0, 5.0, 10.0]), ".+@")
        self.assertEqual(trace_digest.sparkline([0.0, 0.0]), "..")
        self.assertEqual(trace_digest.sparkline([]), "")

    def test_command_line_table_and_json(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(trace_digest.main([str(self.path)]), 0)
        text = output.getvalue()
        self.assertIn("**4 intervals**, 0.0 to 120.5 s, 7200 presents", text)
        self.assertIn("| 2 | 30-60 (dropped) | 1800 | 16.6 | 18.0 | 40.0 | 45.0 | 10.0 | 50 % | Driver.Packet 7.0 |", text)
        self.assertIn("p99 ms  +@.+  0-40.0", text)
        self.assertIn("changed at interval 4", text)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            trace_digest.main([str(self.path), "--json"])
        self.assertEqual(len(json.loads(output.getvalue())["intervals"]), 4)

    def test_file_without_intervals(self):
        self.path.write_text(json.dumps(SUMMARY) + "\n", encoding="utf-8")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            trace_digest.main([str(self.path)])
        self.assertIn("no interval lines", output.getvalue())

if __name__ == "__main__":
    unittest.main()
