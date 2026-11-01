import argparse
import contextlib
import ctypes
import ctypes.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


RESOLUTIONS = {"1080p": (1920, 1080, 8), "1440p": (2560, 1440, 6), "2160p": (3840, 2160, 0)}
SETTINGS = Path("_sd/GOWSOSSAVE999/GoW_SoS999.dat")
KEY = '"resolutionIndex":'.encode("utf-32le")
SDL_INIT_VIDEO = 0x20


class DisplayMode(ctypes.Structure):
    _fields_ = [("format", ctypes.c_uint32), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("refresh_rate", ctypes.c_int), ("driverdata", ctypes.c_void_p)]


def desktop_size():
    display = os.environ.get("ANYPS5_DISPLAY_INDEX", "0")
    if not re.fullmatch(r"[0-9]+", display):
        raise ValueError("ANYPS5_DISPLAY_INDEX must be a nonnegative display index")
    library = ctypes.util.find_library("SDL2") or "SDL2.dll"
    try:
        sdl = ctypes.CDLL(library)
    except OSError as error:
        raise ValueError("Auto resolution needs the SDL2 shared library; install it or use --resolution") from error
    sdl.SDL_InitSubSystem.argtypes = [ctypes.c_uint32]
    sdl.SDL_InitSubSystem.restype = ctypes.c_int
    sdl.SDL_QuitSubSystem.argtypes = [ctypes.c_uint32]
    sdl.SDL_QuitSubSystem.restype = None
    sdl.SDL_GetError.argtypes = []
    sdl.SDL_GetError.restype = ctypes.c_char_p
    sdl.SDL_GetNumVideoDisplays.argtypes = []
    sdl.SDL_GetNumVideoDisplays.restype = ctypes.c_int
    sdl.SDL_GetDesktopDisplayMode.argtypes = [ctypes.c_int, ctypes.POINTER(DisplayMode)]
    sdl.SDL_GetDesktopDisplayMode.restype = ctypes.c_int
    sdl.SDL_SetHint.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    sdl.SDL_SetHint.restype = ctypes.c_int
    sdl.SDL_SetHint(b"SDL_WINDOWS_DPI_AWARENESS", b"permonitorv2")
    if sdl.SDL_InitSubSystem(SDL_INIT_VIDEO) != 0:
        raise ValueError(f"Cannot inspect displays: {sdl.SDL_GetError().decode(errors='replace')}")
    try:
        if int(display) >= sdl.SDL_GetNumVideoDisplays():
            raise ValueError(f"Display {display} is not connected")
        mode = DisplayMode()
        if sdl.SDL_GetDesktopDisplayMode(int(display), ctypes.byref(mode)) != 0:
            raise ValueError(f"Cannot read display mode: {sdl.SDL_GetError().decode(errors='replace')}")
        if mode.w <= 0 or mode.h <= 0:
            raise ValueError("Display dimensions must be positive")
        return mode.w, mode.h
    finally:
        sdl.SDL_QuitSubSystem(SDL_INIT_VIDEO)


def automatic_resolution(width, height):
    if width <= 0 or height <= 0:
        raise ValueError("Display dimensions must be positive")
    return next((name for name, (w, h, _) in reversed(RESOLUTIONS.items())
                 if w <= width and h <= height), "1080p")


def resolution_offset(data):
    if len(data) % 4 or data.count(KEY) != 1:
        raise ValueError("Unrecognized settings: expected one aligned resolutionIndex")
    at = data.index(KEY) + len(KEY)
    if at % 4:
        raise ValueError("Unaligned resolutionIndex in settings")
    while data[at:at + 4] in (b" \0\0\0", b"\t\0\0\0", b"\r\0\0\0", b"\n\0\0\0"):
        at += 4
    digit = data[at:at + 4]
    if len(digit) != 4 or digit[1:] != b"\0\0\0" or not ord("0") <= digit[0] <= ord("8"):
        raise ValueError("Unsupported resolutionIndex in settings")
    end = at + 4
    while data[end:end + 4] in (b" \0\0\0", b"\t\0\0\0", b"\r\0\0\0", b"\n\0\0\0"):
        end += 4
    if data[end:end + 4] not in (b",\0\0\0", b"}\0\0\0"):
        raise ValueError("Invalid resolutionIndex terminator in settings")
    return at


def write_resolution(path, data, at, index):
    changed = data[:at] + str(index).encode("utf-32le") + data[at + 4:]
    if changed == data:
        return
    backup = path.with_name(path.name + ".resolution.bak")
    try:
        with backup.open("xb") as output:
            output.write(data)
    except FileExistsError:
        pass
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".", delete=False) as output:
            temporary = Path(output.name)
            output.write(changed)
            output.flush()
            os.fsync(output.fileno())
        shutil.copymode(path, temporary)
        if path.read_bytes() != data:
            raise ValueError("Settings changed during preparation; stop the game and retry")
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


@contextlib.contextmanager
def launch_lock(path):
    with path.open("a+b") as stream:
        if os.name == "nt":
            import msvcrt
            try:
                stream.seek(0)
                if not stream.read(1):
                    stream.write(b"\0")
                    stream.flush()
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise ValueError("Another launcher is already using these settings") from error
        else:
            import fcntl
            try:
                fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError as error:
                raise ValueError("Another launcher is already using these settings") from error
        yield


def main(argv=None):
    parser = argparse.ArgumentParser(description="Launch Sons of Sparta with monitor-matched internal resolution.")
    parser.add_argument("--resolution", choices=["auto", *RESOLUTIONS], default="auto")
    parser.add_argument("--dry-run", action="store_true", help="show the choice without changing settings or launching")
    parser.add_argument("executable", type=Path, help="converted executable; its directory is the game's working directory")
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    try:
        executable = args.executable.absolute()
        if not executable.is_file():
            raise ValueError(f"Executable not found: {executable}")
        path = executable.parent / SETTINGS
        if not path.is_file():
            raise ValueError(f"Settings not found: {path}. Run the game once to create its settings, then exit and retry.")
        data = path.read_bytes()
        at = resolution_offset(data)
        resolution = args.resolution
        if resolution == "auto":
            width, height = desktop_size()
            resolution = automatic_resolution(width, height)
            print(f"Display {os.environ.get('ANYPS5_DISPLAY_INDEX', '0')}: {width}x{height}", flush=True)
        width, height, index = RESOLUTIONS[resolution]
        print(f"Sons of Sparta internal resolution: {width}x{height} (resolutionIndex={index})", flush=True)
        if args.dry_run:
            return 0
        with launch_lock(path.with_name(path.name + ".launch.lock")):
            data = path.read_bytes()
            at = resolution_offset(data)
            write_resolution(path, data, at, index)
            return subprocess.call([str(executable), *args.arguments], cwd=executable.parent)
    except (OSError, ValueError) as error:
        print(f"launch_sparta: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
