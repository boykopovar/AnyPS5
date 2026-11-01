import copy
from functools import lru_cache
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import performance_compare


SUMMARY = {
    "commit": "abc123", "dirty": False, "env": {"APS5_TRACE_FRAMES": "600:4"},
    "gpu": "GPU", "driver": "Driver 1", "os": "OS 1", "resolution": "1920x1080",
    "window": {"spec": "frames 600:4", "first_present": 600, "ms": 80.0}, "presents": 4,
    "frame_ms": {"p50": 20.0, "p90": 40.0, "p99": 40.0, "max": 40.0},
    "gpu_busy_ms_per_present": 5.0, "worker_busy_pct": {"queue worker 0x0": 50.0},
    "top_phases_ms_per_frame": {"Driver.Packet": 12.0, "Driver.Present": 2.0},
    "trace_dropped": False, "trace": "aps5-trace-abc123-7.json.gz",
}


IDENTITY = {"cpu": 'CPU "X"', "cpu_threads": 16, "memory_mib": 32000,
            "title_id": "PPSA00000", "title": "Example", "content_version": "01.002.000"}


@lru_cache(maxsize=2)
def excessive_json_value(opening, closing):
    depth = sys.getrecursionlimit() + 64
    for _ in range(8):
        value = opening * depth + "0" + closing * depth
        try:
            json.loads(value)
        except RecursionError:
            return opening * (depth + 128) + "0" + closing * (depth + 128)
        depth *= 2
    raise AssertionError("JSON decoder accepted every bounded nesting probe")


class PerformanceCompareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.baseline = copy.deepcopy(SUMMARY)
        self.candidate = copy.deepcopy(SUMMARY)
        self.candidate.update(commit="def456", trace="aps5-trace-def456-8.json.gz")
        self.before = Path(self.directory.name) / "before.jsonl"
        self.after = Path(self.directory.name) / "after.jsonl"

    def write(self, path, *records):
        path.write_text("\n" + "\n\n".join(json.dumps(record) for record in records) + "\n", encoding="utf-8")

    def run_cli(self, *args):
        argv = [str(self.before), str(self.after), *args]
        returncode = 0
        with patch.object(sys, "stdout", new_callable=io.StringIO) as output, \
                patch.object(sys, "stderr", new_callable=io.StringIO) as errors:
            try:
                performance_compare.main(argv)
            except SystemExit as error:
                returncode = error.code
        return subprocess.CompletedProcess(argv, returncode, output.getvalue(), errors.getvalue())

    def run_process(self, *args):
        return subprocess.run([sys.executable, str(Path(performance_compare.__file__)), str(self.before), str(self.after), *args],
                              capture_output=True, text=True, encoding="utf-8", timeout=5,
                              env=dict(os.environ, PYTHONIOENCODING="ascii"))

    def test_timings_and_mean_are_compared_without_inverting_percentiles(self):
        self.candidate["window"]["ms"] = 40.0
        self.candidate["frame_ms"] = {"p50": 10.0, "p90": 20.0, "p99": 20.0, "max": 20.0}
        self.candidate["gpu_busy_ms_per_present"] = 3.0
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        report = json.loads(result.stdout)
        self.assertEqual(report["metrics_ms"]["frame_ms.p99"],
                         {"baseline": 40.0, "candidate": 20.0, "change": -20.0, "change_pct": -50.0})
        self.assertEqual(report["metrics_ms"]["mean_frame_ms"]["candidate"], 10.0)
        self.assertEqual(report["metrics_ms"]["gpu_busy_ms_per_present"]["change_pct"], -40.0)
        self.assertEqual(report["configuration_differences"], {})
        self.assertEqual(report["baseline"], self.baseline)
        self.assertEqual(report["candidate"], self.candidate)

    def test_mean_uses_each_captures_own_present_count(self):
        self.candidate["presents"] = 8
        report = performance_compare.compare(self.baseline, self.candidate)
        self.assertEqual(report["metrics_ms"]["mean_frame_ms"]["change_pct"], -50.0)

    def test_zero_baseline_and_missing_top_phases_are_not_fake_improvements(self):
        self.baseline["gpu_busy_ms_per_present"] = 0.0
        self.candidate["top_phases_ms_per_frame"] = {"Driver.Packet": 18.0, "Driver.NewPhase": 1.0}
        report = performance_compare.compare(self.baseline, self.candidate)
        self.assertIsNone(report["metrics_ms"]["gpu_busy_ms_per_present"]["change_pct"])
        self.assertEqual(report["metrics_ms"]["gpu_busy_ms_per_present"]["change"], 5.0)
        self.assertEqual(report["top_phases_ms_per_frame"]["Driver.Packet"]["change_pct"], 50.0)
        for phase in ("Driver.Present", "Driver.NewPhase"):
            self.assertIsNone(report["top_phases_ms_per_frame"][phase]["change"])
            self.assertIsNone(report["top_phases_ms_per_frame"][phase]["change_pct"])
        self.assertIn("n/a", performance_compare.markdown(report))

    def test_exact_trace_selection_does_not_pick_the_last_session(self):
        self.write(self.before, self.baseline, self.candidate)
        self.write(self.after, self.candidate, self.baseline)
        original = (self.before.read_bytes(), self.after.read_bytes())
        result = self.run_cli("--baseline-trace", self.baseline["trace"], "--candidate-trace", self.candidate["trace"], "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["baseline"]["commit"], "abc123")
        self.assertEqual(report["candidate"]["commit"], "def456")
        self.assertEqual((self.before.read_bytes(), self.after.read_bytes()), original)

    def test_empty_ambiguous_missing_and_duplicate_selections_fail(self):
        self.write(self.after, self.candidate)
        for records, selector, count in (([], [], 0), ([self.baseline, self.candidate], [], 2),
                                         ([self.baseline], ["--baseline-trace", "absent"], 0),
                                         ([self.baseline, self.baseline], ["--baseline-trace", self.baseline["trace"]], 2)):
            with self.subTest(records=len(records), selector=selector):
                self.write(self.before, *records)
                result = self.run_cli(*selector)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertIn(f"found {count}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_configuration_differences_and_incomplete_evidence_are_visible(self):
        self.candidate.update(gpu="Other GPU", driver="Driver 2", os="OS 2", resolution="1280x720",
                              dirty=True, trace_dropped=True)
        self.candidate["window"].update(spec="frames 800:4", first_present=800)
        self.candidate["env"] = {"APS5_TRACE_FRAMES": "800:4", "APS5_WORKER_AFFINITY": "1"}
        report = performance_compare.compare(self.baseline, self.candidate)
        self.assertEqual(set(report["configuration_differences"]),
                         {"gpu", "driver", "os", "resolution", "window.spec", "window.first_present",
                          "env.APS5_TRACE_FRAMES", "env.APS5_WORKER_AFFINITY"})
        self.assertIsNone(report["configuration_differences"]["env.APS5_WORKER_AFFINITY"]["baseline"])
        rendered = performance_compare.markdown(report)
        for text in ("Other GPU", "Driver 2", "source tree is dirty", "trace events were dropped", "do not isolate"):
            self.assertIn(text, rendered)

    def test_trace_loss_and_totals_overflow_are_reported_separately(self):
        for label in ("baseline", "candidate"):
            for totals_full in (False, True):
                for events_lost in (None, 0, 7):
                    for args in ((), ("--json",)):
                        with self.subTest(label=label, totals_full=totals_full, events_lost=events_lost, args=args):
                            baseline = copy.deepcopy(self.baseline)
                            candidate = copy.deepcopy(self.candidate)
                            summary = baseline if label == "baseline" else candidate
                            details = {"totals_full": totals_full}
                            if events_lost is not None:
                                details["events_lost"] = events_lost
                            summary["trace_dropped"] = totals_full or bool(events_lost)
                            summary["trace_dropped_threads"] = {"queue worker 0x0": details}
                            self.write(self.before, baseline)
                            self.write(self.after, candidate)
                            result = self.run_cli(*args)
                            self.assertEqual(result.returncode, 0, result.stderr)
                            output = "\n".join(json.loads(result.stdout)["notes"]) if args else result.stdout
                            self.assertEqual(f"{label}: phase totals are incomplete" in output, totals_full)
                            self.assertEqual(f"{label}: trace events were dropped" in output, bool(events_lost))
                            if totals_full:
                                self.assertNotIn("work included in phase totals", output)

    def test_trace_loss_and_totals_overflow_on_different_threads_are_both_reported(self):
        self.candidate.update(trace_dropped=True, trace_dropped_threads={
            "gpu": {"events_lost": 3, "totals_full": False},
            "presenter": {"totals_full": True},
        })
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        notes = "\n".join(report["notes"])
        self.assertIn("candidate: phase totals are incomplete", notes)
        self.assertIn("candidate: trace events were dropped", notes)
        self.assertNotIn("work included in phase totals", notes)
        self.assertEqual(report["metrics_ms"]["frame_ms.p50"]["change"], 0)
        self.assertEqual(report["candidate"], self.candidate)

    def test_legacy_trace_loss_warning_is_preserved_without_thread_details(self):
        for dropped in (False, True):
            with self.subTest(dropped=dropped):
                self.candidate["trace_dropped"] = dropped
                report = performance_compare.compare(self.baseline, self.candidate)
                warning = "candidate: trace events were dropped; the event trace omits some of the work included in phase totals."
                self.assertEqual(warning in report["notes"], dropped)
                self.assertNotIn("phase totals are incomplete", "\n".join(report["notes"]))

    def test_invalid_trace_loss_details_are_rejected_with_location(self):
        for details in (None, [], {"worker": None}, {"worker": {"totals_full": 1}},
                        {"worker": {"events_lost": True}}, {"worker": {"events_lost": -1}}):
            with self.subTest(details=details):
                self.baseline["trace_dropped_threads"] = details
                self.write(self.before, self.baseline)
                self.write(self.after, self.candidate)
                result = self.run_cli("--json")
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertIn("before.jsonl:2:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_unknown_metadata_is_not_a_claim_of_equivalent_machines(self):
        self.baseline.update(dirty=None, gpu="", driver="")
        report = performance_compare.compare(self.baseline, self.candidate)
        notes = "\n".join(report["notes"])
        self.assertIn("state is unknown", notes)
        self.assertIn("missing configuration: gpu, driver", notes)
        self.assertIn("cannot verify the physical machine or scene", notes)
        self.assertIn("cpu", notes)
        self.assertIn("title_id", notes)

    def test_invalid_metric_and_metadata_shapes_are_rejected_with_location(self):
        cases = [(("presents",), value) for value in (0, -1, True, 1.5, 10 ** 400)]
        cases += [(("window", "ms"), value) for value in (0, -1, True, "10", None, float("nan"), float("inf"))]
        cases += [(("frame_ms", "p50"), -1), (("frame_ms", "p50"), 50), (("frame_ms",), []),
                  (("gpu_busy_ms_per_present",), float("-inf")), (("window",), None),
                  (("window", "first_present"), True), (("env",), []), (("env",), {"APS5_TRACE_FRAMES": 4}),
                  (("trace_dropped",), "false"), (("dirty",), 1), (("trace",), None),
                  (("top_phases_ms_per_frame",), []), (("top_phases_ms_per_frame",), {"phase": -1})]
        cases += [((field,), value) for field in ("cpu", "title_id", "title", "content_version")
                  for value in (None, True, 12, [])]
        cases += [((field,), value) for field in ("cpu_threads", "memory_mib")
                  for value in (None, True, -1, 1.5, "16")]
        for path, value in cases:
            with self.subTest(path=path, value=value):
                invalid = copy.deepcopy(self.baseline)
                target = invalid
                for key in path[:-1]:
                    target = target[key]
                target[path[-1]] = value
                self.write(self.before, invalid)
                with self.assertRaisesRegex(ValueError, r"before.jsonl:2:"):
                    performance_compare.load_summary(self.before)

    def test_invalid_worker_busy_percentages_are_rejected_with_location(self):
        for workers in (None, [], {"worker": "busy"}, {"worker": True},
                        {"worker": -1}, {"worker": float("inf")}):
            with self.subTest(workers=workers):
                self.baseline["worker_busy_pct"] = workers
                self.write(self.before, self.baseline)
                with self.assertRaisesRegex(ValueError, r"before.jsonl:2: worker_busy_pct"):
                    performance_compare.load_summary(self.before)

    def test_worker_busy_share_above_one_hundred_remains_valid(self):
        self.baseline["worker_busy_pct"] = {"queue worker 0x0": 125.0}
        self.assertIs(performance_compare.validate(self.baseline), self.baseline)

    def test_bad_json_and_missing_file_produce_only_a_cli_error(self):
        self.write(self.after, self.candidate)
        for content in ("{bad json", "[]", "{}", '{"frame_ms": Infinity}'):
            with self.subTest(content=content):
                self.before.write_text(content, encoding="utf-8")
                result = self.run_cli("--json")
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertIn("before.jsonl:1:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
        self.before.unlink()
        result = self.run_cli()
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertNotIn("Traceback", result.stderr)

    def test_duplicate_json_fields_are_rejected_at_every_object_level(self):
        record = json.dumps(self.baseline)
        duplicates = (
            (record.replace('"commit": "abc123"', '"commit": "wrong", "commit": "abc123"', 1), "commit"),
            (record.replace('"spec": "frames 600:4"', '"spec": "seconds 0:1", "spec": "frames 600:4"', 1), "spec"),
            (record.replace('"Driver.Packet": 12.0', '"Driver.Packet": 1.0, "Driver.Packet": 12.0', 1), "Driver.Packet"),
        )
        for content, field in duplicates:
            with self.subTest(field=field):
                self.before.write_text(content + "\n", encoding="utf-8")
                original = self.before.read_bytes()
                with self.assertRaises(ValueError) as raised:
                    performance_compare.load_summary(self.before)
                self.assertIn(f"before.jsonl:1: duplicate JSON field: {field!r}", str(raised.exception))
                self.assertIsInstance(raised.exception.__cause__, ValueError)
                self.assertEqual(self.before.read_bytes(), original)

    def test_percentage_overflow_is_an_error(self):
        self.baseline["gpu_busy_ms_per_present"] = 1e-308
        self.candidate["gpu_busy_ms_per_present"] = 1e308
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--json")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn("finite numeric range", result.stderr)

    def test_excessive_json_nesting_reports_the_input_location(self):
        for opening, closing in (("[", "]"), ('{"nested":', "}")):
            with self.subTest(opening=opening):
                record = json.dumps(self.baseline)[:-1] + ', "extra": ' + excessive_json_value(opening, closing) + "}"
                self.before.write_text("\n\n" + record + "\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, r"before.jsonl:3:") as error:
                    performance_compare.load_summary(self.before)
                self.assertIsInstance(error.exception.__cause__, RecursionError)

    def test_excessive_json_nesting_produces_only_a_cli_error(self):
        cases = ((self.before, "[", "]", ()), (self.before, '{"nested":', "}", ("--json",)),
                 (self.after, "[", "]", ("--json",)), (self.after, '{"nested":', "}", ()))
        for path, opening, closing, args in cases:
            with self.subTest(path=path.name, opening=opening, args=args):
                self.write(self.before, self.baseline)
                self.write(self.after, self.candidate)
                record = json.dumps(self.baseline)[:-1] + ', "extra": ' + excessive_json_value(opening, closing) + "}"
                path.write_text("\n\n" + record + "\n", encoding="utf-8")
                original = (self.before.read_bytes(), self.after.read_bytes())
                result = self.run_process(*args)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn(f"{path}:3:", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertEqual((self.before.read_bytes(), self.after.read_bytes()), original)

    def test_json_report_recursion_produces_only_a_cli_error(self):
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        with patch.object(performance_compare.json, "dumps", side_effect=RecursionError("report nesting is too deep")), \
                patch.object(sys, "stdout", new_callable=io.StringIO) as output, \
                patch.object(sys, "stderr", new_callable=io.StringIO) as errors:
            with self.assertRaises(SystemExit) as error:
                performance_compare.main([str(self.before), str(self.after), "--json"])
        self.assertEqual(error.exception.code, 2)
        self.assertEqual(output.getvalue(), "")
        self.assertIn("JSON report: report nesting is too deep", errors.getvalue())
        self.assertNotIn("Traceback", errors.getvalue())

    def test_supported_nested_metadata_is_preserved(self):
        metadata = {"value": 7}
        for depth in range(16):
            metadata = {"nested": [metadata]}
        self.baseline["extra"] = metadata
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        with patch.object(sys, "stdout", new_callable=io.StringIO) as output, \
                patch.object(sys, "stderr", new_callable=io.StringIO) as errors:
            performance_compare.main([str(self.before), str(self.after), "--json"])
            self.assertEqual(errors.getvalue(), "")
            self.assertEqual(json.loads(output.getvalue())["baseline"]["extra"], metadata)

    def test_markdown_reports_deltas_and_escapes_table_values(self):
        self.candidate["frame_ms"]["max"] = 60.0
        self.candidate["gpu"] = "GPU | <vendor>\nline"
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("| frame_ms.max | 40.000 | 60.000 | +20.000 | +50.000 |", result.stdout)
        self.assertIn("GPU &#124; &lt;vendor&gt; line", result.stdout)
        self.assertIn("abc123", result.stdout)
        self.assertIn("def456", result.stdout)

    def test_utf8_paths_and_metadata_work_with_a_non_utf8_output_default(self):
        self.before = Path(self.directory.name) / "avant-\u00e9.jsonl"
        self.candidate["gpu"] = "GPU \u03a9"
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        for args in ((), ("--json",)):
            with self.subTest(args=args):
                result = self.run_process(*args)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("GPU \u03a9", result.stdout)
        self.before.write_text("bad", encoding="utf-8")
        result = self.run_process()
        self.assertEqual(result.returncode, 2)
        self.assertIn("avant-\u00e9.jsonl:1:", result.stderr)

    def test_bom_summary_loads(self):
        self.before.write_text(json.dumps(self.baseline) + "\n", encoding="utf-8-sig")
        self.write(self.after, self.candidate)
        self.assertEqual(performance_compare.load_summary(self.before), self.baseline)
        result = self.run_cli("--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["baseline"], self.baseline)

    def test_matching_recorded_machine_and_title_context_is_accepted(self):
        self.baseline.update(IDENTITY)
        self.candidate.update(IDENTITY)
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--require-matching-context", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["configuration_differences"], {})
        self.assertEqual(report["baseline"], self.baseline)
        self.assertEqual(report["candidate"], self.candidate)
        self.assertNotIn("missing configuration", "\n".join(report["notes"]))
        for key in ("cpu_threads", "memory_mib"):
            with self.subTest(unknown=key):
                self.baseline[key] = self.candidate[key] = 0
                performance_compare.validate(self.baseline)
                report = performance_compare.compare(self.baseline, self.candidate)
                self.assertEqual(report["configuration_differences"], {})
                self.assertIn(key, "\n".join(report["notes"]))

    def test_recorded_identity_differences_are_reported_and_rejected_in_strict_mode(self):
        self.baseline.update(IDENTITY)
        self.write(self.before, self.baseline)
        changes = {"cpu": "Other CPU", "cpu_threads": 8, "memory_mib": 16000,
                   "title_id": "PPSA00001", "title": "Other title", "content_version": "01.003.000"}
        for key, value in changes.items():
            with self.subTest(key=key):
                candidate = {**self.candidate, **IDENTITY, key: value}
                expected = {key: {"baseline": IDENTITY[key], "candidate": value}}
                self.assertEqual(performance_compare.compare(self.baseline, candidate)["configuration_differences"], expected)
                self.write(self.after, candidate)
                result = self.run_cli("--json")
                self.assertEqual(result.returncode, 0, result.stderr)
                report = json.loads(result.stdout)
                self.assertEqual(report["configuration_differences"], expected)
                self.assertEqual(report["metrics_ms"]["frame_ms.p50"]["change"], 0)
                result = self.run_cli("--require-matching-context", "--json")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn(f"recorded configuration differs: {key}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_missing_identity_is_distinct_from_known_identity_but_legacy_summaries_still_work(self):
        for key in IDENTITY:
            for label in ("baseline", "candidate"):
                with self.subTest(key=key, missing=label):
                    baseline = {**self.baseline, **IDENTITY}
                    candidate = {**self.candidate, **IDENTITY}
                    (baseline if label == "baseline" else candidate).pop(key)
                    report = performance_compare.compare(baseline, candidate)
                    self.assertEqual(report["configuration_differences"],
                                     {key: {"baseline": baseline.get(key), "candidate": candidate.get(key)}})
                    self.assertIn(f"{label}: missing configuration: {key}.", report["notes"])
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--require-matching-context", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["configuration_differences"], {})
        self.assertIn("cpu", "\n".join(report["notes"]))
        self.assertIn("title_id", "\n".join(report["notes"]))
        self.write(self.after, {**self.candidate, **IDENTITY})
        result = self.run_cli("--require-matching-context")
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(result.stdout, "")
        for key in IDENTITY:
            self.assertIn(key, result.stderr)

    def test_recorded_identity_differences_are_escaped_in_markdown(self):
        self.baseline.update(IDENTITY)
        self.candidate.update(IDENTITY, cpu="CPU | <\u03a9>\nline", title="Other title", content_version="02.000.000")
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CPU &#124; &lt;\u03a9&gt; line", result.stdout)
        self.assertIn("| title | Example | Other title |", result.stdout)
        self.assertIn("| content_version | 01.002.000 | 02.000.000 |", result.stdout)

    def test_matching_context_can_be_required_without_a_timing_verdict(self):
        self.write(self.before, self.baseline)
        self.candidate["frame_ms"]["max"] = 100.0
        self.write(self.after, self.candidate)
        result = self.run_cli("--require-matching-context", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["metrics_ms"]["frame_ms.max"]["change"], 60.0)
        for field in ("gpu", "driver", "os", "resolution"):
            with self.subTest(field=field):
                candidate = copy.deepcopy(self.candidate)
                candidate[field] += " changed"
                self.write(self.after, candidate)
                result = self.run_cli("--require-matching-context")
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(field, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertEqual(self.run_cli().returncode, 0)
        for key, value in (("window", dict(self.candidate["window"], first_present=900)),
                           ("env", {"APS5_TRACE_FRAMES": "900:4"})):
            with self.subTest(key=key):
                candidate = copy.deepcopy(self.candidate)
                candidate[key] = value
                self.write(self.after, candidate)
                self.assertEqual(self.run_cli("--require-matching-context").returncode, 2)

    def test_timed_windows_allow_different_first_present_counters(self):
        for summary in (self.baseline, self.candidate):
            summary["env"] = {"APS5_TRACE_SECONDS": "270:20"}
            summary["window"].update(spec="seconds 270:20", ms=20000.0)
        self.baseline["window"]["first_present"] = 1010
        self.candidate["window"]["first_present"] = 700
        self.baseline["presents"] = 20
        self.candidate["presents"] = 40
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--require-matching-context", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["configuration_differences"], {})
        self.assertEqual(report["baseline"], self.baseline)
        self.assertEqual(report["candidate"], self.candidate)
        self.assertEqual(report["metrics_ms"]["mean_frame_ms"]["change_pct"], -50.0)

    def test_timed_windows_still_require_matching_requested_windows(self):
        for summary in (self.baseline, self.candidate):
            summary["env"] = {"APS5_TRACE_SECONDS": "270:20"}
            summary["window"]["spec"] = "seconds 270:20"
        self.candidate["window"].update(spec="seconds 280:20", first_present=700)
        report = performance_compare.compare(self.baseline, self.candidate)
        self.assertEqual(set(report["configuration_differences"]), {"window.spec"})
        self.write(self.before, self.baseline)
        self.write(self.after, self.candidate)
        result = self.run_cli("--require-matching-context")
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("window.spec", result.stderr)

    def test_unknown_window_syntax_keeps_first_present_check(self):
        for summary in (self.baseline, self.candidate):
            summary["window"]["spec"] = "seconds unknown"
        self.candidate["window"]["first_present"] = 700
        report = performance_compare.compare(self.baseline, self.candidate)
        self.assertEqual(set(report["configuration_differences"]), {"window.first_present"})



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
        with patch.object(performance_compare.sys, "stdout", out), patch.object(performance_compare.sys, "stderr", err):
            try:
                performance_compare.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
