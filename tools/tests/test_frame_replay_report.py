import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import frame_replay_report as report
import frame_replay_sweep as sweep


COMMIT = "a" * 40
NATIVE = {
    "frames": 3, "replay_s": 0.1, "fps": 30.0,
    "frame_ms": {"p50": 20.5, "p90": 21.0, "max": 21.0, "each": [20, 21]},
    "passes": [{"replay_s": 0.1, "frames": 3, "p50": 20.5, "pace_timeouts": 0, "synthesized": 0}],
    "kept_reservations": 0, "submits": 4, "delta_mib": 1.0, "park_restores": 0,
    "pace_timeouts": 0, "pace_wait_s": 0.0, "stale_registrations": 0,
    "failed_registrations": 0, "skipped_mib": 0.0, "threads": 1,
    "released_waits": 0, "of_them_from_earlier_writes": 0, "of_them_synthesized": 0,
    "unfinished": 0, "frames_compared": 1, "pixels_identical": 1.0,
    "max_channel_diff": 0, "load_s": 0.01, "journal_s": 0.02,
}
HOST = """AnyPS5 host report (format 1)
commit: aaaaaaaaaaaa
[system]
os: Test OS
cpu: Test CPU
cpu_threads: 8
memory_mib: 16384
cpu_gate: passed
[sdl]
linked: 2.30.0
compiled: 2.30.0
[gpu]
device: Test GPU
type: discrete
vendor_id: 0x1234
device_id: 0x5678
vulkan_api: 1.4.335
driver: Test Driver
driver_info: Test 1.0
driver_version: 1.0.0
driver_version_raw: 0x12345678
subgroup_size: 32
device_local_mib: 8192
candidates: Test GPU
present_modes: unknown
present_mode_requested: default
[vulkan]
shaderInt64: feature required present
[shader_cache]
state: present
size_mib: 1
[environment]
ANYPS5_HOST_REPORT: 1
"""


class FrameReplayReportTests(unittest.TestCase):
    def test_bad_saved_metrics_do_not_break_other_observations(self):
        assessment = report.parse_replay_result(json.dumps(NATIVE), HOST)
        trial = {"phase": "trial", "iteration": 1, "returncode": 0, "timed_out": False, "log_limit": False}
        for key, value, metric in (("frame_ms", [], "p50_ms"), ("fps", 10**309, "fps"),
                                   ("replay_s", 1e308, "replay_s")):
            with self.subTest(key=key):
                candidate = copy.deepcopy(assessment)
                candidate["native"][key] = value
                payload = {"format": "aps5-frame-replay-comparison/1", "complete": True,
                           "runs": [{**trial, "arm": "baseline", "assessment": assessment},
                                    {**trial, "arm": "candidate", "assessment": candidate}]}
                result = report.replay_comparison_report(payload)
                metrics = result["observations"]["candidate"]["metrics"]
                self.assertIsNone(metrics[metric]["median"])
                self.assertEqual(metrics["replay_s" if metric == "fps" else "fps"]["n"], 1)
                self.assertIsNone(result["pairs"][0]["metrics"][metric]["candidate"])
                self.assertIsNone(result["paired_observations"][metric]["median_change_pct"])

    def test_saved_comparison_refuses_a_non_object_inputs_section(self):
        with self.assertRaisesRegex(ValueError, "comparison inputs must be an object"):
            report.replay_comparison_report({"format": "aps5-frame-replay-comparison/1", "runs": [], "inputs": []})

    def test_large_finite_percentage_changes_keep_a_finite_median(self):
        baseline = report.parse_replay_result(json.dumps(NATIVE), HOST)
        candidate = copy.deepcopy(baseline)
        baseline["native"]["fps"] = 1e-306
        candidate["native"]["fps"] = 1.0
        runs = [{"phase": "trial", "iteration": iteration, "returncode": 0, "timed_out": False,
                 "log_limit": False, "arm": arm, "assessment": assessment}
                for iteration in (1, 2)
                for arm, assessment in (("baseline", baseline), ("candidate", candidate))]
        result = report.replay_comparison_report(
            {"format": "aps5-frame-replay-comparison/1", "complete": True, "runs": runs})
        fps = result["paired_observations"]["fps"]
        self.assertEqual(fps["n"], 2)
        self.assertEqual(fps["median_change_pct"], fps["max_change_pct"])

    def test_host_report_preserves_unicode_separators_inside_values(self):
        for separator in ("\x85", "\u2028", "\u2029"):
            for newline in ("\n", "\r\n"):
                with self.subTest(separator=separator, newline=newline):
                    driver = f"Test{separator}Driver"
                    environment = f"capture{separator}folder"
                    raw = HOST.replace("Test 1.0", driver)
                    raw += f"APS5_TRACE_DIR: {environment}\nVK_LOADER_DEBUG: info\n"
                    raw = raw.replace("\n", newline)
                    parsed = report.parse_replay_host_report(raw + "warning: loading frames" + newline, COMMIT)
                    self.assertEqual(parsed["gpu"]["driver_info"], driver)
                    self.assertEqual(parsed["environment"]["APS5_TRACE_DIR"], environment)
                    self.assertEqual(parsed["environment"]["VK_LOADER_DEBUG"], "info")
                    self.assertEqual(parsed["raw"], raw)

    def test_native_unavailable_reasons_belong_to_the_host_report(self):
        reasons = ["cpu: cpuid", "memory: no source", "os: uname failed"]
        raw = HOST + "[unavailable]\n" + "\n".join(reasons) + "\n"
        parsed = report.parse_replay_host_report(raw + "warning: loading frames\n", COMMIT)
        self.assertEqual(parsed["unavailable"], reasons)
        self.assertEqual(parsed["raw"], raw)
        with self.assertRaisesRegex(ValueError, "contradictory invocation host reports"):
            report.parse_replay_host_report(raw + raw.replace("os: uname failed", "os: no source"), COMMIT)

    def test_validation_diagnostics_in_stdout_and_stderr(self):
        for stdout_bytes in (False, True):
            for stderr_bytes in (False, True):
                stdout = json.dumps(NATIVE)
                stderr = HOST
                healthy = report.parse_replay_result(stdout.encode() if stdout_bytes else stdout,
                                                     stderr.encode() if stderr_bytes else stderr, COMMIT)
                self.assertEqual(len(healthy["issues"]), 1 if stderr_bytes else 0)
                if stderr_bytes:
                    self.assertIsNone(healthy["host"])
                for stream in ("stdout", "stderr", "both"):
                    for diagnostic in ("Validation Error: shader interface exceeds the device limit",
                                       "VUID-RuntimeSpirv-Location-06272",
                                       "SYNC-HAZARD-WRITE-AFTER-WRITE(ERROR / SPEC): msgNum: 1544472022"):
                        with self.subTest(stream=stream, diagnostic=diagnostic,
                                          stdout_bytes=stdout_bytes, stderr_bytes=stderr_bytes):
                            stdout = json.dumps(NATIVE)
                            stderr = HOST
                            if stream in ("stdout", "both"):
                                stdout = diagnostic + "\n" + stdout
                            if stream in ("stderr", "both"):
                                stderr = diagnostic + "\n" + stderr
                            parsed = report.parse_replay_result(stdout.encode() if stdout_bytes else stdout,
                                                                stderr.encode() if stderr_bytes else stderr, COMMIT)
                            self.assertEqual(parsed["native"], NATIVE)
                            if stderr_bytes:
                                self.assertIsNone(parsed["host"])
                            else:
                                self.assertEqual(parsed["host"]["count"], 1)
                            self.assertEqual(len(parsed["issues"]), 2 if stderr_bytes else 1)
                            validation = [issue for issue in parsed["issues"] if "Vulkan validation diagnostics" in issue]
                            self.assertEqual(len(validation), 1)
                            self.assertIn("stdout/stderr", validation[0])

        invalid = b"\xff VUID-RuntimeSpirv-Location-06272\n"
        for stream in ("stdout", "stderr"):
            with self.subTest(invalid_utf8_stream=stream):
                stdout = invalid + json.dumps(NATIVE).encode() if stream == "stdout" else json.dumps(NATIVE)
                stderr = invalid + HOST.encode() if stream == "stderr" else HOST
                parsed = report.parse_replay_result(stdout, stderr, COMMIT)
                self.assertIsNone(parsed["native"] if stream == "stdout" else parsed["host"])
                self.assertEqual(len(parsed["issues"]), 2)
                self.assertTrue(any("Vulkan validation diagnostics" in issue for issue in parsed["issues"]))

    def test_contract(self):
        with self.subTest("bounded strict JSON"):
            self.assertEqual(report.load_replay_json(b'{"a":1}'), {"a": 1})
            for value in ('{"a":1,"a":2}', '{"x":{"a":1,"a":2}}', '{"a":NaN}', '{"a":Infinity}', '{"a":1e999}', '{}{}', b'\xff', '[' * 2000):
                with self.assertRaises(ValueError):
                    report.load_replay_json(value)
            with self.assertRaises(ValueError):
                report.load_replay_json("{}", max_bytes=1)

        with self.subTest("complete actual invocation evidence"):
            value = report.parse_replay_result(json.dumps(NATIVE), "before\n" + HOST + "[frame-replay] done\n", COMMIT)
            self.assertEqual(value["native"], NATIVE)
            self.assertEqual(value["host"]["gpu"]["driver"], "Test Driver")
            self.assertEqual(value["host"]["count"], 1)
            self.assertEqual(value["issues"], [])
            self.assertTrue(any("zero does not prove" in item for item in value["unknowns"]))
            self.assertTrue(any("UUID" in item for item in value["unknowns"]))
            self.assertEqual(value["host"]["sha256"], hashlib.sha256(value["host"]["raw"].encode()).hexdigest())
            self.assertEqual(report.parse_replay_host_report(HOST.replace("\n", "\r\n"))["raw"], HOST.replace("\n", "\r\n"))
            self.assertEqual(report.parse_replay_result("[diagnostic] before\n" + json.dumps(NATIVE) + "\n[diagnostic] after\n", HOST)["native"], NATIVE)
            self.assertIsNone(report.parse_replay_result(json.dumps(NATIVE) + "\n" + json.dumps(NATIVE), HOST)["native"])

        with self.subTest("host report duplicates and contradictory identity"):
            host = report.parse_replay_host_report(HOST + "other diagnostic\n" + HOST, COMMIT)
            self.assertEqual(host["count"], 2)
            self.assertTrue(any("more than once" in item for item in host["unknowns"]))
            for text in (HOST + HOST.replace("Test GPU", "Other GPU"), HOST.replace("cpu: Test CPU\n", ""), HOST.replace("[sdl]", "[system]"), HOST.replace("format 1", "format 2"), "", HOST.replace("[environment]\nANYPS5_HOST_REPORT: 1\n", "[environment]\n"), HOST.replace("vendor_id: 0x1234", "vendor_id: 0x1234\nvendor_id: 0x1234")):
                with self.assertRaises(ValueError):
                    report.parse_replay_host_report(text, COMMIT)
            mismatch = report.parse_replay_host_report(HOST, "b" * 40)
            self.assertTrue(any("requested arm" in issue for issue in mismatch["issues"]))

        with self.subTest("empty environment values belong to the host report"):
            raw = HOST.replace("[environment]\n", "[environment]\nANYPS5_GPU: \nAPS5_TRACE: \n")
            parsed = report.parse_replay_host_report(raw, COMMIT)
            self.assertEqual(parsed["environment"], {"ANYPS5_GPU": "", "APS5_TRACE": "", "ANYPS5_HOST_REPORT": "1"})
            self.assertEqual(parsed["raw"], raw)

        with self.subTest("a diagnostic after the environment ends the host report"):
            parsed = report.parse_replay_host_report(HOST + "agc_frame_replay: the GPU lacks shaderInt64\n", COMMIT)
            self.assertEqual(parsed["environment"], {"ANYPS5_HOST_REPORT": "1"})
            self.assertEqual(parsed["raw"], HOST)

        for key in report.INTEGER_FIELDS:
            with self.subTest("required integer", field=key):
                for replacement in (None, True, -1, 1.5, 1 << 64):
                    native = copy.deepcopy(NATIVE)
                    native[key] = replacement
                    parsed = report.parse_replay_result(json.dumps(native), HOST, COMMIT)
                    self.assertIsNone(parsed["native"])
                    self.assertTrue(parsed["issues"])
        for key in report.NUMBER_FIELDS:
            with self.subTest("required number", field=key):
                for replacement in (None, True, -0.1, "1", float("inf")):
                    native = copy.deepcopy(NATIVE)
                    native[key] = replacement
                    self.assertIsNone(report.parse_replay_result(json.dumps(native), HOST)["native"])

        with self.subTest("health diagnostics preserve observations"):
            for key in ("stale_registrations", "failed_registrations", "pace_timeouts", "of_them_synthesized", "unfinished"):
                native = copy.deepcopy(NATIVE)
                native[key] = 1
                parsed = report.parse_replay_result(json.dumps(native), HOST)
                self.assertEqual(parsed["native"][key], 1)
                self.assertTrue(any(key in issue for issue in parsed["issues"]))
            parsed = report.parse_replay_result(json.dumps(NATIVE), HOST + "[frame-replay] the capture is truncated; replaying the complete records\n")
            self.assertTrue(any("truncated" in issue for issue in parsed["issues"]))
            native = copy.deepcopy(NATIVE)
            native["frames_compared"] = 0
            parsed = report.parse_replay_result(json.dumps(native), HOST)
            self.assertTrue(any("fidelity is unavailable" in item for item in parsed["unknowns"]))
            native["frames_compared"] = 1
            native["pixels_identical"] = 0.9
            self.assertTrue(any("differ" in issue for issue in report.parse_replay_result(json.dumps(native), HOST)["issues"]))

        with self.subTest("native structure and precision"):
            variants = []
            for key in ("frame_ms", "passes", "max_channel_diff"):
                native = copy.deepcopy(NATIVE)
                native.pop(key)
                variants.append(native)
            native = copy.deepcopy(NATIVE)
            native["frame_ms"]["each"] = [20]
            variants.append(native)
            native = copy.deepcopy(NATIVE)
            native["frame_ms"]["each"] = [20.5, 21]
            variants.append(native)
            for native in variants:
                self.assertIsNone(report.parse_replay_result(json.dumps(native), HOST)["native"])
            native = copy.deepcopy(NATIVE)
            native["future_extension"] = {"x": 1}
            self.assertEqual(report.parse_replay_result(json.dumps(native), HOST)["native"], native)
            native = copy.deepcopy(NATIVE)
            native["frames"] = native["passes"][0]["frames"] = 1
            native["frame_ms"]["each"] = []
            self.assertTrue(any("fewer than two" in issue for issue in report.parse_replay_result(json.dumps(native), HOST)["issues"]))

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "tree"
            root.mkdir()
            (root / "sub").mkdir()
            target = root / "sub" / "data.bin"
            target.write_bytes(b"pinned")
            with self.subTest("streamed tree evidence and exclusions"):
                manifest = report.replay_tree_manifest(root)
                self.assertEqual(manifest, [{"path": "sub/data.bin", "bytes": 6, "sha256": hashlib.sha256(b"pinned").hexdigest()}])
                self.assertTrue(report.verify_replay_bundle(root, manifest))
                (root / "report.json").write_text("{}")
                self.assertEqual(report.replay_tree_manifest(root, exclude=("report.json",)), manifest)
                self.assertTrue(report.verify_replay_bundle(root, manifest, exclude=("report.json",)))
                with self.assertRaises(ValueError):
                    report.verify_replay_bundle(root, manifest)
                (root / "report.json").unlink()
                for kwargs in ({"max_bytes": 5}, {"max_files": 0}, {"exclude": ("../escape",)}):
                    with self.assertRaises(ValueError):
                        report.replay_tree_manifest(root, **kwargs)
            with self.subTest("content mutation and manifest path rejection"):
                target.write_bytes(b"mutate")
                with self.assertRaises(ValueError):
                    report.verify_replay_bundle(root, manifest)
                target.write_bytes(b"pinned")
                for name in ("../outside", "/absolute", "C:/absolute", "sub\\data.bin", "sub/./data.bin"):
                    with self.assertRaises(ValueError):
                        report.verify_replay_bundle(root, [{**manifest[0], "path": name}])
                with self.assertRaises(ValueError):
                    report.verify_replay_bundle(root, [manifest[0], manifest[0]])
            with self.subTest("in-read mutation is rejected"):
                original = target.stat()
                changed = Mock(spec=original)
                for key in ("st_dev", "st_ino", "st_mode", "st_size", "st_mtime_ns", "st_ctime_ns", "st_birthtime_ns"):
                    if not hasattr(original, key):
                        continue
                    setattr(changed, key, getattr(original, key))
                changed.st_size += 1
                with patch.object(report.os, "fstat", side_effect=[original, changed]):
                    with self.assertRaisesRegex(ValueError, "changed during hashing"):
                        report.replay_tree_manifest(root)
            with self.subTest("hardlinks are rejected"):
                alias = root / "alias.bin"
                os.link(target, alias)
                try:
                    with self.assertRaisesRegex(ValueError, "hardlinked"):
                        report.replay_tree_manifest(root)
                finally:
                    alias.unlink()
            with self.subTest("directory aliases are rejected"):
                alias = root / "alias"
                if os.name == "nt":
                    command = ["cmd", "/c", "mklink", "/J", str(alias), str(root / "sub")]
                    completed = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(completed.returncode, 0, completed.stderr)
                else:
                    alias.symlink_to(root / "sub", target_is_directory=True)
                try:
                    with self.assertRaises(ValueError):
                        report.replay_tree_manifest(root)
                    with self.assertRaises(ValueError):
                        report.replay_tree_manifest(alias)
                finally:
                    if os.name == "nt":
                        alias.rmdir()
                    else:
                        alias.unlink()

        with self.subTest("descriptive report retains failures and self-baseline"):
            assessment = report.parse_replay_result(json.dumps(NATIVE), HOST)
            run = {"arm": "baseline", "phase": "trial", "iteration": 1, "returncode": 0,
                   "timed_out": False, "log_limit": False, "assessment": assessment, "path": "runs/1"}
            payload = {"format": "aps5-frame-replay-comparison/1", "runs": [run, {**run, "arm": "candidate"}, {**run, "returncode": 3}],
                       "inputs": {"baseline": [{"path": "x", "sha256": "a"}], "candidate": [{"path": "x", "sha256": "a"}]},
                       "issues": ["unsafe <script>|value"], "unknowns": [], "recipe": {"arms": {"baseline": {"environment": {}}, "candidate": {"environment": {}}}}, "complete": True}
            rendered = report.replay_comparison_report(payload)
            self.assertEqual(len(rendered["runs"]), 3)
            self.assertEqual(rendered["observations"]["baseline"]["attempted_trials"], 2)
            self.assertEqual(rendered["observations"]["baseline"]["healthy_trials"], 1)
            self.assertEqual(rendered["observations"]["candidate"]["metrics"]["replay_s"]["median"], 0.1)
            self.assertTrue(rendered["self_baseline"])
            self.assertNotIn("observations", payload)
            markdown = report.replay_comparison_report(payload, markdown=True)
            self.assertIn("&lt;script&gt;\\|value", markdown)
            self.assertIn("not title FPS", markdown)
            paired = {**payload, "runs": payload["runs"][:2], "issues": []}
            self.assertEqual(report.replay_comparison_report(paired)["paired_observations"]["fps"]["median_change_pct"], 0)
            blocked = {**paired, "issues": ["different GPU"]}
            self.assertIsNone(report.replay_comparison_report(blocked)["paired_observations"]["fps"]["median_change_pct"])
            changed_settings = copy.deepcopy(paired)
            changed_settings["recipe"]["arms"]["candidate"]["environment"] = {"APS5_DRAW_PIPELINE": "1"}
            self.assertFalse(report.replay_comparison_report(changed_settings)["self_baseline"])
            unreported = copy.deepcopy(paired)
            unreported["runs"][0]["assessment"]["unknowns"].append("CPU identity unavailable")
            self.assertIn("CPU identity unavailable", report.replay_comparison_report(unreported, markdown=True))
            extreme = copy.deepcopy(paired)
            extreme["runs"][0]["assessment"] = copy.deepcopy(extreme["runs"][0]["assessment"])
            extreme["runs"][0]["assessment"]["native"]["fps"] = 1e-320
            self.assertIsNone(report.replay_comparison_report(extreme)["paired_observations"]["fps"]["median_change_pct"])
            payload["runs"] = [{**run, "returncode": 3}]
            empty = report.replay_comparison_report(payload)
            self.assertIsNone(empty["observations"]["baseline"]["metrics"]["fps"]["median"])



class FrameReplaySweepTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()

    def bundle(self, name, *, capture=b"same capture", baseline_environment=None,
               baseline_executable=b"replay executable", candidate_exit=0, missing_candidate=False):
        root = self.root / name
        metadata = {"magic": "aps5-frame-capture 1", "frames": 3, "unreadable_bytes": 0,
                    "host_only_sample_bytes": 0, "warnings": []}
        capture_dir = root / "capture"
        capture_dir.mkdir(parents=True)
        (capture_dir / "capture.json").write_text(json.dumps(metadata), encoding="utf-8")
        (capture_dir / "calls.bin").write_bytes(capture)
        (capture_dir / "memory.bin").write_bytes(b"memory capture")
        for arm_name in ("baseline", "candidate"):
            runtime = root / "runtime" / arm_name
            runtime.mkdir(parents=True)
            (runtime / "replay").write_bytes(baseline_executable if arm_name == "baseline" else b"replay executable")
            (runtime / "inspect").write_bytes(b"capture inspector")
            (runtime / "library.prx").write_bytes(b"project library")
        arm = {"commit": COMMIT, "replay": "replay", "inspect": "inspect", "library_dirs": ["."]}
        recipe = {"format": "aps5-frame-replay-recipe/1", "trials": 1, "warmups": 0,
                  "timeout": 120, "log_mib": 16, "environment": {},
                  "arms": {"baseline": {**arm, "environment": baseline_environment or {}},
                           "candidate": {**arm, "environment": {"APS5_TEST_SETTING": "private setting value"}}}}
        runs = []
        for name in ("baseline", "candidate"):
            if name == "candidate" and missing_candidate:
                continue
            relative = f"runs/trial-01-{name}"
            runs.append({"arm": name, "phase": "trial", "iteration": 1, "path": relative})
            run = root / relative
            run.mkdir(parents=True)
            (run / "stdout.txt").write_text(json.dumps(NATIVE), encoding="utf-8")
            (run / "stderr.txt").write_text(HOST, encoding="utf-8")
            receipt = {"returncode": candidate_exit if name == "candidate" else 0,
                       "timed_out": False, "log_limit": False, "io_errors": []}
            (run / "process.json").write_text(json.dumps(receipt), encoding="utf-8")
        staged = {"capture": capture_dir, **{arm_name: root / "runtime" / arm_name
                                            for arm_name in ("baseline", "candidate")}}
        original = {"format": "aps5-frame-replay-comparison/1", "complete": not missing_candidate,
                    "runs": runs, "recipe": recipe, "issues": [],
                    "inputs": {name: report.replay_tree_manifest(directory) for name, directory in staged.items()}}
        (root / "comparison.json").write_text(json.dumps(original), encoding="utf-8")
        metadata = {"format": original["format"], "artifacts": report.replay_tree_manifest(root)}
        (root / "report.json").write_text(json.dumps(metadata), encoding="utf-8")
        return root

    def sweep(self, *cases):
        before = report.replay_tree_manifest(self.root)
        command = [sys.executable, str(Path(report.__file__).with_name("frame_replay_sweep.py"))]
        for label, bundle in cases:
            command.extend(("--case", label, str(bundle)))
        result = subprocess.run(command, cwd=self.root, capture_output=True, encoding="utf-8", timeout=20)
        self.assertEqual(report.replay_tree_manifest(self.root), before)
        return result

    def test_renders_success_failure_and_unattempted_cases_from_sealed_evidence(self):
        passed = self.bundle("passed")
        failed = self.bundle("failed", candidate_exit=3)
        incomplete = self.bundle("incomplete", missing_candidate=True)
        result = self.sweep(("Pass | and <tag>", passed), ("Stops", failed), ("Partial", incomplete))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(r"Pass \| and &lt;tag&gt;", result.stdout)
        self.assertIn("APS5_TEST_SETTING", result.stdout)
        self.assertNotIn("private setting value", result.stdout)
        self.assertNotIn(str(self.root), result.stdout)
        self.assertIn("1 / 1 | 0 / 1 | exit 3 (1) | complete", result.stdout)
        self.assertIn("1 / 1 | 0 / 0 | none recorded | incomplete", result.stdout)

    def test_refuses_different_capture_or_baseline_settings(self):
        matching = self.bundle("matching")
        other_capture = self.bundle("other-capture", capture=b"different")
        other_flag = self.bundle("other-baseline", baseline_environment={"APS5_OTHER_SETTING": "1"})
        other_binary = self.bundle("other-binary", baseline_executable=b"other baseline build")
        for other, message in ((other_capture, "capture differs"), (other_flag, "baseline settings differ"),
                               (other_binary, "staged baseline")):
            with self.subTest(bundle=other.name):
                result = self.sweep(("First", matching), ("Second", other))
                self.assertEqual(result.returncode, 2)
                self.assertIn(message, result.stderr)
                self.assertEqual(result.stdout, "")

    def test_refuses_tampered_evidence(self):
        first = self.bundle("first")
        second = self.bundle("second")
        (second / "runs" / "trial-01-candidate" / "stdout.txt").write_text("changed", encoding="utf-8")
        result = self.sweep(("First", first), ("Bad", second))
        self.assertEqual(result.returncode, 2)
        self.assertIn("Bad: bundle files differ from their pinned manifest", result.stderr)
        self.assertEqual(result.stdout, "")

    def test_escapes_multiline_and_carriage_return_values_in_table_cells(self):
        self.assertEqual(
            sweep._markdown_cell("a\r\nb\rc\nd|<tag>"),
            r"a<br>b<br>c<br>d\|&lt;tag&gt;",
        )


if __name__ == "__main__":
    unittest.main()
