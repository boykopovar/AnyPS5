from pathlib import Path
from types import SimpleNamespace
import contextlib
import io
import os
import re
import signal
import sys
import tempfile
import unittest
from unittest.mock import Mock, call, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import frame_capture_minimize
import vulkan_validation_gate as command_lifecycle
import test_frame_capture_minimize as minimizer_fixtures


class ReplayLifecycleTests(unittest.TestCase):
    def probe(self, root, source):
        runner = root / "probe.py"
        runner.write_text(
            "import os, signal, subprocess, sys, time\nfrom pathlib import Path\n"
            "from unittest.mock import patch\n"
            f"sys.path.insert(0, {str(Path(frame_capture_minimize.__file__).parent)!r})\n"
            "import frame_capture_minimize as target\n"
            "import vulkan_validation_gate as lifecycle\n"
            "root = Path(sys.argv[1])\n" + source, encoding="utf-8")
        log = root / "probe.log"
        try:
            code = command_lifecycle.run([sys.executable, str(runner), str(root)], log, os.environ.copy(), 12)
            self.assertEqual(code, 0, log.read_text(encoding="utf-8", errors="replace"))
        finally:
            if os.name == "posix":
                for path in root.glob("*.pid"):
                    with contextlib.suppress(ProcessLookupError):
                        os.kill(int(path.read_text(encoding="ascii")), signal.SIGKILL)

    def descendant_source(self, timeout):
        child = ("import os, sys, time; from pathlib import Path; "
                 "held = open(sys.argv[3], 'wb'); Path(sys.argv[1]).write_text(str(os.getpid()), encoding='ascii'); "
                 "time.sleep(1); Path(sys.argv[2]).touch(); time.sleep(10)")
        parent = ("import os, subprocess, sys, time\nfrom pathlib import Path\n"
                  "Path(sys.argv[4]).write_text(str(os.getpid()), encoding='ascii')\n"
                  f"subprocess.Popen([sys.executable, '-c', {child!r}, *sys.argv[1:4]])\n"
                  "deadline = time.monotonic() + 3\n"
                  "while not Path(sys.argv[1]).exists():\n"
                  "    if time.monotonic() >= deadline: raise RuntimeError('descendant did not start')\n"
                  "    time.sleep(.01)\n"
                  + ("time.sleep(10)\n" if timeout else "raise SystemExit(7)\n"))
        return (
            "ready, marker, held = (root / name for name in ('child.pid', 'late.marker', 'held.file'))\n"
            f"command = [sys.executable, '-c', {parent!r}, str(ready), str(marker), str(held), str(root / 'parent.pid')]\n"
            "with (root / 'replay.stdout').open('wb') as stdout, (root / 'replay.stderr').open('wb') as stderr:\n"
            "    try:\n"
            f"        code = target.run_replay(command, stdout, stderr, {0.6 if timeout else 5})\n"
            "    except subprocess.TimeoutExpired:\n"
            f"        assert {timeout!r}, 'normal replay timed out'\n"
            "    else:\n"
            f"        assert not {timeout!r} and code == 7, code\n"
            "assert ready.exists(), 'descendant never reached the protected region'\n"
            "held.unlink()\n"
            "time.sleep(1.1)\n"
            "assert not marker.exists(), 'descendant kept running after replay returned'\n"
        )

    def test_owned_descendant_is_drained_after_parent_exit(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), self.descendant_source(False))

    def test_owned_descendant_is_drained_after_timeout(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), self.descendant_source(True))

    def test_completed_descendant_preserves_the_replay_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), """parent = "import subprocess, sys; subprocess.run([sys.executable, '-c', 'pass'], check=True); raise SystemExit(7)"
with (root / 'out').open('wb') as stdout, (root / 'err').open('wb') as stderr:
    assert target.run_replay([sys.executable, '-c', parent], stdout, stderr, 5) == 7
""")

    @unittest.skipUnless(os.name == "nt", "Windows suspended process containment")
    def test_containment_failure_reaps_the_suspended_replay(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), """marker = root / 'uncontained.marker'
spawned = []
real_popen = subprocess.Popen
def popen(*args, **kwargs):
    process = real_popen(*args, **kwargs)
    spawned.append(process)
    return process
with (root / 'out').open('wb') as stdout, (root / 'err').open('wb') as stderr:
    with patch.object(target.subprocess, 'Popen', popen), patch.object(lifecycle, 'contain', side_effect=RuntimeError('assignment failed')):
        try:
            target.run_replay([sys.executable, '-c', f'from pathlib import Path; Path({str(marker)!r}).touch()'], stdout, stderr, 3)
        except target.ReplayCleanupError as error:
            assert 'assignment failed' in str(error), error
        else:
            raise AssertionError('containment failure was accepted')
assert len(spawned) == 1 and spawned[0].poll() is not None
assert not marker.exists(), 'suspended replay ran before containment'
""")

    @unittest.skipUnless(os.name == "nt", "Windows completion port association")
    def test_notification_association_failure_prevents_assignment_and_execution(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), """lifecycle.guard()
marker = root / 'unassociated.marker'
spawned, assigned = [], []
real_popen, real_win32 = subprocess.Popen, lifecycle.win32
def popen(*args, **kwargs):
    process = real_popen(*args, **kwargs)
    spawned.append(process)
    return process
def win32(name, *args, **kwargs):
    if name == 'SetInformationJobObject' and args[1] == 7:
        raise OSError('association failed')
    if name == 'AssignProcessToJobObject':
        assigned.append(args)
    return real_win32(name, *args, **kwargs)
with (root / 'out').open('wb') as stdout, (root / 'err').open('wb') as stderr:
    with patch.object(target.subprocess, 'Popen', popen), patch.object(lifecycle, 'win32', win32):
        try:
            target.run_replay([sys.executable, '-c', f'from pathlib import Path; Path({str(marker)!r}).touch()'], stdout, stderr, 3)
        except target.ReplayCleanupError as error:
            assert 'association failed' in str(error), error
        else:
            raise AssertionError('unassociated replay was accepted')
assert not assigned, 'replay was assigned before notification association succeeded'
assert len(spawned) == 1 and spawned[0].poll() is not None
assert not marker.exists(), 'replay executed before notification association'
""")

    @unittest.skipUnless(os.name == "nt", "Windows job notification accounting")
    def test_missing_descendant_notification_aborts_and_retains_work(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), """from types import SimpleNamespace
from unittest.mock import Mock
replay = root / 'replay.py'
replay.write_text("import subprocess, sys; subprocess.run([sys.executable, '-c', 'pass'], check=True); print('failure'); raise SystemExit(7)\\n", encoding='utf-8')
workdir = root / 'work'
workdir.mkdir()
args = SimpleNamespace(filter='filter', replay=replay, capture=root / 'capture', output=root / 'output', replay_arg=[], replay_timeout=5, expect_exit_code=7)
pattern = Mock()
minimizer = target.Minimizer(args, pattern)
with patch.object(target.ReplayJob, 'collect', lambda self: None), patch.object(target, 'REPLAY_DRAIN_SECONDS', .2):
    try:
        minimizer.replay_once(root / 'capture', workdir)
    except target.ReplayCleanupError as error:
        assert 'membership or termination is unconfirmed' in str(error), error
    else:
        raise AssertionError('missing descendant notification was accepted')
assert minimizer.cleanup_failed
pattern.search.assert_not_called()
minimizer.cleanup(workdir)
assert workdir.is_dir(), 'unconfirmed cleanup deleted trial files'
""")

    @unittest.skipUnless(os.name == "nt", "Windows job cleanup")
    def test_interrupted_replay_drains_its_owned_descendant(self):
        with tempfile.TemporaryDirectory() as directory:
            source = self.descendant_source(True)
            before = source.split("with (root / 'replay.stdout')", 1)[0]
            source = before + """def interrupted(process, tracker, timeout):
    deadline = time.monotonic() + 3
    while not ready.exists():
        if time.monotonic() >= deadline: raise RuntimeError('descendant did not start')
        time.sleep(.01)
    raise KeyboardInterrupt
with (root / 'replay.stdout').open('wb') as stdout, (root / 'replay.stderr').open('wb') as stderr:
    with patch.object(target, 'wait_replay_job', interrupted):
        try:
            target.run_replay(command, stdout, stderr, 5)
        except KeyboardInterrupt:
            pass
        else:
            raise AssertionError('interrupt was swallowed')
assert ready.exists()
held.unlink()
time.sleep(1.1)
assert not marker.exists(), 'descendant survived interruption'
"""
            self.probe(Path(directory), source)

    def test_replay_predicate_keeps_stdout_before_stderr(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            replay = root / "streams.py"
            replay.write_text("import sys\nsys.stderr.write('second\\n'); sys.stderr.flush()\nsys.stdout.write('first\\n'); sys.stdout.flush()\nraise SystemExit(7)\n", encoding="utf-8")
            args = SimpleNamespace(filter="filter", replay=replay, capture=root / "capture", output=root / "output",
                                   replay_arg=[], replay_timeout=5, expect_exit_code=7)
            minimizer = frame_capture_minimize.Minimizer(args, re.compile(r"\Afirst\n\nsecond\n\Z"))
            self.assertEqual(minimizer.replay_once(root / "capture", root), (True, "exit=7, predicate=matched"))

    def test_unconfirmed_cleanup_aborts_before_predicate_or_file_read(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pattern = Mock()
            args = SimpleNamespace(filter="filter", replay="replay", capture=root / "capture", output=root / "output",
                                   replay_arg=[], replay_timeout=1, expect_exit_code=7)
            minimizer = frame_capture_minimize.Minimizer(args, pattern)
            error = frame_capture_minimize.ReplayCleanupError("tree still active")
            with patch.object(frame_capture_minimize, "run_replay", side_effect=error), patch.object(Path, "read_text", side_effect=AssertionError("read before drain")):
                with self.assertRaisesRegex(frame_capture_minimize.ReplayCleanupError, "tree still active"):
                    minimizer.replay_once(root / "capture", root)
            self.assertTrue(minimizer.cleanup_failed)
            pattern.search.assert_not_called()

    @unittest.skipUnless(os.name == "nt", "Windows job cleanup")
    def test_strict_cleanup_failure_overrides_exit_timeout_and_interrupt(self):
        with tempfile.TemporaryDirectory() as directory:
            self.probe(Path(directory), """real_drain = target.drain_replay_job
for outcome in ('exit', 'timeout', 'interrupt'):
    def waited(process, tracker, timeout):
        if outcome == 'timeout': raise subprocess.TimeoutExpired(process.args, timeout)
        if outcome == 'interrupt': raise KeyboardInterrupt
        return 7
    def failed_drain(job, process, tracker):
        real_drain(job, process, tracker)
        raise target.ReplayCleanupError('tree still active')
    with (root / f'{outcome}.stdout').open('wb') as stdout, (root / f'{outcome}.stderr').open('wb') as stderr:
        with patch.object(target, 'wait_replay_job', waited), patch.object(target, 'drain_replay_job', failed_drain):
            try:
                target.run_replay([sys.executable, '-c', 'import time; time.sleep(10)'], stdout, stderr, 1)
            except target.ReplayCleanupError as error:
                assert 'tree still active' in str(error), error
            else:
                raise AssertionError(f'{outcome} survived failed strict cleanup')
""")

    @unittest.skipUnless(os.name == "nt", "Windows suspended process containment")
    def test_interruption_during_containment_marks_cleanup_unconfirmed(self):
        process = Mock(pid=123)
        with patch.object(command_lifecycle, "guard"), patch.object(command_lifecycle, "contain", side_effect=KeyboardInterrupt):
            with patch.object(frame_capture_minimize.subprocess, "Popen", return_value=process):
                with self.assertRaises(frame_capture_minimize.ReplayCleanupError):
                    frame_capture_minimize.run_replay(["replay"], io.BytesIO(), io.BytesIO(), 1)
        process.kill.assert_called_once()
        process.wait.assert_called_once_with(timeout=frame_capture_minimize.REPLAY_DRAIN_SECONDS)

    def test_file_close_failure_cannot_clear_unconfirmed_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            args = SimpleNamespace(filter="filter", replay="replay", capture=root / "capture", output=root / "output",
                                   replay_arg=[], replay_timeout=1, expect_exit_code=7)
            minimizer = frame_capture_minimize.Minimizer(args, re.compile("failure"))
            @contextlib.contextmanager
            def failing_file(*args, **kwargs):
                try:
                    yield io.BytesIO()
                finally:
                    raise OSError("close failed")
            with patch.object(Path, "open", failing_file), patch.object(frame_capture_minimize, "run_replay", side_effect=frame_capture_minimize.ReplayCleanupError("tree still active")):
                with self.assertRaisesRegex(OSError, "close failed"):
                    minimizer.replay_once(root / "capture", root)
            self.assertTrue(minimizer.cleanup_failed)

    def test_unconfirmed_cleanup_retains_each_callers_work_and_stops_trials(self):
        for phase, fail_at, has_calls in (("baseline", 1, False), ("candidate", 3, True), ("final", 3, False)):
            with self.subTest(phase=phase), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                arguments, _, _, output = minimizer_fixtures.FrameCaptureMinimizeTests.empty_cli(self, root)
                temporary = root / "temporary"
                temporary.mkdir()
                arguments.extend(["--temp-dir", str(temporary)])
                description = {"calls": [{"index": 0, "kind": "submit", "memory_group": None}] if has_calls else [], "chunks": []}
                workdirs = []
                def replay(command, stdout, stderr, timeout):
                    workdirs.append(Path(stdout.name).parent)
                    if len(workdirs) == fail_at:
                        raise frame_capture_minimize.ReplayCleanupError("descendants still active")
                    stdout.write(b"failure\n")
                    return 7
                stderr = io.StringIO()
                with patch.object(frame_capture_minimize.Minimizer, "inspect", return_value=description), patch.object(frame_capture_minimize, "run_replay", replay):
                    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                        result = frame_capture_minimize.main(arguments)
                self.assertEqual(result, 2)
                self.assertEqual(len(workdirs), fail_at)
                retained = workdirs[-1]
                self.assertTrue(retained.is_dir(), retained)
                self.assertIn(str(retained if phase != "candidate" else retained.parent), stderr.getvalue())
                self.assertIn("retaining", stderr.getvalue())
                self.assertEqual(output.exists(), phase == "final")
                self.assertTrue(all(not path.exists() for path in workdirs[:-1]))

    def test_interrupt_removes_owned_output_after_confirmed_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = minimizer_fixtures.FrameCaptureMinimizeTests.empty_cli(self, root)
            calls = []
            def replay(command, stdout, stderr, timeout):
                calls.append(Path(stdout.name).parent)
                if len(calls) == 3:
                    raise KeyboardInterrupt
                stdout.write(b"failure\n")
                return 7
            with patch.object(frame_capture_minimize, "run_replay", replay):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(frame_capture_minimize.main(arguments), 130)
            self.assertFalse(output.exists())
            self.assertTrue(all(not path.exists() for path in calls))

    def test_cleanup_failure_during_interrupt_retains_owned_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            arguments, _, _, output = minimizer_fixtures.FrameCaptureMinimizeTests.empty_cli(self, root)
            calls = []
            def replay(command, stdout, stderr, timeout):
                calls.append(Path(stdout.name).parent)
                if len(calls) == 3:
                    try:
                        raise KeyboardInterrupt
                    except KeyboardInterrupt as error:
                        raise frame_capture_minimize.ReplayCleanupError("interrupted cleanup") from error
                stdout.write(b"failure\n")
                return 7
            stderr = io.StringIO()
            with patch.object(frame_capture_minimize, "run_replay", replay):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                    self.assertEqual(frame_capture_minimize.main(arguments), 2)
            self.assertTrue(output.is_dir())
            self.assertTrue(calls[-1].is_dir())
            self.assertIn("interrupted cleanup", stderr.getvalue())
            self.assertIn(str(output), stderr.getvalue())

    def test_replay_refuses_nonfinite_timeout_before_launch(self):
        for timeout in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(timeout=timeout), patch.object(frame_capture_minimize.subprocess, "Popen") as popen:
                with self.assertRaisesRegex(RuntimeError, "finite and positive"):
                    frame_capture_minimize.run_replay(["replay"], io.BytesIO(), io.BytesIO(), timeout)
                popen.assert_not_called()

    def test_cli_reports_nonfinite_timeout_without_starting_replay(self):
        for timeout in ("nan", "inf"):
            with self.subTest(timeout=timeout), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                arguments, _, _, output = minimizer_fixtures.FrameCaptureMinimizeTests.empty_cli(self, root)
                arguments.extend(["--replay-timeout", timeout])
                stderr = io.StringIO()
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                    try:
                        result = frame_capture_minimize.main(arguments)
                    except SystemExit as error:
                        result = error.code
                self.assertEqual(result, 2)
                self.assertIn("finite", stderr.getvalue())
                self.assertFalse(output.exists())

    def test_unsupported_python_host_does_not_start_replay(self):
        for platform in ("cygwin", "msys"):
            with self.subTest(platform=platform), patch.object(frame_capture_minimize.sys, "platform", platform):
                with patch.object(frame_capture_minimize.subprocess, "Popen") as popen:
                    with self.assertRaisesRegex(RuntimeError, "native Windows Python or a POSIX host"):
                        frame_capture_minimize.run_replay(["replay"], io.BytesIO(), io.BytesIO(), 1)
                    popen.assert_not_called()

    def test_process_group_permission_failure_reaps_root_and_fails_closed(self):
        process = Mock(pid=123)
        with patch.object(frame_capture_minimize.signal, "SIGKILL", 9, create=True):
            with patch.object(frame_capture_minimize.os, "killpg", side_effect=PermissionError("not allowed"), create=True):
                with self.assertRaisesRegex(frame_capture_minimize.ReplayCleanupError, "not allowed"):
                    frame_capture_minimize.drain_replay_group(process)
        process.kill.assert_called_once()
        process.wait.assert_called_once()

    def test_process_group_that_remains_after_termination_is_fatal_and_bounded(self):
        process = Mock(pid=123)
        with patch.object(frame_capture_minimize.signal, "SIGKILL", 9, create=True):
            with patch.object(frame_capture_minimize.os, "killpg", create=True) as killpg:
                with patch.object(frame_capture_minimize, "REPLAY_DRAIN_SECONDS", 1), patch.object(frame_capture_minimize.time, "monotonic", side_effect=[0, 0, 2]):
                    with self.assertRaisesRegex(frame_capture_minimize.ReplayCleanupError, "group remains"):
                        frame_capture_minimize.drain_replay_group(process)
        self.assertEqual(killpg.call_args_list, [call(123, 9), call(123, 0)])
        process.wait.assert_called_once_with(timeout=1)

    def test_darwin_probe_retries_until_the_process_group_disappears(self):
        process = Mock(pid=123)
        with (
            patch.object(frame_capture_minimize.sys, "platform", "darwin"),
            patch.object(frame_capture_minimize.signal, "SIGKILL", 9, create=True),
            patch.object(frame_capture_minimize.os, "killpg", create=True,
                         side_effect=[None, PermissionError("not allowed"), ProcessLookupError()]) as killpg,
            patch.object(frame_capture_minimize.time, "sleep"),
        ):
            frame_capture_minimize.drain_replay_group(process)
        self.assertEqual(killpg.call_args_list, [call(123, 9), call(123, 0), call(123, 0)])
        process.wait.assert_called_once()

    def test_darwin_probe_does_not_accept_persistent_permission_failure(self):
        process = Mock(pid=123)
        denied = PermissionError("not allowed")
        with (
            patch.object(frame_capture_minimize.sys, "platform", "darwin"),
            patch.object(frame_capture_minimize.signal, "SIGKILL", 9, create=True),
            patch.object(frame_capture_minimize.os, "killpg", create=True,
                         side_effect=[None, denied, denied]) as killpg,
            patch.object(frame_capture_minimize, "REPLAY_DRAIN_SECONDS", 1),
            patch.object(frame_capture_minimize.time, "monotonic", side_effect=[0, 0, 0.5, 1]),
            patch.object(frame_capture_minimize.time, "sleep"),
        ):
            with self.assertRaisesRegex(frame_capture_minimize.ReplayCleanupError, "group remains"):
                frame_capture_minimize.drain_replay_group(process)
        self.assertEqual(killpg.call_args_list, [call(123, 9), call(123, 0), call(123, 0)])
        process.wait.assert_called_once_with(timeout=1)


if __name__ == "__main__":
    unittest.main()
