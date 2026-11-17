import contextlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import performance_console


@contextlib.contextmanager
def console_probe():
    kernel, user = mock.Mock(), mock.Mock()
    kernel.GetConsoleWindow.return_value = 42
    kernel.GetStdHandle.side_effect = (100, 101, 102)
    kernel.AllocConsole.return_value = True
    kernel.FreeConsole.return_value = True
    kernel.SetStdHandle.return_value = True
    kernel.GetConsoleMode.return_value = True
    user.IsWindowVisible.return_value = True
    streams = [mock.Mock(), mock.Mock()]
    for index, stream in enumerate(streams):
        stream.fileno.return_value = 200 + index
    with contextlib.ExitStack() as stack:
        stack.enter_context(mock.patch.object(performance_console.os, "name", "nt"))
        stack.enter_context(mock.patch.object(performance_console.ctypes, "WinDLL", side_effect=(kernel, user), create=True))
        stack.enter_context(mock.patch.object(performance_console.ctypes, "get_last_error", return_value=5, create=True))
        stack.enter_context(mock.patch.object(performance_console.ctypes, "WinError", return_value=OSError(5, "test allocation denied"), create=True))
        stack.enter_context(mock.patch.dict(sys.modules, {"msvcrt": types.SimpleNamespace(get_osfhandle=lambda value: value)}))
        opened = stack.enter_context(mock.patch("builtins.open", side_effect=streams))
        yield kernel, user, streams, opened


class PerformanceConsoleTests(unittest.TestCase):
    def test_posix_requires_actual_terminal_fds(self):
        stdout, stderr = mock.Mock(), mock.Mock()
        stdout.fileno.return_value, stderr.fileno.return_value = 10, 11
        stdout.isatty.return_value = stderr.isatty.return_value = True
        with mock.patch.object(performance_console.os, "name", "posix"), \
                mock.patch.object(performance_console.os, "isatty", side_effect=(True, False)), \
                mock.patch.object(sys, "stdout", stdout), mock.patch.object(sys, "stderr", stderr):
            with self.assertRaisesRegex(RuntimeError, "both be terminal.*interactive terminal"):
                with performance_console.InteractiveConsole():
                    self.fail("redirected stderr was accepted")
        stdout.close.assert_not_called()
        stderr.close.assert_not_called()

    def test_posix_preserves_parent_streams(self):
        stdout, stderr = mock.Mock(), mock.Mock()
        stdout.fileno.return_value, stderr.fileno.return_value = 10, 11
        with mock.patch.object(performance_console.os, "name", "posix"), \
                mock.patch.object(performance_console.os, "isatty", return_value=True) as isatty, \
                mock.patch.object(sys, "stdout", stdout), mock.patch.object(sys, "stderr", stderr):
            with performance_console.InteractiveConsole() as console:
                self.assertIs(console.stdout, stdout)
                self.assertIs(console.stderr, stderr)
                self.assertEqual(console.provenance, {"kind": "posix-terminal", "verified": True})
            self.assertIs(sys.stdout, stdout)
            self.assertIs(sys.stderr, stderr)
        self.assertEqual(isatty.call_args_list, [mock.call(10), mock.call(11)])
        stdout.close.assert_not_called()
        stderr.close.assert_not_called()

    def test_windows_existing_console_preserves_parent(self):
        before = sys.stdout, sys.stderr, dict(os.environ)
        with console_probe() as (kernel, user, streams, opened):
            with performance_console.InteractiveConsole() as console:
                self.assertIs(console.stdout, streams[0])
                self.assertIs(console.stderr, streams[1])
                self.assertEqual(console.provenance["kind"], "windows-existing-console")
                self.assertTrue(console.provenance["verified"])
            kernel.AllocConsole.assert_not_called()
            kernel.FreeConsole.assert_not_called()
            kernel.SetStdHandle.assert_not_called()
            user.ShowWindow.assert_not_called()
            self.assertEqual(kernel.GetConsoleMode.call_count, 2)
            self.assertEqual(opened.call_args_list, [mock.call("CONOUT$", "wb", buffering=0)] * 2)
            for stream in streams:
                stream.close.assert_called_once_with()
        self.assertEqual(before, (sys.stdout, sys.stderr, dict(os.environ)))

    def test_windows_allocates_and_restores_standard_handles(self):
        with console_probe() as (kernel, user, streams, opened):
            kernel.GetConsoleWindow.side_effect = (0, 42)
            with performance_console.InteractiveConsole() as console:
                self.assertEqual(console.provenance["kind"], "windows-owned-console")
                self.assertTrue(console.provenance["window_visible"])
                self.assertEqual(kernel.SetStdHandle.call_count, 3)
                kernel.FreeConsole.assert_not_called()
            expected = [mock.call(index & 0xffffffff, handle) for index, handle in zip((-10, -11, -12), (100, 101, 102))]
            self.assertEqual(kernel.SetStdHandle.call_args_list, expected + expected)
            kernel.AllocConsole.assert_called_once_with()
            kernel.FreeConsole.assert_called_once_with()
            user.ShowWindow.assert_called_once_with(42, 5)
            console.close()
            self.assertEqual(kernel.FreeConsole.call_count, 1)

    def test_windows_does_not_detach_existing_console(self):
        with console_probe() as (kernel, user, streams, opened):
            user.IsWindowVisible.return_value = False
            with performance_console.InteractiveConsole() as console:
                self.assertFalse(console.provenance["window_visible"])
                self.assertTrue(console.provenance["verified"])
            kernel.AllocConsole.assert_not_called()
            kernel.FreeConsole.assert_not_called()
            user.ShowWindow.assert_not_called()

    def test_windows_rejects_nonconsole_output(self):
        with console_probe() as (kernel, user, streams, opened):
            kernel.GetConsoleMode.return_value = False
            with self.assertRaisesRegex(RuntimeError, "not a Windows console handle.*interactive terminal"):
                with performance_console.InteractiveConsole():
                    self.fail("nonconsole handle was accepted")
            streams[0].close.assert_called_once_with()
            kernel.FreeConsole.assert_not_called()

    def test_windows_cleans_allocated_console_on_open_failure(self):
        with console_probe() as (kernel, user, streams, opened):
            kernel.GetConsoleWindow.side_effect = (0, 42)
            opened.side_effect = OSError("cannot open console output")
            with self.assertRaisesRegex(RuntimeError, "cannot open console output"):
                with performance_console.InteractiveConsole():
                    self.fail("failed console was accepted")
            kernel.FreeConsole.assert_called_once_with()
            self.assertEqual(kernel.SetStdHandle.call_count, 6)

    def test_windows_cleanup_releases_owned_console_after_close_error(self):
        with console_probe() as (kernel, user, streams, opened):
            kernel.GetConsoleWindow.side_effect = (0, 42)
            streams[0].close.side_effect = OSError("console close failed")
            with self.assertRaisesRegex(OSError, "console close failed"):
                with performance_console.InteractiveConsole():
                    pass
            streams[1].close.assert_called_once_with()
            kernel.FreeConsole.assert_called_once_with()
            self.assertEqual(kernel.SetStdHandle.call_count, 6)

    def test_windows_allocation_failure_is_actionable(self):
        with console_probe() as (kernel, user, streams, opened):
            kernel.GetConsoleWindow.return_value = 0
            kernel.AllocConsole.return_value = False
            with self.assertRaisesRegex(RuntimeError, "test allocation denied.*interactive terminal"):
                with performance_console.InteractiveConsole():
                    self.fail("failed allocation was accepted")
            opened.assert_not_called()
            kernel.FreeConsole.assert_not_called()

    def test_context_rejects_reentry(self):
        with console_probe():
            with performance_console.InteractiveConsole() as console:
                with self.assertRaisesRegex(RuntimeError, "already active"):
                    console.__enter__()
                self.assertIsNotNone(console.stdout)

    def test_exit_does_not_suppress_exceptions(self):
        with console_probe() as (kernel, user, streams, opened):
            with self.assertRaisesRegex(ValueError, "capture failed"):
                with performance_console.InteractiveConsole():
                    raise ValueError("capture failed")
            for stream in streams:
                stream.close.assert_called_once_with()

    @unittest.skipUnless(os.name == "nt" and os.environ.get("ANYPS5_TEST_INTERACTIVE_CONSOLE") == "1",
                         "explicit Windows interactive console acceptance is required")
    def test_windows_native_child_receives_console_handles(self):
        child = "import ctypes,json,msvcrt,os,sys;from ctypes import wintypes;k=ctypes.WinDLL('kernel32',use_last_error=True);k.GetConsoleMode.argtypes=(wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD));result={}\nfor name,stream in [('stdout',sys.stdout),('stderr',sys.stderr)]:\n mode=wintypes.DWORD();result[name]=bool(k.GetConsoleMode(msvcrt.get_osfhandle(stream.fileno()),ctypes.byref(mode))) and os.isatty(stream.fileno())\nopen(sys.argv[1],'w',encoding='utf-8').write(json.dumps(result));assert all(result.values())"
        worker = """import ctypes
from ctypes import wintypes
import json
import os
import subprocess
import sys

sys.path.insert(0, sys.argv[1])
from performance_console import InteractiveConsole

k = ctypes.WinDLL('kernel32')
k.GetStdHandle.argtypes = (wintypes.DWORD,)
k.GetStdHandle.restype = wintypes.HANDLE
k.GetConsoleWindow.restype = wintypes.HWND
indices = [-10 & 0xffffffff, -11 & 0xffffffff, -12 & 0xffffffff]
before = [k.GetStdHandle(i) for i in indices]
streams = (sys.stdout, sys.stderr)
environment = dict(os.environ)
receipts = []
for attempt in range(2):
    with InteractiveConsole() as console:
        outer = dict(console.provenance)
        assert before == [k.GetStdHandle(i) for i in indices]
        with InteractiveConsole() as existing:
            inner = dict(existing.provenance)
            done = subprocess.run([sys.executable, '-c', sys.argv[2], sys.argv[3] + str(attempt)],
                                  stdout=existing.stdout, stderr=existing.stderr, timeout=10)
            assert done.returncode == 0
        assert k.GetConsoleWindow()
    assert not k.GetConsoleWindow()
    assert before == [k.GetStdHandle(i) for i in indices]
    assert streams == (sys.stdout, sys.stderr)
    assert environment == dict(os.environ)
    receipts.append({'outer': outer, 'inner': inner})
print(json.dumps(receipts))
"""
        with tempfile.TemporaryDirectory(prefix="anyps5-console-") as directory:
            evidence = Path(directory) / "child.json"
            result = subprocess.run([sys.executable, "-c", worker, str(TOOLS), child, str(evidence)],
                                    capture_output=True, text=True, creationflags=subprocess.DETACHED_PROCESS, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            receipts = json.loads(result.stdout)
            self.assertEqual(len(receipts), 2)
            for index, receipt in enumerate(receipts):
                self.assertEqual(receipt["outer"]["kind"], "windows-owned-console")
                self.assertEqual(receipt["inner"]["kind"], "windows-existing-console")
                observed = Path(str(evidence) + str(index))
                self.assertEqual(json.loads(observed.read_text(encoding="utf-8")), {"stdout": True, "stderr": True})

    @unittest.skipIf(os.name == "nt", "POSIX terminals are required")
    def test_posix_native_child_receives_terminal_fds(self):
        import pty
        primary, secondary = pty.openpty()
        try:
            with os.fdopen(os.dup(secondary), "wb", buffering=0) as stdout, \
                    os.fdopen(os.dup(secondary), "wb", buffering=0) as stderr:
                with mock.patch.object(sys, "stdout", stdout), mock.patch.object(sys, "stderr", stderr):
                    with performance_console.InteractiveConsole() as console:
                        result = subprocess.run([sys.executable, "-c", "import os,sys;assert os.isatty(1) and os.isatty(2)"],
                                                stdout=console.stdout, stderr=console.stderr, timeout=5)
                        self.assertEqual(result.returncode, 0)
                self.assertFalse(stdout.closed)
                self.assertFalse(stderr.closed)
        finally:
            os.close(primary)
            os.close(secondary)


if __name__ == "__main__":
    unittest.main()
