import copy
import gzip
import hashlib
import io
import json
import math
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import benchmark_compare


BASE = "a" * 40
CHANGE = "b" * 40


def benchmark_interval(frames, start=0, end=1, first=0, resolution="1920x1080", gpu=20):
    return {"start_s": start, "end_s": end, "first_present": first,
            "resolution": resolution, "gpu_busy_ms": gpu, "frame_us": list(frames)}


def benchmark_capture(intervals=None, commit=BASE):
    return {"format": "aps5-bench/1", "commit": commit, "dirty": False,
            "env": {"APS5_TRACE_SUMMARY_EVERY": "1", "APS5_TRACE_BENCH": "1"},
            "gpu": "GPU", "driver": "Driver", "os": "OS", "cpu": "CPU",
            "cpu_threads": 8, "memory_mib": 16384, "title_id": "PPSA00000",
            "title": "Example", "content_version": "01.000.000", "intervals_dropped": 0,
            "intervals": intervals if intervals is not None else [benchmark_interval([10000] * 100)]}


class BenchmarkCompareTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def write_capture(self, name="capture.json.gz", data=None, compressed=True):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        data = benchmark_capture() if data is None else data
        raw = json.dumps(data, ensure_ascii=False).encode("utf-8")
        path.write_bytes(gzip.compress(raw, mtime=0) if compressed else raw)
        return path

    def summarize(self, data, intervals=None):
        loaded = benchmark_compare.load_benchmark(self.write_capture(data=data))
        return benchmark_compare.summarize_benchmark(loaded, intervals)

    def write_series(self, runs, recipe=None, name="series.json"):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        data = {"format": "aps5-bench-series/1", "runs": runs}
        if recipe is not None:
            data["recipe"] = recipe
        path.write_text(json.dumps(data, ensure_ascii=False), encoding="utf-8")
        return path

    def series_run(self, run_id, arm, pair, frames, commit=None):
        data = benchmark_capture([benchmark_interval(frames)], commit or (BASE if arm == "baseline" else CHANGE))
        data["producer_extension"] = run_id
        path = self.write_capture(run_id + ".json.gz", data)
        return {"id": run_id, "arm": arm, "pair": pair, "capture": path.name}

    def abba(self):
        return [self.series_run("A1", "baseline", "1", [10000] * 100),
                self.series_run("B1", "candidate", "1", [5000] * 200),
                self.series_run("B2", "candidate", "2", [2000] * 500),
                self.series_run("A2", "baseline", "2", [2500] * 400)]

    def cli(self, *arguments):
        status = 0
        with patch.object(sys, "stdout", new_callable=io.StringIO) as output, \
                patch.object(sys, "stderr", new_callable=io.StringIO) as errors:
            try:
                status = benchmark_compare.main(list(map(str, arguments))) or 0
            except SystemExit as error:
                status = error.code
        return subprocess.CompletedProcess(arguments, status, output.getvalue(), errors.getvalue())

    def test_raw_frames_weight_mean_and_native_percentiles(self):
        data = benchmark_capture([benchmark_interval([10000] * 100, end=1),
                                  benchmark_interval([100000] * 2, start=1, end=1.2, first=100)])
        result = self.summarize(data)
        metrics = result["metrics"]
        self.assertEqual(metrics["frames"], 102)
        self.assertAlmostEqual(metrics["elapsed_s"], 1.2)
        self.assertAlmostEqual(metrics["fps"], 85)
        self.assertAlmostEqual(metrics["mean_frame_ms"], 1200 / 102)
        self.assertEqual(metrics["frame_ms"], {"p50": 10, "p90": 10, "p99": 100, "max": 100})
        self.assertAlmostEqual(metrics["gpu_busy_ms_per_frame"], 40 / 102)
        self.assertEqual([entry["index"] for entry in result["intervals"]], [0, 1])

    def test_percentile_order_matches_native_floor_fraction_times_count(self):
        metrics = self.summarize(benchmark_capture([benchmark_interval([1000, 2000, 3000, 4000])]))["metrics"]
        self.assertEqual(metrics["frame_ms"], {"p50": 3, "p90": 4, "p99": 4, "max": 4})

    def test_wall_throughput_does_not_use_quantized_or_inconsistent_raw_elapsed(self):
        result = self.summarize(benchmark_capture([benchmark_interval([100000], end=2)]))
        self.assertEqual(result["metrics"]["fps"], 0.5)
        self.assertEqual(result["metrics"]["mean_frame_ms"], 100)
        self.assertTrue(result["warnings"])

    def test_selected_complete_intervals_exclude_startup_and_keep_ordinals(self):
        data = benchmark_capture([benchmark_interval([1000000] * 2, end=2),
                                  benchmark_interval([10000] * 100, start=2, end=3, first=2),
                                  benchmark_interval([20000] * 50, start=3, end=4, first=102)])
        result = self.summarize(data, (1, 2))
        self.assertEqual(result["selection"], {"start": 1, "count": 2, "indices": [1, 2],
                                             "start_s": 2, "end_s": 4, "first_present": 2, "last_present": 152})
        self.assertEqual(result["interval_count"], 3)
        self.assertEqual(result["metrics"]["frames"], 150)
        self.assertEqual(result["metrics"]["fps"], 75)
        self.assertEqual(result["metrics"]["frame_ms"]["max"], 20)
        self.assertEqual(self.summarize(data)["metrics"]["frame_ms"]["max"], 1000)

    def test_selection_requires_retained_complete_bounds(self):
        loaded = benchmark_compare.load_benchmark(self.write_capture())
        for selection in ((-1, 1), (0, 0), (0, -1), (1, 1), (0, 2), (True, 1), (0, True), (0.5, 1)):
            with self.subTest(selection=selection), self.assertRaises(ValueError):
                benchmark_compare.summarize_benchmark(loaded, selection)

    def test_dropped_history_is_not_missing_selected_coverage(self):
        data = benchmark_capture([benchmark_interval([10000] * 100, start=10, end=11, first=1000)])
        data["intervals_dropped"] = 10
        result = self.summarize(data, (0, 1))
        self.assertEqual(result["intervals_dropped"], 10)
        self.assertEqual(result["metrics"]["fps"], 100)
        self.assertTrue(any("drop" in warning.lower() for warning in result["warnings"]))
        with self.assertRaises(ValueError):
            self.summarize(data, (0, 2))

    def test_gap_outside_selection_warns_but_selected_gap_fails(self):
        for second in (benchmark_interval([10000] * 100, start=2, end=3, first=100),
                       benchmark_interval([10000] * 100, start=1, end=2, first=101)):
            with self.subTest(second=second):
                data = benchmark_capture([benchmark_interval([10000] * 100), second])
                self.assertTrue(self.summarize(data, (1, 1))["warnings"])
                with self.assertRaises(ValueError):
                    self.summarize(data)

    def test_overlapping_or_out_of_order_intervals_fail_even_outside_selection(self):
        for second in (benchmark_interval([10000] * 100, start=0.5, end=1.5, first=100),
                       benchmark_interval([10000] * 100, start=0, end=0.5, first=100),
                       benchmark_interval([10000] * 100, start=1, end=2, first=50)):
            with self.subTest(second=second), self.assertRaises(ValueError):
                self.summarize(benchmark_capture([benchmark_interval([10000] * 100), second]), (0, 1))

    def test_empty_capture_and_empty_frames_are_unknown_not_zero_performance(self):
        result = self.summarize(benchmark_capture([]))
        self.assertEqual(result["metrics"]["frames"], 0)
        self.assertIsNone(result["metrics"]["fps"])
        with self.assertRaises(ValueError):
            self.summarize(benchmark_capture([]), (0, 1))
        metrics = self.summarize(benchmark_capture([benchmark_interval([])]))["metrics"]
        for key in ("fps", "mean_frame_ms", "gpu_busy_ms_per_frame"):
            self.assertIsNone(metrics[key])
        self.assertTrue(all(value is None for value in metrics["frame_ms"].values()))

    def test_zero_microsecond_frames_are_legal_without_infinite_throughput(self):
        metrics = self.summarize(benchmark_capture([benchmark_interval([0, 0], end=0.001)]))["metrics"]
        self.assertEqual(metrics["zero_frames"], 2)
        self.assertEqual(metrics["fps"], 2000)
        self.assertEqual(metrics["mean_frame_ms"], 0)
        self.assertEqual(metrics["frame_ms"]["max"], 0)

    def test_saturation_censors_raw_metrics_but_keeps_wall_fps(self):
        result = self.summarize(benchmark_capture([benchmark_interval([1000, 4294967295], end=4300)]))
        metrics = result["metrics"]
        self.assertEqual(metrics["saturated_frames"], 1)
        self.assertIsNone(metrics["mean_frame_ms"])
        self.assertTrue(all(value is None for value in metrics["frame_ms"].values()))
        self.assertAlmostEqual(metrics["fps"], 2 / 4300)
        self.assertTrue(result["warnings"])

    def test_gzip_magic_and_original_and_decoded_hashes_are_preserved(self):
        data = benchmark_capture()
        path = self.write_capture("native.data", data)
        original = path.read_bytes()
        loaded = benchmark_compare.load_benchmark(path)
        self.assertEqual(loaded["data"], data)
        self.assertEqual(loaded["source"]["sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual(loaded["source"]["decoded_sha256"], hashlib.sha256(gzip.decompress(original)).hexdigest())
        self.assertEqual(loaded["source"]["bytes"], len(original))
        plain = self.write_capture("plain.json.gz", data, compressed=False)
        self.assertEqual(benchmark_compare.load_benchmark(plain)["data"], data)

    def test_compatible_native_extensions_are_allowed(self):
        data = benchmark_capture()
        data["future_producer_field"] = {"value": 1}
        self.assertEqual(benchmark_compare.validate_benchmark(data), data)

    def test_missing_required_fields_and_invalid_native_metadata_are_refused(self):
        original = benchmark_capture()
        for key in ("format", "commit", "dirty", "env", "gpu", "driver", "os", "intervals_dropped", "intervals"):
            data = copy.deepcopy(original)
            del data[key]
            with self.subTest(missing=key), self.assertRaises(ValueError):
                benchmark_compare.validate_benchmark(data)
        for key, value in (("format", "aps5-bench/2"), ("commit", 3), ("dirty", 1), ("env", []),
                           ("gpu", None), ("intervals_dropped", True), ("intervals_dropped", -1),
                           ("cpu_threads", -1), ("memory_mib", 1 << 64), ("title_id", [])):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                benchmark_compare.validate_benchmark(dict(original, **{key: value}))

    def test_interval_numeric_and_frame_types_are_validated(self):
        replacements = (("start_s", -1), ("start_s", True), ("end_s", 0), ("end_s", math.inf),
                        ("end_s", math.nan), ("first_present", -1), ("first_present", True),
                        ("first_present", 1 << 64), ("gpu_busy_ms", -1), ("gpu_busy_ms", math.inf),
                        ("resolution", ""), ("frame_us", "1000"), ("frame_us", [True]),
                        ("frame_us", [-1]), ("frame_us", [4294967296]), ("frame_us", [1.5]))
        for key, value in replacements:
            data = benchmark_capture()
            data["intervals"][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                benchmark_compare.validate_benchmark(data)

    def test_string_environment_and_interval_bounds(self):
        for mutate in (lambda data: data.update(gpu="x" * 4097),
                       lambda data: data.update(env={str(index): "x" for index in range(129)}),
                       lambda data: data.update(env={"APS5_TRACE_BENCH": 1}),
                       lambda data: data.update(intervals=[benchmark_interval([], start=index, end=index + 1)
                                                           for index in range(241)])):
            data = benchmark_capture()
            mutate(data)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                benchmark_compare.validate_benchmark(data)

    def test_frame_and_decoded_and_raw_read_bounds_are_enforced(self):
        path = self.write_capture()
        with patch.object(benchmark_compare, "MAX_FILE_BYTES", 8), self.assertRaises(ValueError):
            benchmark_compare.load_benchmark(path)
        with patch.object(benchmark_compare, "MAX_JSON_BYTES", 64), self.assertRaises(ValueError):
            benchmark_compare.load_benchmark(path)
        with patch.object(benchmark_compare, "MAX_FRAMES", 99), self.assertRaises(ValueError):
            benchmark_compare.load_benchmark(path)

    def test_corrupt_truncated_invalid_utf8_and_duplicate_json_are_controlled_errors(self):
        original = self.write_capture().read_bytes()
        duplicate = json.dumps(benchmark_capture())[:-1] + ', "dirty": false}'
        invalid = (original[:len(original) // 2], b"\x1f\x8bnot-gzip", b"\xff", b"[]", b"{} trailing",
                   bytes.fromhex("1f8b0800000000000203") + b"\x07" + b"\0" * 8,
                   duplicate.encode(), b'{"nested":' + b'[' * 2000 + b'0' + b']' * 2000 + b'}')
        for index, value in enumerate(invalid):
            path = self.root / (str(index) + ".json")
            path.write_bytes(value)
            with self.subTest(index=index), self.assertRaises((ValueError, OSError)):
                benchmark_compare.load_benchmark(path)

    def test_abba_averages_paired_run_effects_without_pooling_frames(self):
        path = self.write_series(self.abba(), {"scene": "stationary menu", "cache": "operator says warm"})
        report = benchmark_compare.compare_benchmark_runs(path)
        self.assertEqual(report["format"], "aps5-bench-comparison/1")
        self.assertEqual([run["id"] for run in report["runs"]], ["A1", "B1", "B2", "A2"])
        self.assertEqual([pair["metrics"]["fps"]["change_pct"] for pair in report["pairs"]], [100, 25])
        self.assertEqual(report["aggregate"]["complete_pairs"], 2)
        self.assertEqual(report["aggregate"]["metrics"]["fps"],
                         {"mean_change_pct": 62.5, "min_change_pct": 25, "max_change_pct": 100, "n": 2})
        self.assertEqual(report["recipe"]["scene"], "stationary menu")
        self.assertEqual(report["manifest"]["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())

    def test_failed_and_excluded_attempts_are_retained_without_invented_pairs(self):
        runs = self.abba()[:2] + [{"id": "B2", "arm": "candidate", "pair": "2", "status": "failed", "reason": "no capture"},
                                  {"id": "A2", "arm": "baseline", "pair": "2", "status": "excluded", "reason": "wrong scene"}]
        report = benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        self.assertEqual(len(report["runs"]), 4)
        self.assertEqual(report["runs"][2]["reason"], "no capture")
        self.assertEqual(report["runs"][3]["status"], "excluded")
        self.assertEqual(report["aggregate"]["complete_pairs"], 1)
        self.assertEqual(report["aggregate"]["metrics"]["fps"]["n"], 1)
        self.assertFalse(report["pairs"][1]["complete"])

    def test_undefined_paired_percentages_do_not_inflate_sample_count(self):
        runs = [self.series_run("A1", "baseline", "1", [0]),
                self.series_run("B1", "candidate", "1", [1000])]
        report = benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        self.assertTrue(report["pairs"][0]["complete"])
        self.assertIsNone(report["pairs"][0]["metrics"]["mean_frame_ms"]["change_pct"])
        self.assertEqual(report["aggregate"]["metrics"]["mean_frame_ms"]["n"], 0)
        self.assertIsNone(report["aggregate"]["metrics"]["mean_frame_ms"]["mean_change_pct"])
        self.assertEqual(report["aggregate"]["metrics"]["fps"]["n"], 1)
        self.assertTrue(report["warnings"])

    def test_relative_capture_paths_use_manifest_directory_not_working_directory(self):
        runs = self.abba()[:2]
        path = self.write_series(runs)
        previous = os.getcwd()
        try:
            os.chdir(self.root.parent)
            report = benchmark_compare.compare_benchmark_runs(path)
        finally:
            os.chdir(previous)
        self.assertEqual(report["aggregate"]["complete_pairs"], 1)

    def test_recorded_context_changes_report_and_strict_mode_refuses(self):
        for key, value in (("gpu", "Other GPU"), ("driver", "Other driver"), ("os", "Other OS"),
                           ("cpu", "Other CPU"), ("title_id", "PPSA00001"), ("content_version", "02.000.000"),
                           ("env", {"APS5_TRACE_SUMMARY_EVERY": "2"})):
            with self.subTest(key=key):
                runs = self.abba()[:2]
                capture = self.root / runs[1]["capture"]
                data = json.loads(gzip.decompress(capture.read_bytes()))
                data[key] = value
                self.write_capture(capture.name, data)
                path = self.write_series(runs)
                report = benchmark_compare.compare_benchmark_runs(path)
                self.assertTrue(report["configuration_differences"])
                with self.assertRaises(ValueError):
                    benchmark_compare.compare_benchmark_runs(path, require_matching_context=True)

    def test_same_context_different_arm_commits_is_allowed_but_arm_drift_is_reported(self):
        path = self.write_series(self.abba())
        self.assertEqual(benchmark_compare.compare_benchmark_runs(path, True)["configuration_differences"], [])
        data = benchmark_capture([benchmark_interval([2500] * 400)], "c" * 40)
        self.write_capture("A2.json.gz", data)
        self.assertTrue(benchmark_compare.compare_benchmark_runs(path)["configuration_differences"])
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(path, True)

    def test_resolution_and_selected_ordinals_are_checked_as_context(self):
        for change in ("resolution", "selection"):
            runs = self.abba()[:2]
            data = benchmark_capture([benchmark_interval([5000] * 200)], CHANGE)
            if change == "resolution":
                data["intervals"][0]["resolution"] = "3840x2160"
            else:
                data["intervals"].insert(0, benchmark_interval([1000000], start=0, end=1))
                data["intervals"][1].update(start_s=1, end_s=2, first_present=1)
                runs[1]["intervals"] = [1, 1]
            self.write_capture("B1.json.gz", data)
            with self.subTest(change=change), self.assertRaises(ValueError):
                benchmark_compare.compare_benchmark_runs(self.write_series(runs), True)

    def test_dirty_and_missing_identity_are_warnings(self):
        data = benchmark_capture()
        data["dirty"] = None
        for field in ("cpu", "cpu_threads", "memory_mib", "title_id", "title", "content_version"):
            del data[field]
        result = self.summarize(data)
        self.assertTrue(any("dirty" in warning.lower() or "unknown" in warning.lower() for warning in result["warnings"]))
        self.assertNotIn("title_id", result["metadata"])

    def test_manifest_typos_duplicate_ids_and_pair_members_are_refused(self):
        original = self.abba()[:2]
        for mutate in (lambda runs: runs[0].update(interals=[0, 1]),
                       lambda runs: runs[1].update(id="A1"),
                       lambda runs: runs[1].update(arm="baseline"),
                       lambda runs: runs[1].update(pair=""),
                       lambda runs: runs[1].update(arm="other"),
                       lambda runs: runs[1].update(status="failed"),
                       lambda runs: runs[1].update(status="failed", reason="retained", capture="B1.json.gz")):
            runs = copy.deepcopy(original)
            mutate(runs)
            with self.subTest(mutate=mutate), self.assertRaises(ValueError):
                benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        path = self.write_series(original)
        data = json.loads(path.read_text())
        data["recpie"] = {}
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(path)

    def test_capture_hash_mismatch_and_reused_evidence_are_refused(self):
        runs = self.abba()[:2]
        runs[0]["sha256"] = "0" * 64
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        runs[0].pop("sha256")
        runs[1]["capture"] = runs[0]["capture"]
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        source = self.root / runs[0]["capture"]
        alias = self.root / "alias.json.gz"
        os.link(source, alias)
        runs[1]["capture"] = alias.name
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(self.write_series(runs))

    def test_expected_raw_hash_is_verified_and_retained(self):
        runs = self.abba()[:2]
        expected = hashlib.sha256((self.root / runs[0]["capture"]).read_bytes()).hexdigest()
        runs[0]["sha256"] = expected
        report = benchmark_compare.compare_benchmark_runs(self.write_series(runs))
        self.assertEqual(report["runs"][0]["summary"]["source"]["sha256"], expected)

    def test_decoded_duplicate_is_not_counted_as_another_independent_run(self):
        runs = self.abba()[:2]
        original = gzip.decompress((self.root / runs[0]["capture"]).read_bytes())
        (self.root / runs[1]["capture"]).write_bytes(gzip.compress(original, mtime=1))
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(self.write_series(runs))

    def test_series_run_manifest_and_total_frame_limits(self):
        path = self.write_series(self.abba())
        for constant, limit in (("MAX_RUNS", 3), ("MAX_MANIFEST_BYTES", 16), ("MAX_TOTAL_FRAMES", 1000)):
            with self.subTest(constant=constant), patch.object(benchmark_compare, constant, limit), self.assertRaises(ValueError):
                benchmark_compare.compare_benchmark_runs(path)
        path = self.write_series(self.abba()[:2], {"scene": "x" * 4096})
        decoded = path.read_bytes()
        compressed = gzip.compress(decoded, mtime=0)
        self.assertLess(len(compressed), 1024)
        self.assertGreater(len(decoded), 1024)
        path.write_bytes(compressed)
        with patch.object(benchmark_compare, "MAX_MANIFEST_BYTES", 1024), self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(path)

    def test_duplicate_manifest_json_fields_fail(self):
        path = self.write_series(self.abba()[:2])
        path.write_text(path.read_text()[:-1] + ', "runs": []}', encoding="utf-8")
        with self.assertRaises(ValueError):
            benchmark_compare.compare_benchmark_runs(path)

    def test_json_and_markdown_cli_keep_inputs_and_never_write_output(self):
        capture = self.write_capture("capture space λ.json.gz")
        series = self.write_series(self.abba()[:2])
        before = {path: path.read_bytes() for path in self.root.iterdir() if path.is_file()}
        for arguments in (("summarize", capture, "--json"), ("summarize", capture),
                          ("compare", series, "--json"), ("compare", series)):
            result = self.cli(*arguments)
            self.assertEqual(result.returncode, 0, result.stderr)
            if "--json" in arguments:
                self.assertIsInstance(json.loads(result.stdout), dict)
            else:
                self.assertTrue(result.stdout.strip())
        self.assertEqual({path: path.read_bytes() for path in self.root.iterdir() if path.is_file()}, before)
        result = self.cli("summarize", capture, "--output", capture)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(capture.read_bytes(), before[capture])

    def test_comparison_markdown_retains_recorded_identity_and_attempts(self):
        runs = self.abba()[:2]
        identity = {"gpu": "FixtureGPU alpha", "driver": "Fixture driver 42", "os": "FixtureOS 27",
                    "title": "Fixture benchmark title", "content_version": "07.001.002"}
        for run in runs:
            path = self.root / run["capture"]
            data = json.loads(gzip.decompress(path.read_bytes()))
            data.update(identity)
            self.write_capture(path.name, data)
        runs.extend([{"id": "B2", "arm": "candidate", "pair": "2", "status": "failed", "reason": "no capture"},
                     {"id": "A2", "arm": "baseline", "pair": "2", "status": "excluded", "reason": "wrong scene"}])
        result = self.cli("compare", self.write_series(runs))
        self.assertEqual(result.returncode, 0, result.stderr)
        for value in (BASE, CHANGE, "PPSA00000", *identity.values(), "A1", "B1", "B2", "A2",
                      "failed", "excluded", "no capture", "wrong scene"):
            self.assertIn(value, result.stdout)
        self.assertIn("dirty", result.stdout.lower())
        self.assertIn("false", result.stdout.lower())
        self.assertLess(result.stdout.index("A1"), result.stdout.index("B1"))
        self.assertLess(result.stdout.index("B1"), result.stdout.index("B2"))
        self.assertLess(result.stdout.index("B2"), result.stdout.index("A2"))

    def test_cli_selection_and_malformed_input_fail_without_traceback(self):
        capture = self.write_capture()
        bad = self.root / "bad.json"
        bad.write_text("not json", encoding="utf-8")
        bad_gzip = self.root / "bad-deflate.json.gz"
        bad_gzip.write_bytes(bytes.fromhex("1f8b0800000000000203") + b"\x07" + b"\0" * 8)
        for arguments in (("summarize", capture, "--intervals", "0:1", "--json"),):
            self.assertEqual(self.cli(*arguments).returncode, 0)
        for arguments in (("summarize", capture, "--intervals", "-1:1"),
                          ("summarize", capture, "--intervals", "0:0"),
                          ("summarize", capture, "--intervals", "0:2"),
                          ("summarize", bad), ("compare", bad), ("summarize", bad_gzip),
                          ("compare", bad_gzip), ("summarize", self.root / "missing")):
            result = self.cli(*arguments)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertNotIn("Traceback", result.stderr)

    def test_subnormal_wall_span_refuses_nonfinite_metrics_in_both_cli_modes(self):
        capture = self.write_capture(data=benchmark_capture([benchmark_interval([1], end=5e-324)]))
        for flags in ([], ["--json"]):
            with self.subTest(flags=flags):
                result = self.cli("summarize", capture, *flags)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")
                self.assertNotIn("Traceback", result.stderr)

    def test_offline_comparison_does_not_run_commands_or_open_network_connections(self):
        series = self.write_series(self.abba()[:2], {"scene": "$(touch sentinel); https://example.invalid/case"})
        with patch.object(subprocess, "Popen", side_effect=AssertionError("unexpected execution")), \
                patch.object(socket, "socket", side_effect=AssertionError("unexpected network")), \
                patch.object(urllib.request, "urlopen", side_effect=AssertionError("unexpected URL fetch")):
            result = self.cli("compare", series, "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse((self.root / "sentinel").exists())

    def test_real_entrypoint_handles_unicode_and_error_status(self):
        capture = self.write_capture("capture λ.json.gz")
        script = str(Path(benchmark_compare.__file__).resolve())
        result = subprocess.run([sys.executable, script, "summarize", str(capture), "--json"],
                                capture_output=True, text=True, encoding="utf-8", timeout=5,
                                env=dict(os.environ, PYTHONIOENCODING="ascii"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["metrics"]["frames"], 100)
        result = subprocess.run([sys.executable, script, "summarize", str(self.root / "missing")],
                                capture_output=True, text=True, encoding="utf-8", timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertNotIn("Traceback", result.stderr)


class BenchmarkHelperTests(unittest.TestCase):
    def test_benchmark_number_accepts_bounded_finite_numbers(self):
        self.assertEqual(benchmark_compare.benchmark_number(0, "n"), 0)
        self.assertEqual(benchmark_compare.benchmark_number(2.5, "n"), 2.5)
        self.assertEqual(benchmark_compare.benchmark_number(1e15, "n"), 1e15)
        self.assertEqual(benchmark_compare.benchmark_number(7, "n", 7), 7)
        for value in (True, "1", None, [1]):
            with self.assertRaisesRegex(ValueError, "n must be a finite non-negative number"):
                benchmark_compare.benchmark_number(value, "n")
        for value in (-1, -0.5, 1e15 + 1e3, math.inf, -math.inf, math.nan, 10**400):
            with self.assertRaisesRegex(ValueError, "n must be finite and between 0 and 1e\\+15"):
                benchmark_compare.benchmark_number(value, "n")
        with self.assertRaisesRegex(ValueError, "n must be finite and between 0 and 7"):
            benchmark_compare.benchmark_number(7.5, "n", 7)

    def test_benchmark_integer_accepts_unsigned_integers_up_to_the_maximum(self):
        self.assertEqual(benchmark_compare.benchmark_integer(0, "i"), 0)
        self.assertEqual(benchmark_compare.benchmark_integer(2**64 - 1, "i"), 2**64 - 1)
        self.assertEqual(benchmark_compare.benchmark_integer(3, "i", 3), 3)
        for value, maximum in ((-1, 2**64 - 1), (2**64, 2**64 - 1), (4, 3), (True, 3), (1.0, 3), ("1", 3), (None, 3)):
            with self.assertRaisesRegex(ValueError, f"i must be an integer between 0 and {maximum}"):
                benchmark_compare.benchmark_integer(value, "i", maximum)

    def test_benchmark_text_bounds_length_and_optional_nonemptiness(self):
        self.assertEqual(benchmark_compare.benchmark_text("", "t"), "")
        self.assertEqual(benchmark_compare.benchmark_text("x" * 4096, "t"), "x" * 4096)
        self.assertEqual(benchmark_compare.benchmark_text(" x ", "t", True), " x ")
        for value in ("x" * 4097, 1, None, b"x"):
            with self.assertRaisesRegex(ValueError, "t must be a string of at most 4096 characters"):
                benchmark_compare.benchmark_text(value, "t")
        for value in ("", " \t\n"):
            with self.assertRaisesRegex(ValueError, "t must be a nonempty string of at most 4096 characters"):
                benchmark_compare.benchmark_text(value, "t", True)

    def test_reject_benchmark_constant_names_the_constant(self):
        for value in ("NaN", "Infinity", "-Infinity"):
            with self.assertRaisesRegex(ValueError, f"non-finite JSON constant: {value}"):
                benchmark_compare.reject_benchmark_constant(value)

    def test_parse_benchmark_selection_accepts_retained_interval_ranges(self):
        self.assertEqual(benchmark_compare.parse_benchmark_selection("0:1"), (0, 1))
        self.assertEqual(benchmark_compare.parse_benchmark_selection("239:240"), (239, 240))
        for value in ("1", "1:", ":1", "-1:2", "1:2:3", " 1:2", "a:b"):
            with self.assertRaisesRegex(benchmark_compare.argparse.ArgumentTypeError, "intervals must be START:COUNT with a positive count"):
                benchmark_compare.parse_benchmark_selection(value)
        for value in ("240:1", "0:0", "0:241"):
            with self.assertRaisesRegex(benchmark_compare.argparse.ArgumentTypeError, "interval selection must fit 240 retained intervals"):
                benchmark_compare.parse_benchmark_selection(value)

    def test_benchmark_display_uses_six_significant_digits(self):
        self.assertEqual(benchmark_compare.benchmark_display(None), "n/a")
        self.assertEqual(benchmark_compare.benchmark_display(0), "0")
        self.assertEqual(benchmark_compare.benchmark_display(16.6666666), "16.6667")
        self.assertEqual(benchmark_compare.benchmark_display(1234567.0), "1.23457e+06")

    def test_benchmark_metric_value_walks_dotted_keys_and_raises_for_missing(self):
        summary = {"metrics": {"fps": 60.0, "frame_ms": {"p99": 17.5}}}
        self.assertEqual(benchmark_compare.benchmark_metric_value(summary, "fps"), 60.0)
        self.assertEqual(benchmark_compare.benchmark_metric_value(summary, "frame_ms.p99"), 17.5)
        self.assertEqual(benchmark_compare.benchmark_metric_value(summary, "frame_ms"), {"p99": 17.5})
        with self.assertRaises(KeyError):
            benchmark_compare.benchmark_metric_value(summary, "frame_ms.p50")

    def test_read_benchmark_json_reads_plain_bom_and_gzip_and_rejects_bad_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plain = root / "plain.json"
            plain.write_bytes(b'\xef\xbb\xbf{"a": 1}')
            value, info = benchmark_compare.read_benchmark_json(plain)
            self.assertEqual(value, {"a": 1})
            self.assertEqual(info["sha256"], info["decoded_sha256"])
            self.assertEqual(info["bytes"], 11)
            packed = root / "packed.json.gz"
            packed.write_bytes(gzip.compress(b'{"b": [1, 2]}'))
            value, info = benchmark_compare.read_benchmark_json(packed)
            self.assertEqual(value, {"b": [1, 2]})
            self.assertEqual(info["decoded_sha256"], hashlib.sha256(b'{"b": [1, 2]}').hexdigest())
            self.assertNotEqual(info["sha256"], info["decoded_sha256"])
            with self.assertRaisesRegex(ValueError, "decoded JSON exceeds 4 bytes"):
                benchmark_compare.read_benchmark_json(packed, decoded_limit=4)
            with self.assertRaisesRegex(ValueError, "input exceeds 4 bytes"):
                benchmark_compare.read_benchmark_json(plain, limit=4)
            broken = root / "broken.json.gz"
            broken.write_bytes(b"\x1f\x8b" + b"\x00" * 8)
            with self.assertRaisesRegex(ValueError, "invalid gzip data"):
                benchmark_compare.read_benchmark_json(broken)
            constant = root / "constant.json"
            constant.write_text('{"a": NaN}', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-finite JSON constant: NaN"):
                benchmark_compare.read_benchmark_json(constant)
            with self.assertRaisesRegex(ValueError, "input must be a regular file"):
                benchmark_compare.read_benchmark_json(root)
            with self.assertRaisesRegex(ValueError, "input must be a regular file"):
                benchmark_compare.read_benchmark_json(root / "missing.json")


if __name__ == "__main__":
    unittest.main()
