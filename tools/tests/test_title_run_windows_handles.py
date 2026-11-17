import contextlib
import ctypes
import gc
import ntpath
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import types
import unittest
import unittest.mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import title_run
from test_title_run import wait_until


class WindowsSessionNative:
    class Process:
        pid = 10
        _handle = 900

        def __init__(self, native):
            self.native = native
            self.returncode = None
            self.killed = False
            self.reaped = False
            self.wait_error = None

        def poll(self):
            return self.returncode

        def kill(self):
            self.killed = True
            self.returncode = 1

        def wait(self, timeout=None):
            self.reaped = True
            if self.wait_error:
                raise self.wait_error
            self.returncode = self.returncode if self.returncode is not None else 0
            return self.returncode

    class Ids(ctypes.Structure):
        _fields_ = [("assigned", ctypes.c_uint32), ("listed", ctypes.c_uint32), ("ids", ctypes.c_size_t * 256)]

    class Message(ctypes.Structure):
        _fields_ = [("message", ctypes.c_uint32)]

    def __init__(self):
        self.process = self.Process(self)
        self.live = {10, 20}
        self.allocated = set()
        self.closed = []
        self.close_fail = set()
        self.create_ok = self.assign_ok = self.terminate_ok = self.unhook_ok = True
        self.threads, self.sessions, self.thread_errors = [], [], []
        self.queue_created = self.hook_queue_ready = False
        self.hook = None
        self.hook_ok = True
        self.query_entered = threading.Event()
        self.query_release = threading.Event()
        self.query_release.set()
        self.quit = threading.Event()
        self.loop_error = threading.Event()
        self.codes = {10: 259, 20: 259}

    def CreateJobObjectW(self, *_):
        if self.create_ok:
            self.allocated.add(100)
            return 100
        return 0

    def AssignProcessToJobObject(self, *_):
        return self.assign_ok

    def TerminateJobObject(self, handle, _):
        self.valid(handle)
        if not self.terminate_ok:
            return False
        self.live.clear()
        self.codes = {pid: 1 if value == 259 else value for pid, value in self.codes.items()}
        self.process.returncode = self.process.returncode if self.process.returncode is not None else 1
        return True

    def valid(self, handle):
        if handle not in self.allocated or handle in self.closed:
            raise AssertionError("native operation on an unowned or closed handle")

    def CloseHandle(self, handle):
        self.valid(handle)
        if handle in self.close_fail:
            return False
        self.closed.append(handle)
        return True

    def QueryInformationJobObject(self, handle, kind, output, size, length):
        self.valid(handle)
        self.query_entered.set()
        if not self.query_release.wait(2):
            raise RuntimeError("controlled query was not released")
        self.valid(handle)
        output._obj.listed = len(self.live)
        for index, pid in enumerate(sorted(self.live)):
            output._obj.ids[index] = pid
        return True

    def OpenProcess(self, access, inherit, pid):
        handle = 1000 + pid
        self.allocated.add(handle)
        return handle

    def QueryFullProcessImageNameW(self, handle, flags, output, size):
        self.valid(handle)
        output.value = r"C:\owned\title.exe" if handle == 1010 else r"C:\outside\helper.exe"
        return True

    def GetExitCodeProcess(self, handle, output):
        self.valid(handle)
        output._obj.value = self.codes[handle - 1000]
        return True

    def GetProcessTimes(self, handle, created, exited, kernel, user):
        self.valid(handle)
        created._obj.value = handle
        return True

    def GetCurrentThreadId(self):
        return threading.get_ident()

    def PeekMessageW(self, message, window, first, last, remove):
        self.queue_created = True
        if self.loop_error.is_set():
            raise OSError("message pump failed")
        return False

    def SetWinEventHook(self, *args):
        self.hook_queue_ready = self.queue_created
        if not self.hook_ok:
            return 0
        self.hook = threading.get_ident()
        return 200

    def UnhookWinEvent(self, handle):
        if handle != 200 or self.hook != threading.get_ident():
            raise AssertionError("hook removed from the wrong thread")
        self.hook = None
        return self.unhook_ok

    def GetMessageW(self, *args):
        self.quit.wait(2)
        if self.loop_error.is_set():
            raise OSError("message pump failed")
        return 0

    def PostThreadMessageW(self, *args):
        self.quit.set()
        return True

    def EnumWindows(self, callback, argument):
        callback(50, argument)
        return True

    def GetWindowThreadProcessId(self, window, owner):
        owner._obj.value = 10
        return 1

    def IsWindowVisible(self, window):
        return True

    def PostMessageW(self, *args):
        return True

    @contextlib.contextmanager
    def installed(self, thread_factory=None):
        native = self

        def thread(*args, **kwargs):
            created = threading.Thread(*args, **kwargs)
            self.threads.append(created)
            return created

        win_types = types.SimpleNamespace(DWORD=ctypes.c_uint32, MSG=self.Message)
        c_types = types.SimpleNamespace(byref=ctypes.byref, sizeof=ctypes.sizeof, c_ulonglong=ctypes.c_ulonglong,
                                       create_unicode_buffer=ctypes.create_unicode_buffer,
                                       get_last_error=lambda: 5, WinError=lambda code=None: OSError(code or 5, "native operation failed"))
        with contextlib.ExitStack() as stack:
            for name, value in {"os": types.SimpleNamespace(name="nt", path=ntpath), "ctypes": c_types,
                                "wintypes": win_types, "kernel32": self, "user32": self,
                                "ProcessIdList": self.Ids, "WindowCallback": lambda value: value,
                                "WinEventCallback": lambda value: value,
                                "subprocess": types.SimpleNamespace(Popen=lambda *args, **kwargs: self.process, CREATE_NO_WINDOW=0),
                                "threading": types.SimpleNamespace(RLock=threading.RLock, Event=threading.Event,
                                                                   Thread=thread_factory or thread)}.items():
                stack.enter_context(unittest.mock.patch.object(title_run, name, value, create=True))
            stack.enter_context(unittest.mock.patch.object(threading, "excepthook", lambda args: self.thread_errors.append(args.exc_value)))
            try:
                yield native
            finally:
                for session in self.sessions:
                    if hasattr(session, "watch_stop"):
                        session.watch_stop.set()
                self.quit.set()
                self.query_release.set()
                for created in self.threads:
                    if created.ident is not None:
                        created.join(2)
                if self.thread_errors:
                    raise AssertionError("controlled Session worker failed") from self.thread_errors[0]

    def session(self, interactive=True):
        session = title_run.Session([r"C:\owned\title.exe"], Path.cwd(), {}, None, None, owned=r"C:\owned", interactive=interactive)
        self.sessions.append(session)
        return session


class WindowsSessionLifetimeTests(unittest.TestCase):
    def test_kill_releases_raw_handles_once_and_preserves_exit_and_images(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session()
            self.assertEqual(session.process_ids(), {10, 20})
            images = dict(session.images)
            native.live.clear()
            native.codes[10] = 7
            native.process.returncode = 0
            self.assertEqual(session.exit_code(), 7)
            session.kill()
            self.assertEqual(set(native.closed), native.allocated)
            self.assertEqual(len(native.closed), len(set(native.closed)))
            self.assertNotIn(native.process._handle, native.closed)
            self.assertEqual(session.images, images)
            self.assertEqual(session.exit_code(), 7)
            self.assertFalse(session.alive())
            session.kill()
            self.assertEqual(len(native.closed), len(set(native.closed)))

    def test_failed_job_creation_or_assignment_reaps_child_and_releases_acquired_job(self):
        for create_ok, assign_ok in ((False, True), (True, False)):
            with self.subTest(create_ok=create_ok, assign_ok=assign_ok):
                native = WindowsSessionNative()
                native.create_ok, native.assign_ok = create_ok, assign_ok
                with native.installed(), self.assertRaises(OSError):
                    native.session()
                self.assertTrue(native.process.killed)
                self.assertTrue(native.process.reaped)
                self.assertEqual(set(native.closed), native.allocated)

    def test_failed_watcher_start_reaps_child_and_releases_job(self):
        class NotStarted:
            ident = None

            def __init__(self, *args, **kwargs):
                pass

            def start(self):
                raise RuntimeError("thread start failed")

            def is_alive(self):
                return False

        native = WindowsSessionNative()
        with native.installed(thread_factory=NotStarted), self.assertRaisesRegex(RuntimeError, "thread start failed"):
            native.session(interactive=False)
        self.assertTrue(native.process.reaped)
        self.assertEqual(set(native.closed), native.allocated)

    def test_wait_failure_still_releases_every_raw_handle(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session()
            session.process_ids()
            native.process.wait_error = RuntimeError("wait failed")
            with self.assertRaisesRegex(RuntimeError, "wait failed"):
                session.kill()
            self.assertEqual(set(native.closed), native.allocated)

    def test_failed_handle_release_attempts_other_resources_and_allows_retry(self):
        for handle in (100, 1010):
            with self.subTest(handle=handle):
                native = WindowsSessionNative()
                with native.installed():
                    session = native.session()
                    session.process_ids()
                    native.close_fail.add(handle)
                    with self.assertRaises(OSError):
                        session.kill()
                    self.assertEqual(set(native.closed), native.allocated - {handle})
                    self.assertIn(handle, [session.job, *session.handles.values()])
                    native.close_fail.clear()
                    session.kill()
                    self.assertEqual(set(native.closed), native.allocated)
                    self.assertEqual(len(native.closed), len(set(native.closed)))

    def test_failed_job_termination_reaps_parent_and_releases_handles(self):
        native = WindowsSessionNative()
        native.terminate_ok = False
        with native.installed():
            session = native.session()
            session.process_ids()
            with self.assertRaises(OSError):
                session.kill()
            self.assertTrue(native.process.killed)
            self.assertTrue(native.process.reaped)
            self.assertEqual(set(native.closed), native.allocated)

    def test_watcher_loop_error_unhooks_and_releases_handles(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session(interactive=False)
            native.loop_error.set()
            native.quit.set()
            for worker in native.threads:
                worker.join(1)
            with self.assertRaisesRegex(OSError, "message pump failed"):
                session.kill()
            self.assertIsNone(native.hook)
            self.assertEqual(set(native.closed), native.allocated)

    def test_unhook_failure_is_reported_after_other_resources_are_released(self):
        native = WindowsSessionNative()
        native.unhook_ok = False
        with native.installed():
            session = native.session(interactive=False)
            with self.assertRaises(OSError):
                session.kill()
            self.assertTrue(all(not worker.is_alive() for worker in native.threads))
            self.assertEqual(set(native.closed), native.allocated)

    def test_watcher_join_is_bounded_and_later_queries_cannot_use_closed_job(self):
        class Stuck:
            ident = 123

            def __init__(self, target, args, daemon):
                self.ready = args[0]
                self.waits = []

            def start(self):
                self.ready.set()

            def join(self, timeout=None):
                self.waits.append(timeout)

            def is_alive(self):
                return True

        native = WindowsSessionNative()
        with native.installed(thread_factory=Stuck):
            session = native.session(interactive=False)
            with self.assertRaisesRegex(RuntimeError, "watcher.*stop"):
                session.kill()
            self.assertEqual(len(session.watcher.waits), 1)
            self.assertIsNotNone(session.watcher.waits[0])
            self.assertGreater(session.watcher.waits[0], 0)
            self.assertLessEqual(session.watcher.waits[0], 5)
            self.assertEqual(set(native.closed), native.allocated)
            self.assertEqual(session.job_process_ids(), set())

    def test_failed_open_process_is_not_closed_as_a_valid_handle(self):
        native = WindowsSessionNative()
        open_process = native.OpenProcess
        native.OpenProcess = lambda access, inherit, pid: 0 if pid == 20 else open_process(access, inherit, pid)
        with native.installed():
            session = native.session()
            session.process_ids()
            self.assertEqual(session.images[20], "")
            session.kill()
            self.assertEqual(set(native.closed), native.allocated)
            self.assertNotIn(0, native.closed)

    def test_graceful_close_does_not_release_job_or_process_handles(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session()
            session.close()
            self.assertEqual(native.closed, [])
            self.assertEqual(native.live, {10, 20})
            session.kill()

    def test_watcher_creates_queue_before_ready_and_unhooks_before_release(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session(interactive=False)
            self.assertTrue(native.hook_queue_ready)
            self.assertIsNotNone(native.hook)
            session.kill()
            self.assertIsNone(native.hook)
            self.assertTrue(all(not worker.is_alive() for worker in native.threads))
            self.assertEqual(set(native.closed), native.allocated)

    def test_hook_startup_failure_reaps_child_and_releases_job(self):
        native = WindowsSessionNative()
        native.hook_ok = False
        with native.installed(), self.assertRaises(OSError):
            native.session(interactive=False)
        self.assertTrue(native.process.reaped)
        self.assertEqual(set(native.closed), native.allocated)

    def test_watcher_startup_timeout_releases_job_without_an_unbounded_wait(self):
        native = WindowsSessionNative()

        def not_ready(target, args, daemon):
            args[0].wait = lambda timeout=None: False
            worker = threading.Thread(target=target, args=args, daemon=daemon)
            native.threads.append(worker)
            return worker

        with native.installed(thread_factory=not_ready), self.assertRaisesRegex(RuntimeError, "watcher.*start"):
            native.session(interactive=False)
        self.assertTrue(native.process.reaped)
        self.assertEqual(set(native.closed), native.allocated)

    def test_query_finishes_before_raw_job_is_closed(self):
        native = WindowsSessionNative()
        with native.installed():
            session = native.session()
            native.query_release.clear()
            query = threading.Thread(target=session.job_process_ids)
            query.start()
            self.assertTrue(native.query_entered.wait(1))
            cleanup = threading.Thread(target=session.kill)
            cleanup.start()
            try:
                cleanup.join(0.05)
                self.assertTrue(cleanup.is_alive())
                self.assertEqual(native.closed, [])
            finally:
                native.query_release.set()
                query.join(2)
                cleanup.join(2)
            self.assertFalse(query.is_alive())
            self.assertFalse(cleanup.is_alive())
            self.assertEqual(set(native.closed), native.allocated)


class WindowsNativeHandleCountTests(unittest.TestCase):
    @unittest.skipUnless(os.name == "nt", "native Windows process handle counts")
    def test_repeated_sessions_release_raw_windows_handles(self):
        kernel32, ctypes_api, win_types = title_run.kernel32, title_run.ctypes, title_run.wintypes
        kernel32.GetCurrentProcess.restype = win_types.HANDLE
        kernel32.GetProcessHandleCount.argtypes = (win_types.HANDLE, ctypes_api.POINTER(win_types.DWORD))

        def count():
            value = win_types.DWORD()
            self.assertTrue(kernel32.GetProcessHandleCount(kernel32.GetCurrentProcess(), ctypes_api.byref(value)))
            return value.value

        with tempfile.TemporaryDirectory(prefix="title-run-handles-") as directory:
            def cycle(interactive):
                session = title_run.Session([sys.executable, "-c", "import time; time.sleep(3)"], Path(directory), dict(os.environ),
                                            subprocess.DEVNULL, subprocess.DEVNULL, interactive=interactive)
                try:
                    self.assertTrue(wait_until(lambda: bool(session.process_ids()), seconds=2))
                finally:
                    session.kill()
                self.assertEqual(session.handles, {})
                self.assertIsNone(session.job)
                self.assertIsNotNone(session.exit_code())
                self.assertTrue(session.watcher is None or not session.watcher.is_alive())

            for interactive in (True, False):
                cycle(interactive)
            gc.collect()
            before = count()
            for _ in range(4):
                for interactive in (True, False):
                    cycle(interactive)
            gc.collect()
            self.assertLessEqual(count(), before + 4)


if __name__ == "__main__":
    unittest.main()
