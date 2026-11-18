import ctypes
import os
import shlex
import subprocess
import sys


class InteractiveConsole:
    def __init__(self):
        self.stdout = None
        self.stderr = None
        self.provenance = None
        self._streams = []
        self._allocated = False
        self._active = False
        self._kernel = None
        self._standard_handles = ()

    def __enter__(self):
        if self._active:
            raise RuntimeError("interactive console is already active")
        self._active = True
        try:
            if os.name != "nt":
                if not all(stream is not None and os.isatty(stream.fileno())
                           for stream in (sys.stdout, sys.stderr)):
                    raise OSError("stdout and stderr must both be terminal file descriptors")
                self.stdout, self.stderr = sys.stdout, sys.stderr
                self.provenance = {"kind": "posix-terminal", "verified": True}
                return self
            import msvcrt
            from ctypes import wintypes
            kernel = self._kernel = ctypes.WinDLL("kernel32", use_last_error=True)
            user = ctypes.WinDLL("user32", use_last_error=True)
            kernel.GetConsoleWindow.argtypes = ()
            kernel.GetConsoleWindow.restype = wintypes.HWND
            kernel.AllocConsole.argtypes = ()
            kernel.AllocConsole.restype = wintypes.BOOL
            kernel.FreeConsole.argtypes = ()
            kernel.FreeConsole.restype = wintypes.BOOL
            kernel.GetStdHandle.argtypes = (wintypes.DWORD,)
            kernel.GetStdHandle.restype = wintypes.HANDLE
            kernel.SetStdHandle.argtypes = (wintypes.DWORD, wintypes.HANDLE)
            kernel.SetStdHandle.restype = wintypes.BOOL
            kernel.GetConsoleMode.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
            kernel.GetConsoleMode.restype = wintypes.BOOL
            user.ShowWindow.argtypes = (wintypes.HWND, ctypes.c_int)
            user.ShowWindow.restype = wintypes.BOOL
            user.IsWindowVisible.argtypes = (wintypes.HWND,)
            user.IsWindowVisible.restype = wintypes.BOOL
            window = kernel.GetConsoleWindow()
            if not window:
                self._standard_handles = tuple((index & 0xffffffff, kernel.GetStdHandle(index & 0xffffffff))
                                               for index in (-10, -11, -12))
                if not kernel.AllocConsole():
                    raise ctypes.WinError(ctypes.get_last_error())
                self._allocated = True
                for index, handle in self._standard_handles:
                    if not kernel.SetStdHandle(index, handle):
                        raise OSError("could not restore the parent's standard handles")
                window = kernel.GetConsoleWindow()
                if not window:
                    raise OSError("the allocated console has no console window")
                user.ShowWindow(window, 5)
            for _ in range(2):
                stream = open("CONOUT$", "wb", buffering=0)
                self._streams.append(stream)
                mode = wintypes.DWORD()
                if not kernel.GetConsoleMode(msvcrt.get_osfhandle(stream.fileno()), ctypes.byref(mode)):
                    raise OSError("output is not a Windows console handle")
            self.stdout, self.stderr = self._streams
            self.provenance = {"kind": "windows-owned-console" if self._allocated else "windows-existing-console",
                               "verified": True, "window_visible": bool(user.IsWindowVisible(window))}
            return self
        except (OSError, AttributeError, ValueError) as error:
            self.close()
            command = [sys.executable, *sys.argv]
            text = subprocess.list2cmdline(command) if os.name == "nt" else shlex.join(command)
            raise RuntimeError("cannot establish unredirected console output: " + str(error) +
                               "; run this command in an interactive terminal: " + text) from error
        except BaseException:
            self.close()
            raise

    def __exit__(self, exception_type, exception, traceback):
        self.close()
        return False

    def close(self):
        error = None
        for stream in self._streams:
            try:
                stream.close()
            except OSError as failure:
                error = error or failure
        self._streams = []
        if self._allocated:
            if not self._kernel.FreeConsole():
                error = error or OSError("could not release the owned console")
            self._allocated = False
            for index, handle in self._standard_handles:
                if not self._kernel.SetStdHandle(index, handle):
                    error = error or OSError("could not restore the parent's standard handles")
        self._standard_handles = ()
        self._active = False
        self.stdout, self.stderr = None, None
        if error is not None:
            raise error
