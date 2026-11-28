import argparse
import contextlib
import importlib.util
import io
import itertools
import json
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import performance_capture
import performance_console


def load_runner():
    spec = importlib.util.spec_from_file_location("title_run_performance_under_test", TOOLS / "title_run.py")
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    return runner


def run_main(runner, root, extra=(), performance_result=None, smoke_result=None, relink=True, runs=1, console_failure=None):
    out = root / "out"
    work = out / "work/PPSA00001"
    work.mkdir(parents=True, exist_ok=True)
    (work / "relink.log").write_text("deliberate relink failure\n" if not relink else "converted\n", encoding="utf-8")
    package = root / "games/PPSA00001"
    package.mkdir(parents=True, exist_ok=True)
    titles = root / "titles.json"
    titles.write_text("{}", encoding="utf-8")
    found = {"PPSA00001": {"package": package, "name": "Acceptance fixture", "version": "01.000.000"}}
    smoke = smoke_result or {"outcome": "running", "exit_code": None, "seconds": 1, "presents": 60,
                             "fps": 60.0, "first_error": "", "warnings": {}, "frames": [], "report": None,
                             "os": "fixture OS", "gpu": "fixture GPU", "driver": "fixture driver", "cpu": "fixture CPU"}
    stdout, stderr = io.StringIO(), io.StringIO()
    console = mock.MagicMock()
    console.__enter__.return_value = types.SimpleNamespace(stdout=object(), stderr=object(),
        provenance={"kind": "fixture-terminal", "verified": True})
    with mock.patch.object(runner, "discover", return_value=found), \
            mock.patch.object(runner, "validate_layout"), \
            mock.patch.object(runner, "lock_work", return_value=io.BytesIO()), \
            mock.patch.object(runner, "git", side_effect=["abcdef123456", ""]), \
            mock.patch.object(runner, "reset_work"), \
            mock.patch.object(runner, "prepare", return_value=work / "app.exe" if relink else None) as prepare, \
            mock.patch.object(runner, "run_performance_title", return_value=performance_result or {"state": "complete"},
                              side_effect=performance_result if isinstance(performance_result, BaseException) else None) as performance, \
            mock.patch.object(runner, "run_title", side_effect=lambda *arguments, **keywords: dict(smoke)) as legacy, \
            mock.patch.object(performance_console, "InteractiveConsole", return_value=console, side_effect=console_failure) as console_constructor, \
            contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        code = runner.main(["--games", str(root / "games"), "--out", str(out), "--titles", str(titles),
                            *([] if runs is None else ["--runs", str(runs)]), *extra, "PPSA00001"])
    reports = list(out.glob("*/summary.json"))
    return {"code": code, "summary": json.loads(reports[0].read_text(encoding="utf-8")),
            "report_directory": reports[0].parent, "prepare": prepare, "performance": performance,
            "legacy": legacy, "stdout": stdout.getvalue(), "stderr": stderr.getvalue(),
            "console": console, "console_constructor": console_constructor}


class PerformanceFixture:
    def setUp(self):
        environment = mock.patch.dict(os.environ)
        environment.start()
        self.addCleanup(environment.stop)
        for name in ("ANYPS5_NO_SHADER_CACHE", "MESA_SHADER_CACHE_DISABLE", "__GL_SHADER_DISK_CACHE"):
            os.environ.pop(name, None)
        temporary = tempfile.TemporaryDirectory(prefix="anyps5-performance-integration-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.work = self.root / "work"
        self.work.mkdir()
        self.executable = self.work / "owned-probe.exe"
        self.runner = load_runner()
        self.console = types.SimpleNamespace(stdout=object(), stderr=object(),
                                              provenance={"kind": "fixture-terminal", "verified": True})
        self.capture = (self.root / "capture", "frames", (2, 5), "PPSA00001", "01.000.000")


class PerformancePhaseTests(PerformanceFixture, unittest.TestCase):
    def test_completed_capture_closes_and_kills_owned_session(self):
        for native_exit in (0, 17):
            session = mock.Mock()
            session.process.pid = 1
            session.images = {}
            session.exit_code.return_value = native_exit
            session.alive.side_effect = [True, True, False, False]
            clock = itertools.count(0, 0.25)
            with self.subTest(native_exit=native_exit), mock.patch.object(self.runner, "Session", return_value=session) as constructor, \
                    mock.patch.object(self.runner.time, "monotonic", side_effect=clock.__next__), \
                    mock.patch.object(self.runner.time, "sleep"), \
                    mock.patch.object(performance_capture, "inspect_capture", side_effect=[
                        {"state": "ready", "process_id": 1}, {"state": "complete", "process_id": 1, "warnings": []}]) as inspect:
                result = self.runner.performance_phase(self.executable, self.work, {"MARKER": "kept"},
                                                      self.console, 5, capture=self.capture)
            constructor.assert_called_once_with([str(self.executable)], self.work, {"MARKER": "kept"},
                                                self.console.stdout, self.console.stderr, owned=self.work, interactive=True)
            self.assertEqual(result["state"], "complete" if native_exit == 0 else "incomplete")
            self.assertEqual(result["exit_code"], native_exit)
            self.assertEqual(result["cleanup"], {"close_requested": True, "forced": False})
            session.close.assert_called_once()
            session.kill.assert_called_once()
            self.assertEqual(inspect.call_args_list[0].kwargs, {"metadata_only": True})
            self.assertEqual(inspect.call_args_list[1].kwargs, {})

    def test_early_crash_retains_exit_without_completion(self):
        for state, native_exit, process, images in (("pending", 17, 1, {}), ("complete", 17, 1, {}),
                ("complete", 0, 2, {2: str(self.work / "owned-descendant.exe")})):
            session = mock.Mock()
            session.images = images
            session.owned = str(self.work)
            session.process.pid = 1
            session.alive.return_value = False
            session.exit_code.return_value = native_exit
            evidence = {"state": state, "process_id": process, "warnings": [], "marker": "retained native artifact"}
            with self.subTest(state=state, native_exit=native_exit), mock.patch.object(self.runner.os, "name", "nt"), \
                    mock.patch.object(self.runner, "Session", return_value=session), \
                    mock.patch.object(performance_capture, "inspect_capture", return_value=evidence):
                result = self.runner.performance_phase(self.executable, self.work, {}, self.console, 5, capture=self.capture)
            self.assertEqual(result["state"], "complete" if native_exit == 0 else "incomplete")
            self.assertEqual(result["exit_code"], native_exit)
            self.assertEqual(result["marker"], "retained native artifact")
            if native_exit == 0:
                self.assertEqual(result["process_ownership"], "verified")
            session.close.assert_not_called()
            session.kill.assert_called_once()

    def test_deadline_with_pending_capture_is_incomplete(self):
        session = mock.Mock()
        session.images = {}
        session.alive.return_value = True
        clock = itertools.count(0, 1)
        with mock.patch.object(self.runner, "Session", return_value=session), \
                mock.patch.object(self.runner.time, "monotonic", side_effect=clock.__next__), \
                mock.patch.object(self.runner.time, "sleep"), \
                mock.patch.object(performance_capture, "inspect_capture", return_value={"state": "pending"}):
            result = self.runner.performance_phase(self.executable, self.work, {}, self.console, 2, capture=self.capture)
        self.assertEqual(result["state"], "incomplete")
        self.assertTrue(result["cleanup"]["forced"])
        session.close.assert_called_once()
        session.kill.assert_called_once()
        clock = itertools.count(0, 1)
        with mock.patch.object(self.runner, "Session", return_value=session), \
                mock.patch.object(self.runner.time, "monotonic", side_effect=clock.__next__), \
                mock.patch.object(self.runner.time, "sleep"):
            warmup = self.runner.performance_phase(self.executable, self.work, {}, self.console, 2)
        self.assertEqual(warmup["state"], "failed")
        self.assertTrue(warmup["cleanup"]["forced"])

    def test_invalid_capture_still_cleans_session(self):
        for state in ("invalid", "complete"):
            session = mock.Mock()
            session.process.pid = 1
            session.images = {}
            session.exit_code.return_value = 0
            session.alive.side_effect = [True, True, False, False]
            evidence = [{"state": "invalid", "reason": "bad gzip"}] * 2 if state == "invalid" else [
                {"state": "ready", "process_id": 999}, {"state": "complete", "process_id": 999, "warnings": []}]
            with self.subTest(state=state), mock.patch.object(self.runner, "Session", return_value=session), \
                    mock.patch.object(performance_capture, "inspect_capture", side_effect=evidence):
                result = self.runner.performance_phase(self.executable, self.work, {}, self.console, 5, capture=self.capture)
            self.assertEqual(result["state"], "invalid")
            self.assertIn("bad gzip" if state == "invalid" else "owned process", result["reason"])
            session.kill.assert_called_once()

    def test_keyboard_interrupt_kills_owned_session(self):
        session = mock.Mock()
        session.alive.return_value = True
        with mock.patch.object(self.runner, "Session", return_value=session), \
                mock.patch.object(self.runner.time, "sleep", side_effect=KeyboardInterrupt), \
                mock.patch.object(performance_capture, "inspect_capture", return_value={"state": "pending"}), \
                self.assertRaises(KeyboardInterrupt):
            self.runner.performance_phase(self.executable, self.work, {}, self.console, 5, capture=self.capture)
        session.kill.assert_called_once()

    def test_pad_is_queued_consistently_before_capture(self):
        (self.work / "pad-input.txt").write_text("old input", encoding="utf-8")
        pad = {"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]}
        session = mock.Mock()
        session.process.pid = 1
        session.images = {}
        session.exit_code.return_value = 0
        session.alive.side_effect = [True, True, False, False]
        clock = itertools.count(0, 0.25)
        recording = self.work / "pad-record.txt"

        def launch(*args, **kwargs):
            recording.write_text("APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0\n", encoding="utf-8")
            return session

        with mock.patch.object(self.runner, "Session", side_effect=launch) as constructor, \
                mock.patch.object(self.runner.time, "monotonic", side_effect=clock.__next__), \
                mock.patch.object(performance_capture, "inspect_capture", return_value={"state": "complete", "process_id": 1}), \
                mock.patch.object(self.runner.pad_timeline, "queue_timeline") as queued:
            result = self.runner.performance_phase(self.executable, self.work, {"APS5_PAD_RECORD": str(recording)}, self.console, 5, pad, self.capture)
        self.assertEqual(constructor.call_args.args[2]["APS5_PAD_INPUT_FILE"], str(self.work / "pad-input.txt"))
        self.assertFalse((self.work / "pad-input.txt").exists())
        queued.assert_called_once()
        self.assertIsNotNone(result["pad_queued_s"])


class PadPhaseReadinessTests(PerformanceFixture, unittest.TestCase):
    def run_phase(self, ready_after, recording=None):
        self.console.stderr = io.BytesIO()
        clock = [0.0]
        state = {"closed": False, "recorded": False}
        self.phase_index = getattr(self, "phase_index", 0) + 1
        trace_dir = self.root / f"phase{self.phase_index}"
        trace_dir.mkdir()
        env = {"APS5_TRACE_DIR": str(trace_dir)}
        if recording is not None:
            env["APS5_PAD_RECORD"] = str(recording)
        path = recording or trace_dir / "pad-record.txt"
        session = mock.Mock()
        session.process.pid = 1
        session.images = {}
        session.exit_code.return_value = 0

        def alive():
            if ready_after is not None and not state["recorded"] and clock[0] >= ready_after:
                path.write_text("APS5_PAD_RECORDING 1\n0 0 128 128 128 128 0 0\n", encoding="utf-8")
                state["recorded"] = True
            return not state["closed"]

        def sleep(seconds):
            clock[0] += seconds

        session.alive.side_effect = alive
        session.close.side_effect = lambda: state.update(closed=True)
        pad = {"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 200, "buttons": ["cross"]}]}
        with mock.patch.object(self.runner, "Session", return_value=session) as constructor, \
                mock.patch.object(self.runner.time, "monotonic", side_effect=lambda: clock[0]), \
                mock.patch.object(self.runner.time, "sleep", side_effect=sleep):
            result = self.runner.performance_phase(self.executable, self.work, env, self.console, 1, pad)
        return result, constructor.call_args.args[2]

    def test_phase_waits_for_the_first_controller_read(self):
        result, env = self.run_phase(0.5)
        self.assertGreaterEqual(result["pad_queued_s"], 0.5)
        self.assertTrue(Path(env["APS5_PAD_RECORD"]).is_file())
        self.assertNotIn(b"timeline not queued", self.console.stderr.getvalue())

    def test_phase_without_a_controller_read_queues_nothing(self):
        result, _ = self.run_phase(None)
        self.assertIsNone(result["pad_queued_s"])
        self.assertIn(b"no fresh controller sample observed", self.console.stderr.getvalue())

    def test_restart_does_not_reuse_the_warmup_recording(self):
        recording = self.work / "shared-record.txt"
        warmup, _ = self.run_phase(0, recording)
        capture, _ = self.run_phase(None, recording)
        self.assertIsNotNone(warmup["pad_queued_s"])
        self.assertIsNone(capture["pad_queued_s"])


class PerformanceRunTests(PerformanceFixture, unittest.TestCase):
    def test_warmup_and_capture_restart_with_same_work_cache_and_console(self):
        console = mock.MagicMock()
        console.__enter__.return_value = self.console
        pad = {"after_s": 0, "events": [{"at_ms": 0, "hold_ms": 100, "buttons": ["cross"]}]}
        with mock.patch.object(performance_console, "InteractiveConsole", return_value=console), \
                mock.patch.object(self.runner, "performance_phase", side_effect=[{"state": "completed"}, {"state": "complete"}]) as phase:
            result = self.runner.run_performance_title(self.executable, self.work, self.capture[0], 5, {},
                                                       "frames", (2, 5), "owned scene", 2, "PPSA00001", "01.000.000", pad)
        self.assertEqual(phase.call_count, 2)
        warm, capture = [call.args for call in phase.call_args_list]
        self.assertEqual(warm[:2], capture[:2])
        self.assertIs(warm[3], capture[3])
        self.assertIs(warm[5], pad)
        self.assertIs(capture[5], pad)
        self.assertEqual(warm[4], 2)
        self.assertEqual(capture[4], 5)
        self.assertEqual(warm[2]["MESA_SHADER_CACHE_DIR"], capture[2]["MESA_SHADER_CACHE_DIR"])
        self.assertEqual(warm[2]["__GL_SHADER_DISK_CACHE_PATH"], capture[2]["__GL_SHADER_DISK_CACHE_PATH"])
        self.assertNotIn("APS5_TRACE_FRAMES", warm[2])
        self.assertNotIn("APS5_TRACE_SECONDS", warm[2])
        self.assertEqual(capture[2]["APS5_TRACE_FRAMES"], "2:5")
        self.assertEqual(capture[6], self.capture)
        self.assertEqual(result["state"], "complete")
        self.assertFalse(result["scene"]["verified"])
        self.assertFalse(result["cache"]["scene_warmed_verified"])
        self.assertFalse(list(self.capture[0].rglob("stdout.log")))
        self.assertFalse(list(self.capture[0].rglob("stderr.log")))
        self.assertEqual(json.loads((self.capture[0] / "capture.json").read_text(encoding="utf-8")), result)
        console.__exit__.assert_called_once()
        with mock.patch.object(performance_console, "InteractiveConsole") as constructor, \
                mock.patch.object(self.runner, "performance_phase", side_effect=[
                    {"state": "completed"}, {"state": "complete"},
                    {"state": "completed"}, {"state": "complete"}]) as phase:
            for index in range(2):
                self.runner.run_performance_title(self.executable, self.work, self.root / f"borrowed-{index}", 5, {},
                                                  "frames", (2, 5), "owned scene", 2, "PPSA00001", console=self.console)
        constructor.assert_not_called()
        self.assertTrue(all(call.args[3] is self.console for call in phase.call_args_list))

    def test_failed_warmup_never_starts_capture(self):
        console = mock.MagicMock()
        console.__enter__.return_value = self.console
        with mock.patch.object(performance_console, "InteractiveConsole", return_value=console), \
                mock.patch.object(self.runner, "performance_phase", return_value={"state": "failed", "exit_code": 9}) as phase:
            result = self.runner.run_performance_title(self.executable, self.work, self.capture[0], 5, {},
                                                       "frames", (2, 5), "owned scene", 2, "PPSA00001")
        phase.assert_called_once()
        self.assertEqual(result["state"], "incomplete")
        self.assertEqual(result["warmup"]["exit_code"], 9)
        console.__exit__.assert_called_once()

    def test_window_settings_are_clean_before_launch(self):
        console = mock.MagicMock()
        console.__enter__.return_value = self.console
        inherited = {"APS5_TRACE_SUMMARY_EVERY": "10", "APS5_PROFILE_DRAW": "1", "APS5_DUMP_FRAMES": "50",
                     "ANYPS5_LOG_FILE": "inherited.log", "ANYPS5_FPS_LIMIT": "60", "SDL_AUDIODRIVER": "wasapi"}
        extra = {"APS5_TRACE_SECONDS": "10:10", "APS5_DUMP_SCALE": "8", "ANYPS5_FPS_LIMIT": "30"}
        with mock.patch.dict(self.runner.os.environ, inherited, clear=True), \
                mock.patch.object(performance_console, "InteractiveConsole", return_value=console), \
                mock.patch.object(self.runner, "performance_phase", side_effect=[{"state": "completed"}, {"state": "complete"}]) as phase:
            self.runner.run_performance_title(self.executable, self.work, self.capture[0], 5, extra,
                                              "frames", (2, 5), "owned scene", 2, "PPSA00001")
        for call in phase.call_args_list:
            env = call.args[2]
            self.assertNotIn("APS5_TRACE_SUMMARY_EVERY", env)
            self.assertNotIn("APS5_PROFILE_DRAW", env)
            self.assertNotIn("APS5_DUMP_FRAMES", env)
            self.assertNotIn("APS5_DUMP_SCALE", env)
            self.assertNotIn("ANYPS5_LOG_FILE", env)
            self.assertEqual(env["ANYPS5_FPS_LIMIT"], "30")
            self.assertEqual(env["SDL_AUDIODRIVER"], "wasapi")
        self.assertEqual(phase.call_args_list[1].args[2]["APS5_TRACE_FRAMES"], "2:5")
        self.assertNotIn("APS5_TRACE_SECONDS", phase.call_args_list[1].args[2])

    def test_invalid_environment_refuses_before_launch(self):
        for index, settings in enumerate(({"INVALID=NAME": "value"}, {"ANYPS5_NO_SHADER_CACHE": "1"},
                                          {"MESA_SHADER_CACHE_DISABLE": "true"}, {"__GL_SHADER_DISK_CACHE": "0"})):
            with self.subTest(settings=settings), mock.patch.object(performance_console, "InteractiveConsole") as console, \
                    mock.patch.object(self.runner, "performance_phase") as phase, self.assertRaises(ValueError):
                self.runner.run_performance_title(self.executable, self.work, self.root / f"invalid-{index}", 5,
                                                  settings, "frames", (2, 5), "owned scene", 2, "PPSA00001")
            phase.assert_not_called()
            console.assert_not_called()

    def test_interrupt_is_recorded_and_reraised(self):
        console = mock.MagicMock()
        console.__enter__.return_value = self.console
        with mock.patch.object(performance_console, "InteractiveConsole", return_value=console), \
                mock.patch.object(self.runner, "performance_phase", side_effect=KeyboardInterrupt), \
                self.assertRaises(KeyboardInterrupt):
            self.runner.run_performance_title(self.executable, self.work, self.capture[0], 5, {},
                                              "frames", (2, 5), "owned scene", 2, "PPSA00001")
        report = json.loads((self.capture[0] / "capture.json").read_text(encoding="utf-8"))
        self.assertEqual(report["state"], "incomplete")
        self.assertEqual(report["reason"], "interrupted")
        console.__exit__.assert_called_once()


class PerformanceArgumentTests(PerformanceFixture, unittest.TestCase):
    def test_performance_requires_one_window_scene(self):
        cases = (["--profile", "performance", "--scene", "scene"],
                 ["--profile", "performance", "--trace-frames", "2:5"],
                 ["--profile", "performance", "--trace-frames", "2:5", "--scene", " "],
                 ["--profile", "performance", "--trace-frames", "2:5", "--trace-seconds", "1:1", "--scene", "scene"])
        for extra in cases:
            with self.subTest(extra=extra), mock.patch.object(self.runner, "discover") as discover, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as stopped:
                self.runner.main(["--games", str(self.root), *extra])
            self.assertEqual(stopped.exception.code, 2)
            discover.assert_not_called()

    def test_performance_incompatible_modes_refused_before_discovery(self):
        for option in ("--compare", "--json", "--smoke"):
            extra = [option, "missing.json"] if option == "--compare" else [option]
            with self.subTest(option=option), mock.patch.object(self.runner, "discover") as discover, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as stopped:
                self.runner.main(["--games", str(self.root), "--profile", "performance", "--trace-frames", "2:5",
                                  "--scene", "scene", *extra])
            self.assertEqual(stopped.exception.code, 2)
            discover.assert_not_called()

    def test_smoke_default_rejects_performance_options(self):
        for extra in (["--trace-frames", "2:5"], ["--scene", "scene"], ["--warmup-seconds", "2"]):
            with self.subTest(extra=extra), mock.patch.object(self.runner, "discover") as discover, \
                    contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as stopped:
                self.runner.main(["--games", str(self.root), *extra])
            self.assertEqual(stopped.exception.code, 2)
            discover.assert_not_called()

    def test_window_parse_and_warmup_default(self):
        for frames, seconds, kind, window in (("0002:0005", None, "frames", (2, 5)), (None, "1:2", "seconds", (1, 2))):
            args = argparse.Namespace(profile="performance", trace_frames=frames, trace_seconds=seconds,
                                      scene="scene", warmup_seconds=None, compare=None, json=False, smoke=False,
                                      ids=["PPSA00001"])
            self.runner.validate_performance_args(argparse.ArgumentParser(), args)
            self.assertEqual(args.capture_kind, kind)
            self.assertEqual(args.capture_window, window)
            self.assertEqual(args.warmup_seconds, 60)


class PerformanceMainTests(PerformanceFixture, unittest.TestCase):
    def test_performance_main_writes_distinct_observational_report(self):
        result = run_main(self.runner, self.root, ["--profile", "performance", "--trace-frames", "2:5", "--scene", "scene"], runs=2)
        self.assertEqual(result["code"], 0)
        self.assertEqual(result["summary"]["profile"], "performance")
        self.assertEqual(result["summary"]["format"], "anyps5-performance-capture/1")
        self.assertEqual(result["summary"]["window"], {"kind": "frames", "spec": [2, 5]})
        run = result["summary"]["titles"]["PPSA00001"]["runs"][0]
        self.assertEqual(run["state"], "complete")
        self.assertNotIn("outcome", run)
        self.assertNotIn("fps", run)
        self.assertNotIn("cache", run)
        self.assertFalse((result["report_directory"] / "summary.md").exists())
        result["legacy"].assert_not_called()
        self.assertEqual(result["performance"].call_count, 2)
        result["console_constructor"].assert_called_once()
        result["console"].__enter__.assert_called_once()
        result["console"].__exit__.assert_called_once()
        borrowed = result["console"].__enter__.return_value
        self.assertTrue(all(call.kwargs["console"] is borrowed for call in result["performance"].call_args_list))
        default = run_main(self.runner, self.root / "default", ["--profile", "performance", "--trace-frames", "2:5", "--scene", "scene"], runs=None)
        default["performance"].assert_called_once()
        self.assertEqual(default["summary"]["runs"], 1)

    def test_failed_relink_cannot_be_complete(self):
        result = run_main(self.runner, self.root, ["--profile", "performance", "--trace-frames", "2:5", "--scene", "scene"], relink=False)
        self.assertEqual(result["code"], 1)
        result["performance"].assert_not_called()
        result["legacy"].assert_not_called()
        self.assertNotEqual(result["summary"]["titles"]["PPSA00001"]["runs"][0].get("state"), "complete")

    def test_default_smoke_calls_legacy_runner_only(self):
        result = run_main(self.runner, self.root, runs=None)
        self.assertEqual(result["code"], 0)
        result["performance"].assert_not_called()
        self.assertEqual(result["legacy"].call_count, 2)
        result["console_constructor"].assert_not_called()
        self.assertEqual(result["summary"]["runs"], 2)
        self.assertNotIn("profile", result["summary"])
        run = result["summary"]["titles"]["PPSA00001"]["runs"][0]
        self.assertEqual(run["outcome"], "running")
        self.assertEqual(run["cache"], "cold")
        self.assertTrue((result["report_directory"] / "summary.md").is_file())

    def test_invalid_performance_config_stops_before_preparation(self):
        with mock.patch.object(self.runner, "prepare") as prepare, \
                contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as stopped:
            self.runner.main(["--games", str(self.root), "--profile", "performance", "--trace-frames", "2:0", "--scene", "scene"])
        self.assertEqual(stopped.exception.code, 2)
        prepare.assert_not_called()

    def test_smoke_comparison_rejects_a_performance_summary_before_discovery(self):
        capture = run_main(self.runner, self.root, ["--profile", "performance", "--trace-frames", "2:5", "--scene", "scene"])
        baseline = capture["report_directory"] / "summary.json"
        for options in ([], ["--json"]):
            with self.subTest(options=options):
                stderr = io.StringIO()
                with mock.patch.object(self.runner, "discover", return_value={}) as discover, \
                        contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as stopped:
                    self.runner.main(["--games", str(self.root / "games"), "--compare", str(baseline), *options, "PPSA00001"])
                self.assertEqual(stopped.exception.code, 2)
                self.assertIn("--compare requires a smoke-run summary, got a performance capture summary", stderr.getvalue())
                discover.assert_not_called()

    def test_invalid_runtime_capture_is_retained_as_invalid(self):
        for index, (capture_error, console_error) in enumerate(((ValueError("bad native capture"), None),
                (ValueError("shader caches must be enabled"), None), (None, RuntimeError("no interactive console")))):
            with self.subTest(capture_error=capture_error, console_error=console_error):
                result = run_main(self.runner, self.root / f"invalid-{index}",
                    ["--profile", "performance", "--trace-seconds", "1:1", "--scene", "scene"],
                    performance_result=capture_error, console_failure=console_error)
            self.assertEqual(result["code"], 1)
            run = result["summary"]["titles"]["PPSA00001"]["runs"][0]
            self.assertEqual(run["state"], "invalid")
            self.assertEqual(run["reason"], str(capture_error or console_error))
            report = result["report_directory"] / "PPSA00001/run1/capture.json"
            recorded = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(recorded["state"], "invalid")
            self.assertEqual(recorded["reason"], run["reason"])
            if console_error is not None:
                result["performance"].assert_not_called()

    def test_main_exit_fails_unfinished_capture(self):
        for state in ("incomplete", "invalid"):
            with self.subTest(state=state), tempfile.TemporaryDirectory(dir=self.root) as directory:
                result = run_main(self.runner, Path(directory), ["--profile", "performance", "--trace-frames", "2:5", "--scene", "scene"],
                                  performance_result={"state": state, "reason": "retained evidence"})
            self.assertEqual(result["code"], 1)
            self.assertEqual(result["summary"]["titles"]["PPSA00001"]["runs"][0]["state"], state)


if __name__ == "__main__":
    unittest.main()
