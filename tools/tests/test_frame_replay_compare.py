import contextlib
import copy
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import frame_replay_compare as runner


def native_result():
    return {
        "frames": 3, "replay_s": 0.03, "fps": 100.0,
        "frame_ms": {"p50": 10.0, "p90": 10.0, "max": 10.0, "each": [10, 10]},
        "passes": [{"replay_s": 0.03, "frames": 3, "p50": 10.0,
                    "pace_timeouts": 0, "synthesized": 0}],
        "kept_reservations": 0, "submits": 1, "delta_mib": 0.1,
        "park_restores": 0, "pace_timeouts": 0, "pace_wait_s": 0.0,
        "stale_registrations": 0, "failed_registrations": 0, "skipped_mib": 0.0,
        "threads": 1, "released_waits": 0, "of_them_from_earlier_writes": 0,
        "of_them_synthesized": 0, "unfinished": 0, "frames_compared": 3,
        "pixels_identical": 1.0, "max_channel_diff": 0, "load_s": 0.001,
        "journal_s": 0.0,
    }


def host_report(commit):
    return f"""AnyPS5 host report (format 1)
commit: {commit[:12]}
[system]
os: Test OS
cpu: Test CPU
cpu_threads: 4
memory_mib: 8192
cpu_gate: passed
[sdl]
linked: 2.32.0
compiled: 2.32.0
[gpu]
device: Test GPU
type: discrete
vendor_id: 0x1234
device_id: 0x5678
vulkan_api: 1.4.335
driver: Test driver
driver_info: Test driver 1
driver_version: 1.0.0
driver_version_raw: 1
subgroup_size: 32
device_local_mib: 4096
candidates: Test GPU
present_modes: none
present_mode_requested: default
[vulkan]
VK_KHR_buffer_device_address: present
[shader_cache]
state: present
size_mib: 0
[environment]
ANYPS5_REQUIRE_VULKAN: 1
ANYPS5_HOST_REPORT: 1
"""


class ReplayFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        (self.root / "capture").mkdir()
        (self.root / "capture" / "capture.bin").write_bytes(b"captured bytes")
        (self.root / "capture" / "capture.json").write_text(json.dumps({
            "magic": "aps5-frame-capture 1", "frames": 3, "unreadable_bytes": 0,
            "host_only_sample_bytes": 0, "warnings": [],
        }), encoding="utf-8")
        for arm in runner.ARMS:
            runtime = self.root / arm
            runtime.mkdir()
            (runtime / "replay.exe").write_bytes(b"replay " + arm.encode())
            (runtime / "inspect.exe").write_bytes(b"inspect " + arm.encode())
            (runtime / "library.prx").write_bytes(b"library " + arm.encode())
        self.recipe = {
            "format": "aps5-frame-replay-recipe/1", "capture": "capture",
            "arms": {name: {"runtime": name, "replay": "replay.exe", "inspect": "inspect.exe",
                            "commit": ("a" if name == "baseline" else "b") * 40}
                     for name in runner.ARMS},
            "trials": 2, "warmups": 1,
        }

    def validate(self, recipe=None):
        return runner.validate_replay_recipe(self.recipe if recipe is None else recipe, self.root)


class RecipeTests(ReplayFixture):
    def test_contract_normalizes_defaults_and_requires_both_arms(self):
        recipe = self.validate()
        self.assertEqual(recipe["timeout"], 120)
        self.assertEqual(recipe["log_mib"], 16)
        self.assertEqual(recipe["arms"]["baseline"]["library_dirs"], ["."])
        self.assertEqual(Path(recipe["capture"]), self.root / "capture")
        for invalid in ([], {}, {"format": "different"}):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                self.validate(invalid)
        recipe = copy.deepcopy(self.recipe)
        del recipe["arms"]["candidate"]
        with self.assertRaisesRegex(ValueError, "exactly"):
            self.validate(recipe)

    def test_unknown_fields_forced_environment_and_limits_are_rejected(self):
        invalids = [("trials", value) for value in (True, 0, 21, 1.5, "2", 10 ** 400)]
        invalids += [("warmups", value) for value in (-1, 6, False)]
        invalids += [("timeout", value) for value in (0, 3601, float("nan"), float("inf"))]
        invalids += [("log_mib", value) for value in (0, 129, 1.5)]
        invalids.append(("unrecognized", 1))
        for key, value in invalids:
            recipe = copy.deepcopy(self.recipe)
            recipe[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                self.validate(recipe)
        for key in (*runner.FORCED, "ANYPS5_SHADER_CACHE_DIR", "APS5_FRAME_CAPTURE", "PATH"):
            recipe = copy.deepcopy(self.recipe)
            recipe["environment"] = {key: "1"}
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(recipe)
        recipe = copy.deepcopy(self.recipe)
        recipe["arms"]["candidate"]["surprise"] = True
        with self.assertRaises(ValueError):
            self.validate(recipe)

    def test_runtime_tools_and_library_paths_cannot_escape(self):
        for key, values in (("replay", ("../baseline/replay.exe", "/replay.exe", "a\\b", "C:/x", "missing")),
                            ("inspect", ("..", ".", "", None)),
                            ("library_dirs", (["../baseline"], ["missing"], [], ["a\\b"], ["C:/x"]))):
            for value in values:
                recipe = copy.deepcopy(self.recipe)
                recipe["arms"]["baseline"][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    self.validate(recipe)
        for commit in ("a" * 12, "A" * 40, "z" * 40, 1):
            recipe = copy.deepcopy(self.recipe)
            recipe["arms"]["candidate"]["commit"] = commit
            with self.subTest(commit=commit), self.assertRaises(ValueError):
                self.validate(recipe)

    def test_device_selection_is_shared_and_inherited_instrumentation_is_removed(self):
        for key in ("ANYPS5_GPU", "VK_ICD_FILENAMES"):
            recipe = copy.deepcopy(self.recipe)
            recipe["arms"]["baseline"]["environment"] = {key: "selection"}
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.validate(recipe)
        self.recipe["environment"] = {"ANYPS5_GPU": "shared", "VK_ICD_FILENAMES": "driver.json"}
        self.recipe["arms"]["candidate"]["environment"] = {"APS5_NO_INPUT_TWINS": "1"}
        recipe = self.validate()
        inherited = {"APS5_PROFILE_DRAW": "1", "ANYPS5_GPU": "wrong", "VK_INSTANCE_LAYERS": "wrong",
                     "LD_PRELOAD": "wrong", "DYLD_INSERT_LIBRARIES": "wrong"}
        with mock.patch.dict(os.environ, inherited):
            env = runner._trial_environment(recipe, recipe["arms"]["candidate"], self.root / "candidate", self.root / "cache")
        for key in ("APS5_PROFILE_DRAW", "VK_INSTANCE_LAYERS", "LD_PRELOAD", "DYLD_INSERT_LIBRARIES"):
            self.assertNotIn(key, env)
        self.assertEqual(env["ANYPS5_GPU"], "shared")
        self.assertEqual(env["VK_ICD_FILENAMES"], "driver.json")
        self.assertEqual(env["APS5_NO_INPUT_TWINS"], "1")
        self.assertEqual(env["ANYPS5_REQUIRE_VULKAN"], "1")
        self.assertEqual(env["ANYPS5_HOST_REPORT"], "1")
        self.assertEqual(env["ANYPS5_SHADER_CACHE_DIR"], str(self.root / "cache"))

    def test_stage_replay_runtime_copies_files_and_rejects_changed_bytes(self):
        source = self.root / "baseline"
        manifest = runner.evidence.replay_tree_manifest(source)
        staged = runner.stage_replay_runtime(source, self.root / "staged", manifest)
        self.assertEqual(staged, self.root / "staged")
        self.assertEqual((staged / "replay.exe").read_bytes(), b"replay baseline")
        self.assertEqual(runner.evidence.replay_tree_manifest(staged), manifest)
        (source / "replay.exe").write_bytes(b"replay changed")
        with self.assertRaises(ValueError):
            runner.stage_replay_runtime(source, self.root / "second", manifest)

    def test_requested_validation_adds_layer_logging_without_replacing_flags(self):
        layer = runner.processes.FORCED["VK_INSTANCE_LAYERS"]
        max_length_flags = "error," * 682 + "warn"
        cases = ((layer, None, "layer"),
                 (f"VK_LAYER_example:{layer}", "warn", "warn,layer"),
                 (f"{layer};VK_LAYER_example", "error,layer", "error,layer"),
                 (layer, "all", "all"),
                 (layer, max_length_flags, max_length_flags + ",layer"),
                 (f"{layer}_other", None, None))
        for layers, flags, expected in cases:
            with self.subTest(layers=layers, flags=flags):
                shared = {"VK_INSTANCE_LAYERS": layers}
                if flags is not None:
                    shared["VK_LOADER_DEBUG"] = flags
                self.recipe["environment"] = shared
                recipe = self.validate()
                env = runner._trial_environment(recipe, recipe["arms"]["baseline"], self.root / "baseline", self.root / "cache")
                self.assertEqual(env["VK_INSTANCE_LAYERS"], layers)
                self.assertEqual(env.get("VK_LOADER_DEBUG"), expected)
                self.assertEqual(recipe["environment"].get("VK_LOADER_DEBUG"), flags)



class PreflightTests(ReplayFixture):
    def setUp(self):
        super().setUp()
        capture = self.root / "capture"
        (capture / "capture.bin").rename(capture / "calls.bin")
        (capture / "memory.bin").write_bytes(b"memory stream")
        self.recipe_path = self.root / "recipe.json"
        self.recipe_path.write_text(json.dumps(self.recipe), encoding="utf-8")

    def preflight(self):
        before = {path.relative_to(self.root): path.read_bytes() if path.is_file() else None
                  for path in self.root.rglob("*")}
        result = subprocess.run([sys.executable, str(Path(runner.__file__).resolve()), "validate", "recipe.json"],
                                cwd=self.root, capture_output=True, encoding="utf-8", timeout=30)
        after = {path.relative_to(self.root): path.read_bytes() if path.is_file() else None
                 for path in self.root.rglob("*")}
        self.assertEqual(after, before)
        return result

    def test_reports_both_runtimes_and_capture_without_launching(self):
        result = self.preflight()
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ("capture", *runner.ARMS):
            with self.subTest(input=name):
                self.assertIn(f"{name}: 3 files, ", result.stdout)
        self.assertIn("Native inspection", result.stdout)

    def test_rejects_missing_capture_stream_or_candidate_tool(self):
        for relative, message in (("capture/memory.bin", "capture: missing or empty memory.bin"),
                                  ("candidate/inspect.exe", "tool is not a regular file")):
            path = self.root / relative
            contents = path.read_bytes()
            path.unlink()
            with self.subTest(input=relative):
                result = self.preflight()
                self.assertEqual(result.returncode, 2)
                self.assertIn(message, result.stderr)
                self.assertEqual(result.stdout, "")
            path.write_bytes(contents)

    def test_rejects_incomplete_capture_metadata(self):
        metadata = self.root / "capture" / "capture.json"
        value = json.loads(metadata.read_text(encoding="utf-8"))
        value["warnings"] = ["unverified host memory"]
        metadata.write_text(json.dumps(value), encoding="utf-8")
        result = self.preflight()
        self.assertEqual(result.returncode, 2)
        self.assertIn("capture warning: unverified host memory", result.stderr)
        self.assertEqual(result.stdout, "")

    @unittest.skipUnless(os.name == "nt", "Windows case-insensitive path matching")
    def test_accepts_recipe_tool_paths_with_different_file_case(self):
        if not (self.root / "candidate" / "REPLAY.EXE").is_file():
            self.skipTest("temporary runtime directory is case-sensitive")
        self.recipe["arms"]["candidate"]["replay"] = "REPLAY.EXE"
        self.recipe_path.write_text(json.dumps(self.recipe), encoding="utf-8")
        result = self.preflight()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_a_hardlinked_runtime_entry(self):
        runtime = self.root / "candidate"
        os.link(runtime / "library.prx", runtime / "copied-library.prx")
        result = self.preflight()
        self.assertEqual(result.returncode, 2)
        self.assertIn("candidate:", result.stderr)
        self.assertIn("hardlinked file", result.stderr)
        self.assertEqual(result.stdout, "")


class RunnerTests(ReplayFixture):
    def setUp(self):
        super().setUp()
        self.calls = []
        self.commands = []
        self.failure = None
        self.mismatch = False
        self.mutate_input = False
        self.device_mismatch = False
        self.workload_mismatch = False
        self.bad_inspection = False
        self.interrupt_trial = False
        self.loader_logs = {}

    def execute(self, command, directory, environment, timeout, log_bytes):
        self.commands.append(command)
        directory.mkdir(parents=True)
        command = [str(value) for value in command]
        stdout = json.dumps({"calls": [{"index": 0, "kind": "frame-start", "memory_group": None}],
                             "chunks": [{"index": 0, "kind": "reference", "group": 0}]})
        if self.bad_inspection:
            stdout = json.dumps({"calls": [{"index": 7, "kind": "frame-start", "memory_group": None}],
                                 "chunks": [{"index": 0, "kind": "reference", "group": 0}]})
        stderr = ""
        code = 0
        if len(command) > 1 and command[1] != "inspect":
            arm = Path(command[0]).parent.name
            cache = Path(environment["ANYPS5_SHADER_CACHE_DIR"])
            memo = cache / "memo.bin"
            before = memo.read_bytes() if memo.exists() else None
            self.calls.append({"arm": arm, "path": directory, "command": command,
                               "cache": cache, "before": before, "environment": dict(environment)})
            memo.write_bytes(directory.name.encode())
            native = native_result()
            if self.workload_mismatch and arm == "candidate":
                native["submits"] = 2
            if self.failure == (arm, directory.name.split("-", 1)[0]):
                native["unfinished"] = 1
                code = 3
            commit = ("c" * 40 if self.mismatch and arm == "candidate"
                      else self.recipe["arms"][arm]["commit"])
            loader_stdout, loader_stderr = self.loader_logs.get(arm, ("", ""))
            stdout = loader_stdout + json.dumps(native)
            stderr = loader_stderr + host_report(commit)
            if self.device_mismatch and arm == "candidate":
                stderr = stderr.replace("Test GPU", "Other GPU")
            if self.mutate_input and directory.name.startswith("trial"):
                (self.root / "capture" / "capture.bin").write_bytes(b"changed")
        (directory / "stdout.txt").write_text(stdout, encoding="utf-8")
        (directory / "stderr.txt").write_text(stderr, encoding="utf-8")
        result = {"command": command, "returncode": code, "timed_out": False,
                  "log_limit": False, "io_errors": [], "wall_s": 0.01}
        if self.interrupt_trial and directory.name.startswith("trial"):
            result["interrupted"] = True
            result["io_errors"].append("interrupted")
        (directory / "process.json").write_text(json.dumps(result), encoding="utf-8")
        return result

    def run_comparison(self, output=None):
        with mock.patch.object(runner, "execute_replay_trial", side_effect=self.execute):
            return runner.run_replay_comparison(self.validate(), output or self.root / "evidence")

    def test_staged_inputs_alternating_trials_and_independent_cache_clones(self):
        self.recipe["warmups"] = 2
        originals = {name: runner.evidence.replay_tree_manifest(self.root / name)
                     for name in ("capture", *runner.ARMS)}
        report = self.run_comparison()
        self.assertTrue(report["complete"], report["issues"])
        self.assertEqual(report["issues"], [])
        self.assertEqual(len(report["runs"]), 8)
        self.assertEqual([run["assessment"]["issues"] for run in report["runs"]], [[]] * 8)
        trials = [call for call in self.calls if call["path"].name.startswith("trial")]
        self.assertEqual([call["arm"] for call in trials], ["baseline", "candidate", "candidate", "baseline"])
        for call in trials:
            arm = call["arm"]
            self.assertEqual(call["before"], f"warmup-02-{arm}".encode())
            self.assertEqual(call["command"][-4:], ["--strict", "--no-synthesize", "--repeat", "1"])
            self.assertEqual(Path(call["command"][0]).parent, self.root / "evidence" / "runtime" / arm)
            self.assertEqual(Path(call["command"][1]), self.root / "evidence" / "capture")
        self.assertEqual(len({call["cache"] for call in self.calls}), len(self.calls))
        for name, manifest in originals.items():
            self.assertEqual(runner.evidence.replay_tree_manifest(self.root / name), manifest)
        offline = runner._offline_report(self.root / "evidence")
        self.assertEqual(offline["observations"]["baseline"]["healthy_trials"], 2)

    def test_failed_and_build_mismatched_trials_remain_in_report(self):
        self.recipe["warmups"] = 0
        self.failure = ("baseline", "trial")
        self.mismatch = True
        report = self.run_comparison()
        self.assertTrue(report["complete"])
        self.assertEqual(len(report["runs"]), 4)
        self.assertTrue(all(run["assessment"]["issues"] for run in report["runs"]))
        self.assertEqual(report["observations"]["baseline"]["healthy_trials"], 0)
        self.assertEqual(report["observations"]["candidate"]["healthy_trials"], 0)
        for run in report["runs"]:
            self.assertTrue((self.root / "evidence" / run["path"] / "stdout.txt").is_file())
        self.assertEqual(len(runner._offline_report(self.root / "evidence")["runs"]), 4)

    def test_bad_warmup_and_input_mutation_abort_with_evidence(self):
        self.failure = ("candidate", "warmup")
        report = self.run_comparison()
        self.assertFalse(report["complete"])
        self.assertEqual(len(report["runs"]), 2)
        self.assertTrue(any("warmup failed" in issue for issue in report["issues"]))
        self.failure = None
        self.mutate_input = True
        self.recipe["warmups"] = 0
        report = self.run_comparison(self.root / "mutation")
        self.assertFalse(report["complete"])
        self.assertEqual(len(report["runs"]), 1)
        self.assertTrue(any("pinned manifest" in issue for issue in report["issues"]))
        self.assertTrue((self.root / "mutation" / "report.md").exists())

    def test_inspection_failure_and_context_workload_mismatches_stay_visible(self):
        self.recipe["warmups"] = 0
        self.bad_inspection = True
        report = self.run_comparison(self.root / "inspection")
        self.assertFalse(report["complete"])
        self.assertEqual(report["runs"], [])
        self.assertTrue(any("index" in issue for issue in report["issues"]))
        self.assertTrue((self.root / "inspection" / "inspect-baseline" / "stdout.txt").is_file())
        self.bad_inspection = False
        self.device_mismatch = True
        report = self.run_comparison(self.root / "devices")
        self.assertEqual(len(report["runs"]), 4)
        self.assertTrue(any("context differs" in issue for issue in report["issues"]))
        self.device_mismatch = False
        self.workload_mismatch = True
        report = self.run_comparison(self.root / "workloads")
        self.assertEqual(len(report["runs"]), 4)
        self.assertTrue(any("counts differ" in issue for issue in report["issues"]))

    def test_incomplete_capture_metadata_and_low_disk_stop_before_native_launch(self):
        path = self.root / "capture" / "capture.json"
        original = json.loads(path.read_text(encoding="utf-8"))
        cases = (("warnings", ["unverified host memory"]), ("unreadable_bytes", 8),
                 ("host_only_sample_bytes", 4), ("frames", 0), ("frames", True),
                 ("magic", "unknown capture format"), ("warnings", "unknown"))
        for index, (key, value) in enumerate(cases):
            metadata = {**original, key: value}
            path.write_text(json.dumps(metadata), encoding="utf-8")
            output = self.root / f"metadata-{index}"
            with self.subTest(key=key, value=value):
                report = self.run_comparison(output)
                self.assertFalse(report["complete"])
                self.assertTrue(report["issues"])
                self.assertEqual(report["runs"], [])
                self.assertEqual(self.commands, [])
                self.assertEqual(json.loads((output / "capture" / "capture.json").read_text()), metadata)
                self.assertTrue((output / "report.json").is_file())
        path.write_text(json.dumps(original), encoding="utf-8")
        with mock.patch.object(runner.shutil, "disk_usage", return_value=mock.Mock(free=1)):
            report = self.run_comparison(self.root / "disk")
        self.assertFalse(report["complete"])
        self.assertTrue(any("disk space" in issue for issue in report["issues"]))
        self.assertEqual(self.commands, [])

    def test_offline_log_tampering_and_output_input_aliases_are_refused(self):
        self.recipe["warmups"] = 0
        self.run_comparison()
        log = self.root / "evidence" / "runs" / "trial-01-baseline" / "stdout.txt"
        log.write_text("tampered", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "pinned manifest"):
            runner._offline_report(self.root / "evidence")
        for output in (self.root, self.root / "capture", self.root / "baseline" / "nested"):
            with self.subTest(output=output), self.assertRaisesRegex(ValueError, "overlap"):
                self.run_comparison(output)

    def test_cli_retains_failure_status_and_can_read_bundle_offline(self):
        self.recipe["warmups"] = 0
        self.failure = ("candidate", "trial")
        path = self.root / "recipe.json"
        path.write_text(json.dumps(self.recipe), encoding="utf-8")
        with mock.patch.object(runner, "execute_replay_trial", side_effect=self.execute), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(runner.main(["run", str(path), "--output", str(self.root / "evidence")]), 2)
        output = io.StringIO()
        with mock.patch.object(runner, "execute_replay_trial", side_effect=AssertionError("offline launched a process")), contextlib.redirect_stdout(output):
            self.assertEqual(runner.main(["report", str(self.root / "evidence"), "--json"]), 2)
        self.assertEqual(len(json.loads(output.getvalue())["runs"]), 4)

    def test_report_prints_a_path_outside_the_ansi_code_page_as_utf8(self):
        self.recipe["warmups"] = 0
        self.recipe["capture"] = "测-capture"
        (self.root / "capture").rename(self.root / self.recipe["capture"])
        self.run_comparison()
        stdout = io.TextIOWrapper(io.BytesIO(), encoding="cp1252")
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(runner.main(["report", str(self.root / "evidence"), "--json"]), 0)
            stdout.flush()
        self.assertIn("测-capture", stdout.buffer.getvalue().decode("utf-8"))

    def test_offline_recomputes_assessments_from_original_logs(self):
        self.recipe["warmups"] = 0
        self.run_comparison()
        path = self.root / "evidence" / "report.json"
        altered = json.loads(path.read_text(encoding="utf-8"))
        altered["runs"][0]["assessment"]["issues"] = ["fabricated summary"]
        altered["observations"]["baseline"]["healthy_trials"] = 9000
        path.write_text(json.dumps(altered), encoding="utf-8")
        report = runner._offline_report(path.parent)
        self.assertEqual(report["runs"][0]["assessment"]["issues"], [])
        self.assertEqual(report["observations"]["baseline"]["healthy_trials"], 2)

    def test_requested_validation_requires_loader_evidence_live_and_offline(self):
        self.recipe["warmups"] = 0
        self.recipe["trials"] = 1
        layer = runner.processes.FORCED["VK_INSTANCE_LAYERS"]
        self.recipe["environment"] = {**{f"APS5_TEST_OPTION_{index}": "1" for index in range(127)}, "VK_INSTANCE_LAYERS": layer}
        inserted = runner.processes.INSERTED_LAYER + "\n"
        missing = f"WARNING: {layer} {runner.processes.MISSING_LAYER}\n"
        cases = (
            ("inserted", inserted, [], 0),
            ("absent", "", ["Khronos validation layer insertion was not recorded"], 2),
            ("missing", inserted + missing, ["Vulkan loader reported a requested layer missing"], 2),
        )
        path = self.root / "recipe.json"
        path.write_text(json.dumps(self.recipe), encoding="utf-8")
        for name, messages, issues, status in cases:
            with self.subTest(case=name):
                self.loader_logs = {"baseline": (inserted, ""), "candidate": ("", messages)}
                bundle = self.root / name
                with mock.patch.object(runner, "execute_replay_trial", side_effect=self.execute), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(runner.main(["run", str(path), "--output", str(bundle)]), status)
                live = json.loads((bundle / "report.json").read_text(encoding="utf-8"))
                offline = io.StringIO()
                with mock.patch.object(runner, "execute_replay_trial", side_effect=AssertionError("offline launched a process")), contextlib.redirect_stdout(offline):
                    self.assertEqual(runner.main(["report", str(bundle), "--json"]), status)
                for report in (live, json.loads(offline.getvalue())):
                    self.assertEqual([run["assessment"]["issues"] for run in report["runs"]], [[], issues])
                    self.assertEqual(report["recipe"]["environment"], self.recipe["environment"])
                self.assertTrue(all(call["environment"]["VK_LOADER_DEBUG"] == "layer" for call in self.calls[-2:]))

    def test_interrupted_trial_is_retained_and_cli_returns_130(self):
        self.recipe["warmups"] = 0
        self.interrupt_trial = True
        path = self.root / "recipe.json"
        path.write_text(json.dumps(self.recipe), encoding="utf-8")
        with mock.patch.object(runner, "execute_replay_trial", side_effect=self.execute), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(runner.main(["run", str(path), "--output", str(self.root / "evidence")]), 130)
        report = runner._offline_report(self.root / "evidence")
        self.assertFalse(report["complete"])
        self.assertIn("interrupted", report["issues"])
        self.assertEqual(len(report["runs"]), 1)
        self.assertTrue(report["runs"][0]["interrupted"])
        self.assertIn("interrupted", report["runs"][0]["assessment"]["issues"])


class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()

    def test_real_child_exit_and_timeout_preserve_bounded_logs(self):
        result = runner.execute_replay_trial([sys.executable, "-c", "import sys;print('kept');sys.exit(7)"],
                                            self.root / "exit", dict(os.environ), 5, 1024)
        self.assertEqual(result["returncode"], 7)
        self.assertFalse(result["timed_out"])
        self.assertEqual((self.root / "exit" / "stdout.txt").read_text().strip(), "kept")
        result = runner.execute_replay_trial([sys.executable, "-c", "import time;time.sleep(10)"],
                                            self.root / "timeout", dict(os.environ), 0.15, 1024)
        self.assertTrue(result["timed_out"])
        self.assertLess(result["wall_s"], 5)
        missing = runner.execute_replay_trial([str(self.root / "missing-program.exe")],
                                             self.root / "missing", dict(os.environ), 1, 1024)
        self.assertIsNone(missing["returncode"])
        self.assertTrue(missing["io_errors"])
        self.assertTrue((self.root / "missing" / "process.json").is_file())

    def test_log_limit_stops_running_child_after_partial_block(self):
        limit = 1024 * 1024
        for fd, name in ((1, "stdout.txt"), (2, "stderr.txt")):
            with self.subTest(stream=name):
                directory = self.root / name
                child = f"import os,time;os.write({fd},b'x'*{limit + 1});time.sleep(20)"
                result = runner.execute_replay_trial([sys.executable, "-c", child], directory,
                                                     dict(os.environ), 5, limit)
                self.assertTrue(result["log_limit"])
                self.assertFalse(result["timed_out"])
                self.assertFalse(result["io_errors"])
                self.assertFalse(result["cleanup_failed"])
                self.assertEqual((directory / name).read_bytes(), b"x" * limit)

    def test_real_log_quota_and_owned_descendant_cleanup(self):
        result = runner.execute_replay_trial([sys.executable, "-c", "import os;os.write(1,b'x'*262144);os.write(2,b'y'*262144)"],
                                            self.root / "quota", dict(os.environ), 5, 1024)
        self.assertTrue(result["log_limit"])
        for name in ("stdout.txt", "stderr.txt"):
            self.assertLessEqual((self.root / "quota" / name).stat().st_size, 1024)
        ready = self.root / "descendant-spawned"
        release = self.root / "descendant-release"
        marker = self.root / "descendant-survived"
        child = ("import pathlib,time\nend=time.monotonic()+30\n"
                 "while time.monotonic()<end and not pathlib.Path(" + repr(str(release)) + ").exists():\n    time.sleep(0.01)\n"
                 "pathlib.Path(" + repr(str(marker)) + ").write_text('survived')")
        parent = ("import pathlib,subprocess,sys,time;subprocess.Popen([sys.executable,'-c'," + repr(child) + "]);"
                  "print('spawned',flush=True);pathlib.Path(" + repr(str(ready)) + ").touch();time.sleep(30)")
        monotonic = time.monotonic
        with mock.patch.object(runner.time, "monotonic", side_effect=lambda: monotonic() + (30 if ready.exists() else 0)):
            result = runner.execute_replay_trial([sys.executable, "-c", parent], self.root / "descendants", dict(os.environ), 20, 1024)
        release.touch()
        self.assertTrue(result["timed_out"])
        self.assertIn("spawned", (self.root / "descendants" / "stdout.txt").read_text())
        time.sleep(0.9)
        self.assertFalse(marker.exists())

    @unittest.skipUnless(os.name == "nt", "Windows process containment")
    def test_containment_failure_retains_process_receipt(self):
        with mock.patch.object(runner.processes, "contain", side_effect=runner.processes.GateError("synthetic containment failure")):
            result = runner.execute_replay_trial([sys.executable, "-c", "import time;time.sleep(10)"],
                                                self.root / "containment", dict(os.environ), 1, 1024)
        self.assertTrue(any("synthetic containment failure" in error for error in result["io_errors"]))
        self.assertTrue((self.root / "containment" / "process.json").is_file())

    @unittest.skipUnless(os.name == "nt", "Windows process containment")
    def test_guard_failure_stops_before_launch_and_retains_process_receipt(self):
        with mock.patch.object(runner.processes, "guard", side_effect=runner.processes.GateError("synthetic guard failure")):
            with mock.patch.object(runner.subprocess, "Popen", side_effect=AssertionError("replay started before guard")) as launch:
                result = runner.execute_replay_trial(["replay"], self.root / "guard-failure", dict(os.environ), 1, 1024)
        launch.assert_not_called()
        self.assertIsNone(result["returncode"])
        self.assertIn("synthetic guard failure", result["io_errors"])
        self.assertTrue((self.root / "guard-failure" / "process.json").is_file())

    def test_wait_interrupt_cleans_owned_child_and_retains_receipt(self):
        sleep = time.sleep
        interruptions = []

        def interrupt_once(seconds):
            if not interruptions:
                interruptions.append(True)
                raise KeyboardInterrupt
            sleep(seconds)

        with mock.patch.object(runner.time, "sleep", side_effect=interrupt_once):
            result = runner.execute_replay_trial([sys.executable, "-c", "import time;time.sleep(10)"],
                                                self.root / "interrupt", dict(os.environ), 5, 1024)
        self.assertTrue(result["interrupted"])
        self.assertIn("interrupted", result["io_errors"])
        self.assertIsNotNone(result["returncode"])
        self.assertFalse(result["cleanup_failed"])
        self.assertTrue((self.root / "interrupt" / "process.json").is_file())


if __name__ == "__main__":
    unittest.main()
