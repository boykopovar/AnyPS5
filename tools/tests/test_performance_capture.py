import codecs
import copy
import gzip
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import performance_capture


class CaptureFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name).resolve()
        self.summary = {
            "commit": "native123", "dirty": False,
            "env": {"APS5_TRACE_FRAMES": "3:2", "APS5_TRACE_DIR": "<path>", "APS5_DRAW_PIPELINE": "1"},
            "gpu": "GPU", "driver": "Driver", "os": "OS", "resolution": "1920x1080",
            "title_id": "PPSA00001", "title": "Fixture", "content_version": "01.000.000",
            "window": {"spec": "frames 3:2", "first_present": 3, "ms": 40.0}, "presents": 2,
            "frame_ms": {"p50": 20.0, "p90": 20.0, "p99": 20.0, "max": 20.0},
            "gpu_busy_ms_per_present": 5.0, "worker_busy_pct": {}, "phase_totals_ms": {},
            "top_phases_ms_per_frame": {}, "trace_dropped": False,
            "trace": "aps5-trace-native123-42.json.gz",
        }
        self.trace = {"traceEvents": [
            {"name": "process_name", "ph": "M", "ts": 0.0, "pid": 42, "tid": 0, "args": {"name": "AnyPS5"}},
            {"name": "thread_name", "ph": "M", "ts": 0.0, "pid": 42, "tid": 1, "args": {"name": "presenter"}},
            {"name": "Driver.Present", "ph": "X", "ts": -10.0, "dur": 20.0, "pid": 42, "tid": 1},
            {"name": "present", "ph": "i", "ts": 0.0, "pid": 42, "tid": 1, "args": {"present": 3}},
            {"name": "present", "ph": "i", "ts": 20000.0, "pid": 42, "tid": 1, "args": {"present": 4}},
        ]}
        self.summary_path = self.directory / "aps5-summary.jsonl"
        self.trace_path = self.directory / self.summary["trace"]

    def tearDown(self):
        self.temporary.cleanup()

    def write_summary(self, summary=None, raw=None):
        if raw is None:
            raw = (json.dumps(self.summary if summary is None else summary) + "\n").encode()
        self.summary_path.write_bytes(raw)
        return raw

    def write_trace(self, document=None, raw=None):
        if raw is None:
            raw = gzip.compress(json.dumps(self.trace if document is None else document).encode())
        self.trace_path.write_bytes(raw)
        return raw

    def capture_fixture(self, kind="frames", window=(3, 2), title_id="PPSA00001", content_version="01.000.000", metadata_only=False):
        return performance_capture.inspect_capture(self.directory, kind, window, title_id, content_version, metadata_only)


class WindowParsing(unittest.TestCase):
    def test_valid_windows(self):
        maximum = performance_capture.UINT64_MAX
        for text, expected in (("0:1", (0, 1)), ("0003:0002", (3, 2)),
                               (f"{maximum}:{maximum}", (maximum, maximum))):
            with self.subTest(text=text):
                self.assertEqual(performance_capture.parse_window(text), expected)

    def test_invalid_windows(self):
        for value in ("", "1", "1:0", "-1:2", "1:-2", "1:2:3", "1.0:2", " 1:2", "1:2\n", "\u0661:2",
                      f"{2**64}:1", f"0:{2**64}", None, (1, 2)):
            with self.subTest(value=value), self.assertRaises(ValueError):
                performance_capture.parse_window(value)


class CaptureEnvironment(unittest.TestCase):
    def test_environment_isolated(self):
        base = {"PATH": "runtime", "APS5_PROFILE_GPU": "0", "APS5_TRACE_SECONDS": "1:2",
                "APS5_DUMP_FRAMES": "0", "APS5_SAMPLE_WORKER": "1", "aps5_trace_events": "100",
                "ANYPS5_LOG_FILE": "runtime.log"}
        extra = {"APS5_CAPTURE_INPUTS": "1", "APS5_PIPELINE_STATS": "1", "APS5_TRACE_FRAMES": "0:1"}
        before = (base.copy(), extra.copy())
        environment, removed = performance_capture.clean_environment(base, extra, ".", "frames", (3, 2))
        self.assertEqual(environment, {"PATH": "runtime", "APS5_TRACE_FRAMES": "3:2", "APS5_TRACE_DIR": str(Path.cwd())})
        self.assertEqual(set(removed), (set(base) | set(extra)) - {"PATH"})
        self.assertEqual((base, extra), before)

    def test_runtime_environment_retained(self):
        runtime = {"APS5_DRAW_PIPELINE": "1", "APS5_NO_WORDWISE_CAPTURE": "1", "APS5_NO_GPU_DUMP": "1",
                   "ANYPS5_SHADER_CACHE_DIR": "cache", "ANYPS5_NO_SHADER_CACHE": "0", "HOME": "saves",
                   "MESA_SHADER_CACHE_DIR": "driver-cache", "VK_ICD_FILENAMES": "driver", "APS5_PAD_INPUT_FILE": "input",
                   "ANYPS5_LOG_DIR": "unrecognized-untouched"}
        child, removed = performance_capture.clean_environment(runtime, {"APS5_DRAW_PIPELINE": "2"}, ".", "seconds", (30, 30))
        self.assertEqual({key: child[key] for key in runtime}, {**runtime, "APS5_DRAW_PIPELINE": "2"})
        self.assertEqual(removed, [])
        self.assertEqual(child["APS5_TRACE_SECONDS"], "30:30")
        child, _ = performance_capture.clean_environment(os.environ, {}, ".", "frames", (0, 1))
        self.assertEqual(child["APS5_TRACE_FRAMES"], "0:1")

    def test_invalid_environment(self):
        for base, extra, kind, window in (({"BAD\x00": "x"}, {}, "frames", (1, 2)),
                                           ({}, {"A": None}, "frames", (1, 2)),
                                           ({}, {}, "other", (1, 2)), ({}, {}, "frames", (True, 1))):
            with self.subTest(base=base, extra=extra, kind=kind), self.assertRaises(ValueError):
                performance_capture.clean_environment(base, extra, ".", kind, window)


class CompletedCapture(CaptureFixture):
    def test_missing_and_partial_pending(self):
        self.assertEqual(self.capture_fixture()["state"], "pending")
        raw = self.write_summary()
        self.summary_path.write_bytes(raw[:-1])
        self.assertEqual(self.capture_fixture()["state"], "pending")
        self.write_summary()
        self.assertEqual(self.capture_fixture()["state"], "pending")
        self.assertEqual(self.capture_fixture(metadata_only=True)["state"], "pending")
        trace = self.write_trace()
        self.trace_path.write_bytes(trace[:-5])
        self.assertEqual(self.capture_fixture()["state"], "pending")
        with mock.patch.object(performance_capture, "validate_trace", side_effect=AssertionError("metadata polling parsed trace")), \
                mock.patch.object(performance_capture, "read_regular", wraps=performance_capture.read_regular) as reads:
            result = self.capture_fixture(metadata_only=True)
        self.assertEqual(result["state"], "ready", result)
        self.assertEqual(result["native_summary"], self.summary)
        self.assertEqual(result["process_id"], 42)
        self.assertNotIn("artifacts", result)
        self.assertEqual([call.args[0].name for call in reads.call_args_list], ["aps5-summary.jsonl"])

    def test_complete_pair_without_screenshots(self):
        summary_raw, trace_raw = self.write_summary(), self.write_trace()
        result = self.capture_fixture()
        self.assertEqual(result["state"], "complete", result)
        self.assertEqual(result["native_summary"], self.summary)
        self.assertEqual(result["completeness"], {"events": "complete", "totals": "complete"})
        self.assertEqual(result["process_id"], 42)
        for kind, raw in (("summary", summary_raw), ("trace", trace_raw)):
            self.assertEqual(result["artifacts"][kind]["bytes"], len(raw))
            self.assertEqual(result["artifacts"][kind]["sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(self.summary_path.read_bytes(), summary_raw)
        self.assertEqual(self.trace_path.read_bytes(), trace_raw)
        self.summary["env"] = {"APS5_TRACE_SECONDS": "30:1", "APS5_TRACE_DIR": "<path>"}
        self.summary["window"] = {"spec": "seconds 30:1", "first_present": 3, "ms": 1000.0}
        self.summary["dirty"] = None
        self.write_summary()
        result = self.capture_fixture("seconds", (30, 1))
        self.assertEqual(result["state"], "complete", result)
        self.assertIsNone(result["native_summary"]["dirty"])
        self.assertTrue(result["warnings"])

    def test_native_identity_mismatch(self):
        self.write_trace()
        for field, value in (("title_id", "PPSA99999"), ("content_version", "02.000.000"), ("commit", "other")):
            with self.subTest(field=field):
                summary = {**self.summary, field: value}
                self.write_summary(summary)
                self.assertEqual(self.capture_fixture()["state"], "invalid")
        summary = copy.deepcopy(self.summary)
        summary["window"]["spec"] = "frames 0:2"
        self.write_summary(summary)
        self.assertEqual(self.capture_fixture()["state"], "invalid")
        summary = copy.deepcopy(self.summary)
        summary["window"]["first_present"] = 4
        self.write_summary(summary)
        self.assertEqual(self.capture_fixture()["state"], "invalid")
        self.summary.pop("title_id")
        self.summary.pop("content_version")
        self.write_summary()
        self.assertEqual(self.capture_fixture(title_id=None, content_version=None)["state"], "complete")


class InvalidSummaries(CaptureFixture):
    def test_instrumentation_rejected(self):
        self.write_trace()
        for name in ("APS5_PROFILE_GPU", "APS5_TRACE_BENCH", "APS5_TRACE_SUMMARY_EVERY", "APS5_DUMP_FRAMES",
                     "APS5_SAMPLE_WORKER", "APS5_FRAME_CAPTURE", "APS5_PIPELINE_STATS", "ANYPS5_LOG_FILE"):
            with self.subTest(name=name):
                summary = copy.deepcopy(self.summary)
                summary["env"][name] = "0"
                self.write_summary(summary)
                self.assertEqual(self.capture_fixture()["state"], "invalid")

    def test_invalid_summary(self):
        self.write_trace()
        for raw in (b"{}\n", b"[]\n", b"null\n", b'{"commit":"one","commit":"two"}\n', b'{"x":NaN}\n',
                    b'{"x":1e999}\n', b'{"x":\n', b"\xff\n"):
            with self.subTest(raw=raw):
                self.write_summary(raw=raw)
                self.assertEqual(self.capture_fixture()["state"], "invalid")
        for field, value in (("dirty", "false"), ("commit", ""), ("presents", 1), ("phase_totals_ms", {"Draw": -1})):
            with self.subTest(field=field):
                self.write_summary({**self.summary, field: value})
                self.assertEqual(self.capture_fixture()["state"], "invalid")

    def test_duplicate_summary_and_trace(self):
        raw = self.write_summary()
        self.write_trace()
        self.summary_path.write_bytes(raw + raw)
        self.assertEqual(self.capture_fixture()["state"], "invalid")
        self.write_summary()
        (self.directory / "aps5-trace-native123-43.json.gz").write_bytes(self.trace_path.read_bytes())
        self.assertEqual(self.capture_fixture()["state"], "invalid")


class InvalidTraces(CaptureFixture):
    def test_invalid_trace(self):
        self.write_summary()
        for raw in (b"not gzip", gzip.compress(b"{}"), gzip.compress(b'{"traceEvents":[],"traceEvents":[]}'),
                    gzip.compress(b'{"traceEvents":[NaN]}'), gzip.compress(b'{"traceEvents":['),
                    gzip.compress(b'{"traceEvents":[]}') + b"trailing"):
            with self.subTest(raw=raw):
                self.write_trace(raw=raw)
                self.assertEqual(self.capture_fixture()["state"], "invalid")
        for field, value in (("pid", 43), ("tid", True), ("args", []), ("ts", float("nan"))):
            with self.subTest(field=field):
                trace = copy.deepcopy(self.trace)
                trace["traceEvents"][0][field] = value
                self.write_trace(trace)
                self.assertEqual(self.capture_fixture()["state"], "invalid")

    def test_pair_present_identity(self):
        self.write_summary()
        for operation in ("count", "range", "timestamp", "origin", "span", "process"):
            with self.subTest(operation=operation):
                trace = copy.deepcopy(self.trace)
                if operation == "count":
                    trace["traceEvents"].pop()
                elif operation == "range":
                    trace["traceEvents"][-1]["args"]["present"] = 5
                elif operation == "timestamp":
                    trace["traceEvents"][-1]["ts"] = -1
                elif operation == "origin":
                    trace["traceEvents"][-2]["ts"] = 1
                elif operation == "span":
                    trace["traceEvents"][-1]["ts"] = 50000
                else:
                    trace["traceEvents"][0]["args"]["name"] = "Other"
                self.write_trace(trace)
                self.assertEqual(self.capture_fixture()["state"], "invalid")


class BoundsAndLinks(CaptureFixture):
    def test_bounded_inputs(self):
        self.write_summary()
        self.write_trace()
        for constant in ("MAX_SUMMARY_BYTES", "MAX_TRACE_BYTES", "MAX_TRACE_JSON_BYTES", "MAX_EVENTS"):
            with self.subTest(constant=constant), mock.patch.object(performance_capture, constant, 1):
                self.assertEqual(self.capture_fixture()["state"], "invalid")
                self.assertEqual(self.capture_fixture(metadata_only=True)["state"],
                                 "invalid" if constant in ("MAX_SUMMARY_BYTES", "MAX_TRACE_BYTES") else "ready")
        for name in ("../escape.json.gz", "C:\\escape.json.gz", "/escape.json.gz", "aps5-trace-other-42.json.gz"):
            with self.subTest(name=name):
                self.write_summary({**self.summary, "trace": name})
                self.assertEqual(self.capture_fixture()["state"], "invalid")
                self.assertEqual(self.capture_fixture(metadata_only=True)["state"], "invalid")

    def test_symlink_and_hardlink_rejected(self):
        self.write_summary()
        self.write_trace()
        original = self.directory / "original"
        original.write_bytes(self.trace_path.read_bytes())
        for symbolic in (False, True):
            with self.subTest(symbolic=symbolic):
                self.trace_path.unlink(missing_ok=True)
                try:
                    if symbolic:
                        self.trace_path.symlink_to(original)
                    else:
                        os.link(original, self.trace_path)
                except (NotImplementedError, OSError):
                    if symbolic:
                        continue
                    raise
                self.assertEqual(self.capture_fixture()["state"], "invalid")
                self.assertEqual(self.capture_fixture(metadata_only=True)["state"], "invalid")


class LossEvidence(CaptureFixture):
    def test_loss_distinctions(self):
        for lost, totals in ((3, False), (0, True), (3, True), (0, False)):
            with self.subTest(lost=lost, totals=totals):
                summary = copy.deepcopy(self.summary)
                trace = copy.deepcopy(self.trace)
                summary["trace_dropped"] = True
                if lost or totals:
                    summary["trace_dropped_threads"] = {"presenter": {"events_lost": lost, "totals_full": totals}}
                if lost:
                    trace["traceEvents"].append({"name": "trace buffer full", "ph": "i", "ts": 10000.0,
                                                 "pid": 42, "tid": 1, "args": {"events_lost": lost}})
                self.write_summary(summary)
                self.write_trace(trace)
                result = self.capture_fixture()
                self.assertEqual(result["state"], "complete", result)
                expected = {"events": "incomplete" if lost else "complete", "totals": "incomplete" if totals else "complete"}
                if not lost and not totals:
                    expected = {"events": "unknown", "totals": "unknown"}
                self.assertEqual(result["completeness"], expected)
                self.assertTrue(result["warnings"])


class StrictJson(unittest.TestCase):
    def test_accepts_bom_and_plain(self):
        self.assertEqual(performance_capture.strict_json(codecs.BOM_UTF8 + b'{"a": [1, 2.5]}', "x"), {"a": [1, 2.5]})

    def test_rejects_nonfinite_nested(self):
        for raw in (b'{"a": [NaN]}', b'{"a": {"b": Infinity}}', b'[-Infinity]'):
            with self.subTest(raw=raw):
                with self.assertRaisesRegex(ValueError, "non-finite"):
                    performance_capture.strict_json(raw, "label")

    def test_rejects_duplicate_keys(self):
        with self.assertRaises(ValueError):
            performance_capture.strict_json(b'{"a": 1, "a": 2}', "label")


class InstrumentationName(unittest.TestCase):
    def test_case_folded_match(self):
        prefix = next(iter(performance_capture.INSTRUMENTATION_PREFIXES))
        self.assertTrue(performance_capture.instrumentation_name(prefix.lower() + "x"))
        self.assertFalse(performance_capture.instrumentation_name("HOME"))


class ReadRegular(unittest.TestCase):
    def test_limit_boundary_and_digest(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "f.bin"
            path.write_bytes(b"abcd")
            raw, meta = performance_capture.read_regular(path, 4)
            self.assertEqual(raw, b"abcd")
            self.assertEqual(meta, {"path": "f.bin", "bytes": 4, "sha256": hashlib.sha256(b"abcd").hexdigest()})
            with self.assertRaisesRegex(ValueError, "byte limit"):
                performance_capture.read_regular(path, 3)


if __name__ == "__main__":
    unittest.main()
