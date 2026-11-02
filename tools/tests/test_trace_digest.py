import contextlib
from functools import lru_cache
import gzip
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
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


@lru_cache(maxsize=2)
def excessive_trace_json(opening, closing):
    depth = sys.getrecursionlimit() + 64
    for _ in range(8):
        value = opening * depth + "0" + closing * depth
        try:
            json.loads(value)
        except RecursionError:
            return opening * (depth + 128) + "0" + closing * (depth + 128)
        depth *= 2
    raise AssertionError("JSON decoder accepted every bounded nesting probe")


class TraceJsonNestingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.trace = Path(self.directory.name) / "baseline.json"
        self.trace.write_text(json.dumps({"traceEvents": EVENTS}), encoding="utf-8")

    def test_excessive_trace_json_nesting_has_a_path_qualified_error(self):
        for opening, closing in (("[", "]"), ('{"nested":', "}")):
            data = ('{"traceEvents": [], "extra": ' + excessive_trace_json(opening, closing) + "}").encode("utf-8")
            for compressed in (False, True):
                with self.subTest(opening=opening, compressed=compressed):
                    path = Path(self.directory.name) / ("deep-\u00e9.json.gz" if compressed else "deep-\u00e9.json")
                    original = gzip.compress(data) if compressed else data
                    path.write_bytes(original)
                    with self.assertRaisesRegex(ValueError, "trace JSON nesting is too deep") as error:
                        trace_digest.load_trace(path)
                    self.assertIn(str(path), str(error.exception))
                    self.assertIsInstance(error.exception.__cause__, RecursionError)
                    self.assertEqual(path.read_bytes(), original)

    def test_excessive_trace_json_nesting_produces_only_a_cli_error(self):
        for opening, closing, compressed, args in (("[", "]", False, ()), ('{"nested":', "}", True, ("--json",)),
                                                   ("[", "]", True, ()), ('{"nested":', "}", False, ("--json",))):
            with self.subTest(opening=opening, compressed=compressed, args=args):
                path = Path(self.directory.name) / ("deep-\u00e9.json.gz" if compressed else "deep-\u00e9.json")
                data = ('{"traceEvents": [], "extra": ' + excessive_trace_json(opening, closing) + "}").encode("utf-8")
                original = gzip.compress(data) if compressed else data
                path.write_bytes(original)
                result = subprocess.run([sys.executable, trace_digest.__file__, str(path), *args],
                                        capture_output=True, text=True, encoding="utf-8", timeout=20)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn(f"FAIL: {path}: trace JSON nesting is too deep", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(path.read_bytes(), original)

    def test_supported_nested_trace_metadata_preserves_the_digest(self):
        expected = trace_digest.digest(trace_digest.load_trace(self.trace), SUMMARY, None, 5, 10)
        metadata = {"value": 7}
        for _ in range(16):
            metadata = {"nested": [metadata]}
        data = json.dumps({"traceEvents": EVENTS, "extra": metadata}).encode("utf-8")
        for compressed in (False, True):
            with self.subTest(compressed=compressed):
                path = Path(self.directory.name) / ("nested.json.gz" if compressed else "nested.json")
                original = gzip.compress(data) if compressed else data
                path.write_bytes(original)
                trace = trace_digest.load_trace(path)
                self.assertEqual(trace_digest.digest(trace, SUMMARY, None, 5, 10), expected)
                self.assertEqual(path.read_bytes(), original)

class TraceDigestTests(unittest.TestCase):
    def test_summary_jsonl_excessive_nesting_reports_line_without_traceback(self):
        path = Path(self.directory.name) / "deep.jsonl"
        path.write_text('{}\n\n{"extra":' + excessive_trace_json('[', ']') + '}\n', encoding="utf-8")
        for arguments in ([str(path)], [str(self.trace), "--summary", str(path)]):
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, trace_digest.__file__, *arguments],
                                        capture_output=True, text=True, encoding="utf-8", timeout=20)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(f"{path}:3: JSON nesting is too deep", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual(result.stdout, "")

    def test_summary_jsonl_record_shapes_are_validated(self):
        path = Path(self.directory.name) / "invalid-summary.jsonl"
        for record in (None, True, False, 7, 1.5, "", "interval_start_s", [], ["interval_start_s"]):
            for mode in ("capture", "intervals"):
                with self.subTest(record=record, mode=mode):
                    path.write_text("{}\n\n" + json.dumps(record) + "\n", encoding="utf-8")
                    with self.assertRaises(ValueError) as raised:
                        if mode == "capture":
                            trace_digest.load_summary(path, self.trace)
                        else:
                            trace_digest.load_intervals(path)
                    self.assertEqual(str(raised.exception), f"{path}:3: summary record must be an object")

    def test_summary_jsonl_syntax_errors_name_the_source_line(self):
        path = Path(self.directory.name) / "invalid-summary.jsonl"
        for record in ("{", '{"trace": }', "{} trailing"):
            for mode in ("capture", "intervals"):
                with self.subTest(record=record, mode=mode):
                    path.write_text("{}\n\n" + record + "\n", encoding="utf-8")
                    with self.assertRaises(ValueError) as raised:
                        if mode == "capture":
                            trace_digest.load_summary(path, self.trace)
                        else:
                            trace_digest.load_intervals(path)
                    self.assertTrue(str(raised.exception).startswith(f"{path}:3: invalid JSON: "), str(raised.exception))

    def test_cli_rejects_invalid_summary_records_without_partial_output(self):
        summary = self.trace.with_name("aps5-summary.jsonl")
        explicit = summary.with_name("explicit-\u03a9.jsonl")
        invalid = [json.dumps(record) for record in (None, True, 7, "interval_start_s", [], ["interval_start_s"])]
        for record in [*invalid, "{", "{} trailing"]:
            data = (json.dumps(SUMMARY) + "\n\n" + record + "\n").encode("utf-8")
            summary.write_bytes(data)
            explicit.write_bytes(data)
            for arguments, path in (([str(self.trace)], summary), ([str(self.trace), "--summary", str(explicit)], explicit),
                                    ([str(summary)], summary)):
                for output in ([], ["--json"]):
                    with self.subTest(record=record, arguments=arguments, output=output):
                        result = subprocess.run([sys.executable, trace_digest.__file__, *arguments, *output],
                                                capture_output=True, text=True, encoding="utf-8", timeout=20)
                        self.assertEqual(result.returncode, 2, result.stderr)
                        self.assertTrue(result.stderr.startswith(f"FAIL: {path}:3: "), result.stderr)
                        self.assertNotIn("Traceback", result.stderr)
                        self.assertEqual(result.stdout, "")
                        self.assertEqual(path.read_bytes(), data)

    def test_mixed_summary_jsonl_objects_preserve_selection_and_intervals(self):
        path = self.trace.with_name("aps5-summary.jsonl")
        expected = self.json_output(self.trace)
        interval = interval_line(0.0, 30.0, 20.0, 5.0)
        records = [{}, dict(SUMMARY, trace="other.json.gz"), SUMMARY, interval, {"unrelated": "\u03a9"}]
        text = "\r\n \t\r\n" + "\r\n\r\n".join(json.dumps(record) for record in records) + "\r\n"
        path.write_text(text, encoding="utf-8-sig")
        self.assertEqual(self.json_output(self.trace), expected)
        self.assertEqual(trace_digest.load_intervals(path), [interval])
        records.extend([dict(SUMMARY, commit="latest"), dict(SUMMARY, trace=self.trace.with_suffix("").name, commit="alternate")])
        path.write_text("\n".join(json.dumps(record) for record in records), encoding="utf-8")
        self.assertEqual(trace_digest.load_summary(path, self.trace)["commit"], "latest")
        self.assertEqual(trace_digest.load_summary(path, self.trace.with_suffix(""))["commit"], "alternate")

    def test_empty_summary_jsonl_files_remain_valid(self):
        path = self.trace.with_name("aps5-summary.jsonl")
        for text in ("", "\n \t\r\n"):
            for encoding in ("utf-8", "utf-8-sig"):
                with self.subTest(text=text, encoding=encoding):
                    path.write_text(text, encoding=encoding)
                    self.assertIsNone(trace_digest.load_summary(path, self.trace))
                    self.assertEqual(trace_digest.load_intervals(path), [])

    def test_unicode_separators_inside_summary_strings_are_not_record_boundaries(self):
        path = self.trace.with_name("aps5-summary.jsonl")
        for separator in ("\u0085", "\u2028", "\u2029"):
            with self.subTest(separator=separator):
                gpu = f"GPU{separator}name"
                summary = dict(SUMMARY, gpu=gpu)
                interval = interval_line(0.0, 30.0, 20.0, 5.0, gpu=gpu)
                path.write_text("\n".join(json.dumps(record, ensure_ascii=False) for record in (summary, interval)), encoding="utf-8")
                self.assertEqual(trace_digest.load_summary(path, self.trace), summary)
                self.assertEqual(trace_digest.load_intervals(path), [interval])

    def test_cli_rejects_invalid_event_container_shapes_without_partial_output(self):
        cases = [(value, "trace must be an event array or object") for value in (None, True, 7, "trace")]
        cases += [({"traceEvents": value}, "traceEvents must be an array")
                  for value in (None, False, 0, "", {}, "events")]
        cases += [([value], "trace event 0 must be an object") for value in (None, True, 7, "event", [])]
        cases.append(({"traceEvents": [{}, None]}, "trace event 1 must be an object"))
        for document, message in cases:
            for compressed in (False, True):
                with self.subTest(document=document, compressed=compressed):
                    path = Path(self.directory.name) / ("invalid.json.gz" if compressed else "invalid.json")
                    data = json.dumps(document).encode("utf-8")
                    path.write_bytes(gzip.compress(data) if compressed else data)
                    result = subprocess.run([sys.executable, trace_digest.__file__, str(path), "--json"],
                                            capture_output=True, text=True, encoding="utf-8", timeout=20)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("FAIL: " + message, result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(result.stdout, "")

    def test_cli_rejects_invalid_event_fields_without_partial_output(self):
        slice_event = complete(1, "Driver.Packet", 0, 10)
        cases = [
            (dict(slice_event, tid=None), "trace event 0: tid must be an integer"),
            (dict(slice_event, tid="1"), "trace event 0: tid must be an integer"),
            (dict(slice_event, tid=True), "trace event 0: tid must be an integer"),
            ({key: value for key, value in slice_event.items() if key != "tid"}, "trace event 0: tid must be an integer"),
            ({key: value for key, value in slice_event.items() if key != "dur"}, "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur=None), "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur="slow"), "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur="10"), "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur=True), "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur=[1]), "trace event 0: dur must be a finite number"),
            (dict(slice_event, dur=-1), "trace event 0: dur must be non-negative"),
            (dict(slice_event, dur=-0.001, args={"batch": 42}), "trace event 0: dur must be non-negative"),
            (dict(slice_event, ts={}), "trace event 0: ts must be a finite number"),
            (dict(slice_event, ts="0"), "trace event 0: ts must be a finite number"),
            (dict(slice_event, ts=float("nan")), "trace event 0: ts must be a finite number"),
            (dict(slice_event, ts=float("inf")), "trace event 0: ts must be a finite number"),
            (dict(slice_event, ts=1e308), "trace event 0: ts must be a finite number"),
            (dict(slice_event, name=7), "trace event 0: name must be a string"),
            ({key: value for key, value in slice_event.items() if key != "name"}, "trace event 0: name must be a string"),
            (dict(slice_event, args=[]), "trace event 0: args must be an object"),
            (dict(slice_event, args={"batch": [1]}), "trace event 0: args.batch must be an integer or a string"),
            (dict(slice_event, args={"batch": []}), "trace event 0: args.batch must be an integer or a string"),
            (dict(slice_event, args={"batch": None}), "trace event 0: args.batch must be an integer or a string"),
            ({"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1}, "trace event 0: args.name must be a string"),
            ({"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": 1, "args": {}}, "trace event 0: args.name must be a string"),
            ({"name": "thread_name", "ph": "M", "ts": 0, "pid": 7, "tid": [1], "args": {"name": "a"}}, "trace event 0: tid must be an integer"),
            ({"name": "present", "ph": "i", "ts": None, "pid": 7, "tid": 2, "s": "g"}, "trace event 0: ts must be a finite number"),
            ({"name": "present", "ph": "i", "pid": 7, "tid": 2, "s": "g"}, "trace event 0: ts must be a finite number"),
            ({"name": "present", "ph": "i", "ts": 0, "pid": 7, "tid": 2, "s": "g", "args": 5}, "trace event 0: args must be an object"),
            ({"name": "folded", "ph": "C", "ts": 0, "pid": 7, "args": {"ms": 1.5}}, "trace event 0: tid must be an integer"),
            ({"name": "folded", "ph": "C", "ts": 0, "pid": 7, "tid": 1, "args": []}, "trace event 0: args must be an object"),
            ({"name": "folded", "ph": "C", "ts": 0, "pid": 7, "tid": 1, "args": {"ms": "x"}}, "trace event 0: args value must be a finite number"),
            ({"name": "folded", "ph": "C", "ts": 0, "pid": 7, "tid": 1, "args": {"ms": True}}, "trace event 0: args value must be a finite number"),
        ]
        for event, message in cases:
            for compressed in (False, True):
                with self.subTest(event=event, compressed=compressed):
                    path = Path(self.directory.name) / ("fields.json.gz" if compressed else "fields.json")
                    data = json.dumps({"traceEvents": [event]}).encode("utf-8")
                    path.write_bytes(gzip.compress(data) if compressed else data)
                    result = subprocess.run([sys.executable, trace_digest.__file__, str(path), "--json"],
                                            capture_output=True, text=True, encoding="utf-8", timeout=20)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("FAIL: " + message, result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(result.stdout, "")

    def test_cli_reports_unreadable_inputs_without_partial_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compressed = gzip.compress(b'{"traceEvents": []}')
            files = {"invalid.json": b"{", "broken.json.gz": b"not gzip", "intervals.jsonl": b"{\n",
                     "truncated.json.gz": compressed[:-4],
                     "corrupt.json.gz": compressed[:10] + b"\xff" * 4 + compressed[14:]}
            for name, data in files.items():
                (root / name).write_bytes(data)
            for name in ("missing.json", *files):
                with self.subTest(name=name):
                    result = subprocess.run([sys.executable, trace_digest.__file__, str(root / name)],
                                            capture_output=True, text=True, encoding="utf-8", timeout=20)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertTrue(result.stderr.startswith("FAIL: "), result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(result.stdout, "")


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

    def test_trace_accepts_event_array_and_empty_document(self):
        trace_from_array = trace_digest.Trace(EVENTS)
        self.assertEqual(len(trace_from_array.presents), 3)
        empty_dict = trace_digest.Trace({})
        self.assertEqual(empty_dict.presents, [])
        empty_list = trace_digest.Trace([])
        self.assertEqual(empty_list.presents, [])
        empty_events = trace_digest.Trace({"traceEvents": []})
        self.assertEqual(empty_events.presents, [])

    def result(self):
        return trace_digest.digest(trace_digest.load_trace(self.trace), trace_digest.load_summary(None, self.trace),
                                   trace_digest.load_stacks(self.stacks), worst=2, top=3)

    def json_output(self, trace, *arguments):
        result = subprocess.run([sys.executable, trace_digest.__file__, str(trace), "--json", *arguments],
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_utf8_bom_plain_and_compressed_traces_preserve_the_digest(self):
        events = [dict(event, args={"name": "worker \u03a9"}) if event["name"] == "thread_name" and event["tid"] == 1
                  else event for event in EVENTS]
        text = json.dumps({"traceEvents": events}, ensure_ascii=False)
        self.trace.write_bytes(gzip.compress(text.encode("utf-8")))
        expected = self.json_output(self.trace)
        for trace in (self.trace, self.trace.with_suffix("")):
            for encoding in ("utf-8", "utf-8-sig"):
                with self.subTest(trace=trace.name, encoding=encoding):
                    data = text.encode(encoding)
                    trace.write_bytes(gzip.compress(data) if trace.suffix == ".gz" else data)
                    self.assertEqual(self.json_output(trace), expected)

    def test_utf8_bom_summary_preserves_the_digest(self):
        expected = self.json_output(self.trace)
        summary = self.trace.with_name("aps5-summary.jsonl")
        summary.write_text(summary.read_text(encoding="utf-8"), encoding="utf-8-sig")
        self.assertEqual(self.json_output(self.trace), expected)

    def test_utf8_bom_stacks_preserve_comments_and_frame_names(self):
        for text in (STACKS, "root;module\u03a9+0x1 2\n"):
            with self.subTest(comments=text.startswith("#")):
                self.stacks.write_text(text, encoding="utf-8")
                expected = self.json_output(self.trace, "--stacks", str(self.stacks))
                expected_stacks = trace_digest.load_stacks(self.stacks)
                self.stacks.write_text(text, encoding="utf-8-sig")
                self.assertEqual(self.json_output(self.trace, "--stacks", str(self.stacks)), expected)
                self.assertEqual(trace_digest.load_stacks(self.stacks), expected_stacks)

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

    def test_markdown_thread_names_cannot_split_table_cells_or_rows(self):
        name = "worker|<gpu>\nrow & more"
        phase = "phase|<tag>\r\nnext"
        trace = trace_digest.Trace({"traceEvents": [
            {"ph": "M", "name": "thread_name", "tid": 1, "args": {"name": name}},
            complete(1, phase, 0, 1000),
        ]})
        result = trace_digest.digest(trace)
        self.assertEqual(result["threads"][0]["thread"], name)
        self.assertEqual(result["threads"][0]["top"][0]["name"], phase)
        rows = [line for line in trace_digest.markdown(result).splitlines() if line.startswith("|")]
        self.assertEqual(len(rows), 3)
        self.assertTrue(all(line.count("|") == 7 for line in rows), rows)
        self.assertIn("worker&#124;&lt;gpu&gt;<br>row &amp; more", rows[2])
        self.assertIn("phase&#124;&lt;tag&gt;<br>next", rows[2])


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

    def test_thread_totals_only_count_time_inside_the_frame_window(self):
        events = [
            complete(1, "Driver.Packet", -5000, 20000),
            complete(1, "GpuMutex.wait.draw", -2000, 4000),
            complete(1, "Driver.Packet.after", 12000, 1000),
            complete(2, "Driver.WorkerWait", -10000, 2000),
            {"name": "present", "ph": "i", "ts": 0, "tid": 1, "args": {"present": 600}},
            {"name": "present", "ph": "i", "ts": 10000, "tid": 1, "args": {"present": 601}},
        ]
        result = trace_digest.digest(trace_digest.Trace(events))
        active, outside = result["threads"]
        self.assertEqual(active["busy_ms_per_frame"], 8.0)
        self.assertEqual(active["wait_ms_per_frame"], 2.0)
        self.assertEqual(active["busy_pct"], 80.0)
        self.assertEqual(active["wait_pct"], 20.0)
        self.assertEqual(active["top"], [{"name": "Driver.Packet", "ms_per_frame": 10.0}])
        self.assertEqual(outside["busy_ms_per_frame"], 0.0)
        self.assertEqual(outside["wait_ms_per_frame"], 0.0)
        self.assertEqual(outside["untraced_ms_per_frame"], 10.0)
        self.assertEqual(outside["top"], [])
        self.assertEqual(result["top_slices_ms_per_frame"], [
            {"name": "Driver.Packet", "ms": 10.0},
            {"name": "GpuMutex.wait.draw", "ms": 2.0},
        ])

    def test_thread_totals_use_the_summary_end_after_the_last_present(self):
        trace = trace_digest.Trace([
            complete(1, "Driver.Packet", 1000, 20000),
            {"name": "present", "ph": "i", "ts": 0, "tid": 1, "args": {"present": 600}},
        ])
        result = trace_digest.digest(trace, {"window": {"ms": 10.0}})
        thread = result["threads"][0]
        self.assertEqual(result["frames"], 1)
        self.assertEqual(thread["busy_ms_per_frame"], 9.0)
        self.assertEqual(thread["busy_pct"], 90.0)
        self.assertEqual(thread["untraced_ms_per_frame"], 1.0)

    def test_largest_slices_per_frame(self):
        largest = self.result()["top_slices_ms_per_frame"]
        self.assertEqual(largest[0]["name"], "Driver.Packet")
        self.assertAlmostEqual(largest[0]["ms"], 21.0 / 3)

    def test_longest_waits(self):
        waits = self.result()["longest_waits"]
        self.assertEqual((waits[0]["name"], waits[0]["thread"], waits[0]["present"]), ("Driver.Present.inflight_wait", "presenter", 601))
        self.assertAlmostEqual(waits[0]["ms"], 14.5)
        self.assertEqual(waits[1]["name"], "Driver.WorkerWait")

    def test_longest_waits_omit_slices_outside_capture_in_cli(self):
        events = [EVENTS[1], *EVENTS[-3:],
                  complete(1, "Inside.wait", 1000, 2000),
                  complete(3, "Before.wait", -100000, 90000),
                  complete(4, "After.wait", 40000, 90000),
                  complete(5, "EndsAtStart.wait", -1000, 1000),
                  complete(6, "StartsAtEnd.wait", 40000, 1000)]
        plain = self.trace.with_suffix("")
        plain.write_text(json.dumps({"traceEvents": events}), encoding="utf-8")
        self.trace.write_bytes(gzip.compress(plain.read_bytes()))
        for path in (self.trace, plain):
            for output in ([], ["--json"]):
                with self.subTest(path=path.name, output=output):
                    result = subprocess.run([sys.executable, trace_digest.__file__, str(path), "--top", "1", *output],
                                            capture_output=True, text=True, encoding="utf-8", timeout=20)
                    self.assertEqual((result.returncode, result.stderr), (0, ""))
                    if output:
                        report = json.loads(result.stdout)
                        self.assertEqual(report["longest_waits"], [
                            {"name": "Inside.wait", "thread": "queue worker 0x0", "ms": 2.0, "at_ms": 1.0, "present": 600}])
                    else:
                        self.assertIn("- Inside.wait (queue worker 0x0) 2.0 ms at 1 ms, present 600", result.stdout)
                        for name in ("Before.wait", "After.wait", "EndsAtStart.wait", "StartsAtEnd.wait"):
                            self.assertNotIn(name, result.stdout)

    def test_longest_waits_rank_only_the_overlap_with_capture(self):
        events = [dict(EVENTS[-3], ts=10000), dict(EVENTS[-2], ts=30000),
                  complete(1, "Leading.wait", -100000, 111000),
                  complete(2, "Inside.wait", 12000, 4000),
                  complete(3, "Trailing.wait", 28000, 90000),
                  complete(4, "Spanning.wait", -100000, 230000),
                  complete(4, "Nested.wait", 11000, 1000)]
        report = trace_digest.digest(trace_digest.Trace(events), top=4)
        self.assertEqual(report["longest_waits"], [
            {"name": "Spanning.wait", "thread": "tid 4", "ms": 20.0, "at_ms": 10.0, "present": 600},
            {"name": "Inside.wait", "thread": "tid 2", "ms": 4.0, "at_ms": 12.0, "present": 600},
            {"name": "Trailing.wait", "thread": "tid 3", "ms": 2.0, "at_ms": 28.0, "present": 600},
            {"name": "Leading.wait", "thread": "tid 1", "ms": 1.0, "at_ms": 10.0, "present": 600}])
        threads = {thread["thread"]: thread for thread in report["threads"]}
        for wait in report["longest_waits"]:
            self.assertEqual(wait["ms"], threads[wait["thread"]]["wait_ms_per_frame"])

    def test_longest_waits_clip_to_summary_end(self):
        trace = trace_digest.Trace([EVENTS[-3],
                                    complete(1, "Trailing.wait", 8000, 20000),
                                    complete(2, "Inside.wait", 1000, 3000)])
        report = trace_digest.digest(trace, {"window": {"ms": 10.0}}, top=1)
        self.assertEqual(report["longest_waits"], [
            {"name": "Inside.wait", "thread": "tid 2", "ms": 3.0, "at_ms": 1.0, "present": 600}])

    def test_longest_waits_without_frames_keep_the_full_slices(self):
        for duration in (0, 3000):
            with self.subTest(duration=duration):
                trace = trace_digest.Trace([complete(1, "Worker.wait", -1000, duration)])
                self.assertEqual(trace_digest.digest(trace)["longest_waits"], [
                    {"name": "Worker.wait", "thread": "tid 1", "ms": duration / 1000.0, "at_ms": -1.0, "present": None}])

    def test_wait_subscope_without_parent_is_not_busy(self):
        events = [EVENTS[1], complete(1, "Driver.WorkerWait.dequeue_order", 0, 4000),
                  complete(1, "Driver.Packet", 4000, 5000), EVENTS[-3], EVENTS[-2]]
        result = trace_digest.digest(trace_digest.Trace({"traceEvents": events}))
        worker = result["threads"][0]
        self.assertAlmostEqual(worker["wait_pct"], 40.0)
        self.assertAlmostEqual(worker["busy_pct"], 50.0)
        self.assertEqual(result["longest_waits"][0]["name"], "Driver.WorkerWait.dequeue_order")
        self.assertEqual(worker["top"][0]["name"], "Driver.Packet")

    def test_nested_wait_subscope_does_not_double_count(self):
        events = [EVENTS[1], complete(1, "Driver.WorkerWait", 0, 4000),
                  complete(1, "Driver.WorkerWait.dequeue_order", 1000, 2000),
                  complete(1, "Driver.Packet", 4000, 5000), EVENTS[-3], EVENTS[-2]]
        result = trace_digest.digest(trace_digest.Trace({"traceEvents": events}))
        worker = result["threads"][0]
        self.assertAlmostEqual(worker["wait_pct"], 40.0)
        self.assertAlmostEqual(worker["busy_pct"], 50.0)
        self.assertEqual(len(result["longest_waits"]), 1)

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

    def test_invalid_stack_weights_fail_without_a_digest(self):
        for row in ("worker;draw -1", "worker;draw 0", "worker;draw nope", "worker;;draw 1", " 1"):
            with self.subTest(row=row):
                self.stacks.write_text("# header\n" + row + "\n", encoding="utf-8")
                output, errors = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    with self.assertRaises(SystemExit) as exit:
                        trace_digest.main([str(self.trace), "--stacks", str(self.stacks), "--json"])
                self.assertEqual(exit.exception.code, 2)
                self.assertEqual(output.getvalue(), "")
                self.assertIn(str(self.stacks) + ":2:", errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())


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

    def test_negative_list_limits_are_rejected_and_zero_is_supported(self):
        for option in ("--frames", "--top"):
            with self.subTest(option=option):
                output, errors = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    with self.assertRaises(SystemExit) as exit:
                        trace_digest.main([str(self.trace), option, "-1", "--json"])
                self.assertEqual(exit.exception.code, 2)
                self.assertEqual(output.getvalue(), "")
                self.assertIn(option, errors.getvalue())
                self.assertIn("nonnegative", errors.getvalue())
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(trace_digest.main([str(self.trace), "--frames", "0", "--top", "0", "--json"]), 0)
        result = json.loads(output.getvalue())
        self.assertEqual(result["slowest_frames"], [])
        self.assertEqual(result["top_slices_ms_per_frame"], [])
        self.assertEqual(result["longest_waits"], [])


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

    def test_interval_input_rejects_stack_samples(self):
        path = Path(self.directory.name) / "intervals.jsonl"
        path.write_text("", encoding="utf-8")
        for output in ([], ["--json"]):
            with self.subTest(output=output):
                result = subprocess.run(
                    [sys.executable, trace_digest.__file__, str(path), "--stacks", str(self.stacks), *output],
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertIn("--stacks requires an event trace, not interval summaries", result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertNotIn("Traceback", result.stderr)

    def test_missing_summary_file_is_an_error(self):
        missing = Path(self.directory.name) / "missing.jsonl"
        error = io.StringIO()
        with contextlib.redirect_stderr(error), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                trace_digest.main([str(self.trace), "--summary", str(missing)])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("summary file not found", error.getvalue())

    def test_interval_input_rejects_capture_summary(self):
        path = Path(self.directory.name) / "intervals.jsonl"
        path.write_text("", encoding="utf-8")
        summary = Path(self.directory.name) / "capture.jsonl"
        summary.write_text(json.dumps(SUMMARY) + "\n", encoding="utf-8")
        for output in ([], ["--json"]):
            with self.subTest(output=output):
                result = subprocess.run(
                    [sys.executable, trace_digest.__file__, str(path), "--summary", str(summary), *output],
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertIn("--summary requires an event trace, not interval summaries", result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertNotIn("Traceback", result.stderr)

    def test_thread_filter_keeps_frame_bounds_and_capture_summary(self):
        command = [sys.executable, trace_digest.__file__, str(self.trace)]
        result = subprocess.run([*command, "--thread", "queue worker 0x0", "--json"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["frames"], 3)
        self.assertEqual(report["summary"], SUMMARY)
        self.assertEqual(report["thread_filter"], ["queue worker 0x0"])
        self.assertEqual([thread["thread"] for thread in report["threads"]], ["queue worker 0x0"])
        self.assertTrue(all("Present" not in entry["name"] for entry in report["top_slices_ms_per_frame"]))
        self.assertTrue(all(wait["thread"] == "queue worker 0x0" for wait in report["longest_waits"]))
        result = subprocess.run([*command, "--thread", "presenter", "--thread", "queue worker 0x0", "--json"],
                                capture_output=True, text=True)
        unfiltered = trace_digest.digest(trace_digest.load_trace(self.trace), SUMMARY)
        report = json.loads(result.stdout)
        del report["thread_filter"]
        self.assertEqual(report, unfiltered)
        result = subprocess.run([*command, "--thread", "presenter"], capture_output=True, text=True)
        self.assertIn("Thread filter:", result.stdout)
        self.assertIn("capture-wide", result.stdout)
        result = subprocess.run([*command, "--thread", "misspelled"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown thread", result.stderr)
        self.assertEqual(result.stdout, "")
        result = subprocess.run([sys.executable, trace_digest.__file__, str(self.trace.with_suffix(".jsonl")),
                                 "--thread", "presenter"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("event trace", result.stderr)



def interval_line(start, end, p99, gpu_ms, **changes):
    line = dict(SUMMARY, window={"spec": "summary_every 30", "first_present": 0, "ms": (end - start) * 1000.0}, interval_start_s=start, interval_end_s=end,
                presents=1800, frame_ms={"p50": min(16.6, p99), "p90": min(18.0, p99), "p99": p99, "max": p99 + 5.0}, gpu_busy_ms_per_present=gpu_ms, trace="")
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

    def test_utf8_bom_intervals_preserve_the_command_line_digest(self):
        expected = trace_digest.digest_intervals(trace_digest.load_intervals(self.path))
        self.path.write_text(self.path.read_text(encoding="utf-8"), encoding="utf-8-sig")
        result = subprocess.run([sys.executable, trace_digest.__file__, str(self.path), "--json"],
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), expected)

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

    def test_identity_changes_are_reported_where_they_happen(self):
        lines = [interval_line(30.0 * index, 30.0 * (index + 1), 20.0, 5.0, commit=commit)
                 for index, commit in enumerate(("a", "a", "b", "b", "a"))]
        result = trace_digest.digest_intervals(lines)
        self.assertEqual(result["identity_changed_at"], [2, 4])
        self.assertIn("build, environment, host, resolution or title metadata changed at interval 3, 5", trace_digest.markdown_intervals(result).splitlines())

    def test_machine_and_title_identity_fields_are_retained_and_compared(self):
        metadata = {"cpu": "CPU A", "cpu_threads": 16, "memory_mib": 32768,
                    "title_id": "PPSA00001", "title": "Title One", "content_version": "01.000.000"}
        first = interval_line(0.0, 30.0, 20.0, 5.0, **metadata)
        for key, value in (("cpu", "CPU B"), ("cpu_threads", 32), ("memory_mib", 65536),
                           ("title_id", "PPSA00002"), ("title", "Title Two"), ("content_version", "02.000.000")):
            with self.subTest(key=key):
                changed = dict(first, **{key: value})
                result = trace_digest.digest_intervals([first, first, changed, changed, first])
                self.assertEqual({key: result["identity"].get(key) for key in metadata}, metadata)
                self.assertEqual(result["identity_changed_at"], [2, 4])
                self.assertIn("changed at interval 3, 5", trace_digest.markdown_intervals(result))

    def test_optional_identity_appearance_disappearance_and_return(self):
        absent = interval_line(0.0, 30.0, 20.0, 5.0)
        for key, value in (("cpu", "CPU A"), ("cpu_threads", 0), ("memory_mib", 0),
                           ("title_id", "PPSA00001"), ("title", "Title One"), ("content_version", "")):
            with self.subTest(key=key, value=value):
                present = dict(absent, **{key: value})
                result = trace_digest.digest_intervals([absent, present, present, absent, present])
                self.assertEqual(result["identity_changed_at"], [1, 3, 4])
                self.assertIn(key, result["identity"])
                self.assertIsNone(result["identity"][key])
                self.assertIn("changed at interval 2, 4, 5", trace_digest.markdown_intervals(result))

    def test_stable_or_absent_optional_identity_does_not_report_changes(self):
        absent = interval_line(0.0, 30.0, 20.0, 5.0)
        metadata = {"cpu": "CPU A", "cpu_threads": 0, "memory_mib": 0,
                    "title_id": "PPSA00001", "title": "Title One", "content_version": ""}
        for line in (absent, dict(absent, **metadata)):
            with self.subTest(line=line):
                result = trace_digest.digest_intervals([line, line])
                self.assertEqual(result["identity_changed_at"], [])
                self.assertNotIn("changed at interval", trace_digest.markdown_intervals(result))

    def test_resolution_changes_are_reported(self):
        first = interval_line(0.0, 30.0, 20.0, 5.0)
        result = trace_digest.digest_intervals([first, dict(first, resolution="1280x720")])
        self.assertEqual(result["identity_changed_at"], [1])
        self.assertIn("build, environment, host, resolution or title metadata changed at interval 2",
                      trace_digest.markdown_intervals(result))

    def test_cli_retains_machine_and_title_identity_and_reports_changes(self):
        metadata = {"cpu": "CPU A", "cpu_threads": 16, "memory_mib": 32768,
                    "title_id": "PPSA00001", "title": "Title One", "content_version": "01.000.000"}
        first = interval_line(0.0, 30.0, 20.0, 5.0, **metadata)
        second = interval_line(30.0, 60.0, 40.0, 10.0, **dict(metadata, content_version="02.000.000"))
        self.path.write_text("\n".join(json.dumps(line) for line in (first, second)) + "\n", encoding="utf-8")
        result = subprocess.run([sys.executable, trace_digest.__file__, str(self.path), "--json"],
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual({key: report["identity"].get(key) for key in metadata}, metadata)
        self.assertEqual(report["identity_changed_at"], [1])
        without_metadata = [{key: value for key, value in line.items() if key not in metadata} for line in (first, second)]
        unchanged_metrics = trace_digest.digest_intervals(without_metadata)
        self.assertEqual(report["intervals"], unchanged_metrics["intervals"])
        result = subprocess.run([sys.executable, trace_digest.__file__, str(self.path)],
                                capture_output=True, text=True, encoding="utf-8", timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("build, environment, host, resolution or title metadata changed at interval 2", result.stdout)
        expected = trace_digest.markdown_intervals(unchanged_metrics)
        self.assertEqual(result.stdout[result.stdout.index("| # |"):], expected[expected.index("| # |"):])

    def test_sparkline_scales_to_the_largest_value(self):
        self.assertEqual(trace_digest.sparkline([0.0, 5.0, 10.0]), ".+@")
        self.assertEqual(trace_digest.sparkline([0.0, 0.0]), "..")
        self.assertEqual(trace_digest.sparkline([]), "")

    def test_largest_phase_does_not_depend_on_json_key_order(self):
        phases = [("Driver.Completion", 1.0), ("Driver.Packet", 7.0), ("Driver.Present", 2.0)]
        for offset in range(len(phases)):
            for sort_keys in (False, True):
                with self.subTest(offset=offset, sort_keys=sort_keys):
                    line = interval_line(0.0, 30.0, 20.0, 5.0,
                                         top_phases_ms_per_frame=dict(phases[offset:] + phases[:offset]))
                    self.path.write_text(json.dumps(line, sort_keys=sort_keys) + "\n", encoding="utf-8")
                    output = io.StringIO()
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(trace_digest.main([str(self.path), "--json"]), 0)
                    result = json.loads(output.getvalue())
                    self.assertEqual(result["intervals"][0]["top_phase"],
                                     {"name": "Driver.Packet", "ms_per_frame": 7.0})
                    output = io.StringIO()
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(trace_digest.main([str(self.path)]), 0)
                    self.assertIn("| Driver.Packet 7.0 |", output.getvalue())

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

    def test_interval_phase_names_preserve_json_and_markdown_table_structure(self):
        for name, escaped in (("Draw|<pass>&", "Draw&#124;&lt;pass&gt;&amp;"),
                              ("Draw\r\npass", "Draw<br>pass"),
                              ("Draw\rpass\nend", "Draw<br>pass<br>end")):
            with self.subTest(name=name):
                line = interval_line(0.0, 30.0, 20.0, 5.0, top_phases_ms_per_frame={name: 7.5})
                self.path.write_text(json.dumps(line) + "\n", encoding="utf-8")
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(trace_digest.main([str(self.path), "--json"]), 0)
                row = json.loads(output.getvalue())["intervals"][0]
                self.assertEqual(row["top_phase"], {"name": name, "ms_per_frame": 7.5})
                self.assertEqual(row["frame_ms"], line["frame_ms"])
                self.assertEqual(row["gpu_busy_ms_per_present"], 5.0)
                self.assertEqual((row["start_s"], row["end_s"], row["presents"]), (0.0, 30.0, 1800))
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(trace_digest.main([str(self.path)]), 0)
                rows = [line for line in output.getvalue().splitlines() if line.startswith("|")]
                self.assertEqual(len(rows), 3)
                self.assertTrue(all(line.count("|") == 11 for line in rows), rows)
                cells = [cell.strip() for cell in rows[2].split("|")[1:-1]]
                self.assertEqual(cells, ["1", "0-30", "1800", "16.6", "18.0", "20.0", "25.0", "5.0", "50 %", escaped + " 7.5"])

    def test_bom_encoded_stacks_load(self):
        with tempfile.TemporaryDirectory() as directory:
            stacks_file = Path(directory) / "stacks.txt"
            stacks_file.write_bytes(b"\xef\xbb\xbf# samples=1 depth=1 truncated=0\nmodule.prx+0x10 1\n")
            loaded = trace_digest.load_stacks(stacks_file)
            self.assertEqual(loaded, [(["module.prx+0x10"], 1)])

    def test_bom_encoded_intervals_load(self):
        with tempfile.TemporaryDirectory() as directory:
            intervals_file = Path(directory) / "intervals.jsonl"
            intervals_file.write_bytes(b"\xef\xbb\xbf" + json.dumps(interval_line(0.0, 10.0, 20.0, 5.0, presents=600)).encode("utf-8") + b"\n")
            loaded = trace_digest.load_intervals(intervals_file)
            self.assertEqual(len(loaded), 1)
            self.assertEqual(loaded[0]["interval_start_s"], 0.0)

    def test_bom_encoded_summary_loads(self):
        with tempfile.TemporaryDirectory() as directory:
            summary_file = Path(directory) / "aps5-summary.jsonl"
            summary_file.write_bytes(b"\xef\xbb\xbf" + json.dumps(SUMMARY).encode("utf-8") + b"\n")
            loaded = trace_digest.load_summary(str(summary_file), SUMMARY["trace"])
            self.assertEqual(loaded["commit"], SUMMARY["commit"])

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with mock.patch.object(trace_digest.sys, "stdout", out), mock.patch.object(trace_digest.sys, "stderr", err):
            try:
                trace_digest.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


class IntervalSchemaTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / "summary.jsonl"

    def write(self, record):
        self.path.write_text("\n" + json.dumps(SUMMARY) + "\n\n" + json.dumps(record) + "\n", encoding="utf-8")

    def test_invalid_interval_fields_report_physical_line_numbers(self):
        baseline = interval_line(0, 1, 20, 1)
        changes = [("interval_start_s", value) for value in (-1, True, "0", None, float("nan"), float("inf"), 10**400)]
        changes += [("interval_end_s", -1), ("interval_start_s", 2)]
        changes += [("presents", value) for value in (-1, True, 1.5, "1", None)]
        changes += [("frame_ms", value) for value in (None, [], {"p50": 1})]
        changes += [("gpu_busy_ms_per_present", value) for value in (-1, False, "1", float("inf"))]
        changes += [("worker_busy_pct", value) for value in ([], None, {"worker": "busy"}, {"worker": -1})]
        changes += [("top_phases_ms_per_frame", value) for value in ([], {"phase": True}, {"phase": float("nan")})]
        changes += [("trace_dropped", value) for value in (None, 0, "false")]
        for field, value in changes:
            with self.subTest(field=field, value=value):
                self.write({**baseline, field: value})
                with self.assertRaisesRegex(ValueError, r"summary\.jsonl:4:"):
                    trace_digest.load_intervals(self.path)

    def test_invalid_percentile_values_are_rejected(self):
        for field in ("p50", "p90", "p99", "max"):
            for value in (True, -1, "bad", float("nan")):
                with self.subTest(field=field, value=value):
                    record = interval_line(0, 1, 20, 1)
                    record["frame_ms"][field] = value
                    self.write(record)
                    with self.assertRaisesRegex(ValueError, r"summary\.jsonl:4:"):
                        trace_digest.load_intervals(self.path)

    def test_out_of_order_percentiles_are_rejected_by_both_cli_reports(self):
        for frames in (dict(p50=2, p90=1, p99=3, max=4), dict(p50=1, p90=3, p99=2, max=4),
                       dict(p50=1, p90=2, p99=4, max=3)):
            self.write(interval_line(0, 1, 20, 1, frame_ms=frames))
            for script in (trace_digest.__file__, Path(trace_digest.__file__).with_name("trace_intervals_csv.py")):
                with self.subTest(frames=frames, script=script):
                    result = subprocess.run([sys.executable, str(script), str(self.path)],
                                            capture_output=True, text=True, encoding="utf-8", timeout=10)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn("summary.jsonl:4:", result.stderr)
                    self.assertIn("p50 <= p90 <= p99 <= max", result.stderr)
                    self.assertEqual(result.stdout, "")
                    self.assertNotIn("Traceback", result.stderr)

    def test_json_and_markdown_reject_bad_intervals_without_tracebacks(self):
        self.write(interval_line(0, 1, 20, 1, presents=-1))
        for options in ([], ["--json"]):
            result = subprocess.run([sys.executable, trace_digest.__file__, str(self.path), *options],
                                    capture_output=True, text=True, encoding="utf-8", timeout=10)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("summary.jsonl:4:", result.stderr)
            self.assertNotIn("Traceback", result.stderr)
            self.assertEqual(result.stdout, "")

    def test_zero_frame_intervals_optional_maps_and_large_worker_percentages_remain_valid(self):
        record = interval_line(0, 0, 0, 0, presents=0, frame_ms=dict(p50=0, p90=0, p99=0, max=0), worker_busy_pct={"worker": 125.0})
        record.pop("top_phases_ms_per_frame", None)
        record.pop("trace_dropped", None)
        self.write(record)
        loaded = trace_digest.load_intervals(self.path)
        self.assertEqual(loaded, [record])
        digest = trace_digest.digest_intervals(loaded)
        self.assertEqual(digest["intervals"][0]["busiest_worker"]["pct"], 125)
        self.assertIn("0-0", trace_digest.markdown_intervals(digest))


if __name__ == "__main__":
    unittest.main(defaultTest=os.environ.get("TRACE_DIGEST_TEST"))
