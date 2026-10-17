import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import unwrap_self
from package_release import windows_runtime
OUTCOMES = ("relink-failed", "crashed", "exited", "stalled", "blank", "rendering")
TITLE_FIELDS = {"name": str, "relink": list, "fixed_base": str, "executables": list, "seconds": int, "env": dict, "input": list}
FIXED_BASE_FLAGS = {"windows": "--windows-fixed-base", "linux": "--linux-fixed-base"}


def load_titles(path):
    titles = json.loads(Path(path).read_text(encoding="utf-8"))
    for title_id, entry in titles.items():
        if not re.fullmatch(r"[A-Z]{4}[0-9]{5}", title_id):
            raise ValueError(f"{path}: {title_id} is not a title ID")
        for key, value in entry.items():
            if key not in TITLE_FIELDS:
                raise ValueError(f"{path}: {title_id}: unknown field {key}")
            if not isinstance(value, TITLE_FIELDS[key]):
                raise ValueError(f"{path}: {title_id}: {key} must be {TITLE_FIELDS[key].__name__}")
    return titles


def relink_args(entry, platform):
    args = ["--windows"] if platform == "windows" else []
    args += entry.get("relink", [])
    if "fixed_base" in entry:
        if platform not in FIXED_BASE_FLAGS:
            raise ValueError(f"fixed_base is not supported on {platform}")
        args += [FIXED_BASE_FLAGS[platform], entry["fixed_base"]]
    for executable in entry.get("executables", []):
        args += ["--executable", executable]
    return args


ERROR = re.compile(r"what\(\):|FATAL|not implemented|unhandled exception")
WARNINGS = ("destroyed with GPU results pending", "guest memory changed while GPU results were pending")
STALL_SECONDS = 20


def first_error(text):
    return next((line.strip() for line in text.splitlines() if ERROR.search(line)), "")


def warning_counts(text):
    return {warning: text.count(warning) for warning in WARNINGS}


def read_summary(path):
    lines = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()] if path.is_file() else []
    presents = sum(line.get("presents", 0) for line in lines)
    seconds = sum(line.get("window", {}).get("ms", 0) for line in lines) / 1000
    last = lines[-1] if lines else {}
    return {"presents": presents, "fps": round(presents / seconds, 1) if seconds else 0.0,
            "gpu": last.get("gpu", ""), "driver": last.get("driver", ""), "os": last.get("os", "")}


def frame_is_uniform(path, tolerance=8):
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    channels = struct.unpack_from("<H", data, 28)[0] // 8
    stride = (width * channels + 3) & ~3
    first = data[offset:offset + 3]
    for row in range(abs(height)):
        pixels = data[offset + row * stride:offset + row * stride + width * channels]
        for index in range(0, len(pixels), channels):
            if max(abs(pixels[index + channel] - first[channel]) for channel in range(3)) > tolerance:
                return False
    return True


def classify(relinked, exit_code, summary_age, last_frame_uniform):
    if not relinked:
        return "relink-failed"
    if exit_code is not None:
        return "crashed" if exit_code != 0 else "exited"
    if summary_age is None or summary_age > STALL_SECONDS:
        return "stalled"
    if last_frame_uniform:
        return "blank"
    return "rendering"


def cell(runs, cache):
    outcomes = [entry["outcome"] for entry in runs if entry["cache"] == cache]
    if not outcomes:
        return "-"
    if len(outcomes) == 1:
        return outcomes[0]
    return ", ".join(f"{outcome} {outcomes.count(outcome)}/{len(outcomes)}" for outcome in sorted(set(outcomes), key=OUTCOMES.index))


def worst(runs):
    return min((entry["outcome"] for entry in runs), key=OUTCOMES.index)


def compare(summary, previous):
    regressions, improvements = [], []
    for title_id, title in summary["titles"].items():
        if title_id not in previous["titles"]:
            continue
        before, now = worst(previous["titles"][title_id]["runs"]), worst(title["runs"])
        if OUTCOMES.index(now) < OUTCOMES.index(before):
            regressions.append((title_id, before, now))
        elif OUTCOMES.index(now) > OUTCOMES.index(before):
            improvements.append((title_id, before, now))
    return regressions, improvements


def render_markdown(summary, previous=None):
    build, machine = summary["build"], summary["machine"]
    header = f"AnyPS5 `{build['commit']}`" + (" (dirty)" if build["dirty"] else "") + (f", {build['note']}" if build["note"] else "")
    header += f". {machine['os']}, {machine['gpu']}, driver {machine['driver']}, CPU {machine['cpu']}."
    header += f" {summary['runs']} runs per title, {summary['seconds']} s each."
    lines = [header, ""]
    columns = ["Title", "ID", "Version", "Cold", "Warm", "FPS", "First error"] + (["Before"] if previous else [])
    if previous:
        regressions, improvements = compare(summary, previous)
        for label, changes in (("Regressions", regressions), ("Improvements", improvements)):
            if changes:
                names = ", ".join(f"{summary['titles'][title_id]['name']} ({title_id}) {before} -> {now}" for title_id, before, now in changes)
                lines += [f"{label}: {names}", ""]
        missing = sorted(title_id for title_id in previous["titles"] if title_id not in summary["titles"])
        if missing:
            names = ", ".join(f"{previous['titles'][title_id]['name']} ({title_id})" for title_id in missing)
            lines += [f"Missing: {names}", ""]
    lines +=["| " + " | ".join(columns) + " |", "|" + "---|" * len(columns)]
    for title_id, title in sorted(summary["titles"].items()):
        runs = title["runs"]
        error = next((entry["first_error"] for entry in runs if entry["outcome"] != "rendering"), "")
        row = [title["name"], title_id, title["version"], cell(runs, "cold"), cell(runs, "warm"), f"{runs[-1]['fps']:.1f}",
               error[:120].replace("|", "\\|")]
        if previous:
            old = previous["titles"].get(title_id)
            row.append(worst(old["runs"]) if old else "new")
        lines.append("| " + " | ".join(row) + " |")
    return "\n".join(lines + [""])


def discover(directories):
    found = {}
    for directory in directories:
        for param in sorted(Path(directory).glob("**/sce_sys/param.json")):
            package = param.parent.parent
            if not (package / "eboot.bin").is_file():
                continue
            data = json.loads(param.read_text(encoding="utf-8-sig"))
            title_id = data["titleId"]
            if title_id in found:
                raise ValueError(f"{title_id} found more than once: {found[title_id]['package']} and {package}")
            localized = data.get("localizedParameters", {})
            name = localized.get(localized.get("defaultLanguage", "en-US"), {}).get("titleName", title_id)
            found[title_id] = {"package": package, "version": data.get("contentVersion", ""), "name": name}
    return found


def unwrap_or_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.read_bytes()[:4] == b"\x7fELF":
        shutil.copyfile(source, target)
    else:
        unwrap_self.unwrap(source, target)


def link_file(source, target):
    try:
        os.link(source, target)
    except OSError:
        shutil.copy2(source, target)


def link_tree(source, target, skip):
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if relative.parts[0] in skip:
            continue
        if path.is_dir():
            (target / relative).mkdir(parents=True, exist_ok=True)
        else:
            (target / relative).parent.mkdir(parents=True, exist_ok=True)
            link_file(path, target / relative)


def prepare(title_id, found, entry, platform, build, work):
    package = found["package"]
    shutil.rmtree(work, ignore_errors=True)
    source = work / "input"
    (source / "sce_module").mkdir(parents=True)
    unwrap_or_copy(package / "eboot.bin", source / "eboot.elf")
    for module in sorted((package / "sce_module").glob("*.prx")):
        unwrap_or_copy(module, source / "sce_module" / module.name)
    for executable in entry.get("executables", []):
        unwrap_or_copy(package / executable, source / executable)
    executable = work / (title_id + (".exe" if platform == "windows" else ""))
    relinker = build / "core/relinker" / ("relinker.exe" if platform == "windows" else "relinker")
    with (work / "relink.log").open("w", encoding="utf-8") as log:
        relinked = subprocess.run([str(relinker), *relink_args(entry, platform), str(source / "eboot.elf"), str(executable)],
                                  stdout=log, stderr=subprocess.STDOUT)
    if relinked.returncode != 0:
        return None
    link_tree(package, work / "app0", {"sce_module"})
    libs = work / "libs"
    libs.mkdir()
    for library in sorted((build / "core/libs/libs").glob("*.prx")):
        link_file(library, libs / library.name)
    if platform == "windows":
        runtime = windows_runtime(build)
        for name in ("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll"):
            link_file(runtime / name, libs / name)
    return executable


if os.name == "nt":
    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = (ctypes.c_void_p, wintypes.LPCWSTR)
    kernel32.AssignProcessToJobObject.argtypes = (wintypes.HANDLE, wintypes.HANDLE)
    kernel32.TerminateJobObject.argtypes = (wintypes.HANDLE, wintypes.UINT)
    kernel32.QueryInformationJobObject.argtypes = (wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p)
    WindowCallback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = (WindowCallback, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user32.IsWindowVisible.argtypes = (wintypes.HWND,)
    user32.SetWindowPos.argtypes = (wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT)
    user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)

    class ProcessIdList(ctypes.Structure):
        _fields_ = [("assigned", wintypes.DWORD), ("listed", wintypes.DWORD), ("ids", ctypes.c_size_t * 256)]


class Session:
    def __init__(self, command, cwd, env, stdout, stderr):
        if os.name == "nt":
            self.process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
            self.job = kernel32.CreateJobObjectW(None, None)
            kernel32.AssignProcessToJobObject(self.job, int(self.process._handle))
        else:
            self.process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stdout, stderr=stderr, start_new_session=True)

    def process_ids(self):
        listing = ProcessIdList()
        kernel32.QueryInformationJobObject(self.job, 3, ctypes.byref(listing), ctypes.sizeof(listing), None)
        return set(listing.ids[:listing.listed])

    def windows(self):
        ids, found = self.process_ids(), []

        def visit(window, _):
            owner = wintypes.DWORD()
            user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
            if owner.value in ids and user32.IsWindowVisible(window):
                found.append(window)
            return True

        user32.EnumWindows(WindowCallback(visit), 0)
        return found

    def alive(self):
        self.process.poll()
        if os.name == "nt":
            return bool(self.process_ids())
        try:
            os.killpg(self.process.pid, 0)
            return True
        except ProcessLookupError:
            return False

    def exit_code(self):
        return self.process.poll()

    def lower_windows(self):
        if os.name == "nt":
            for window in self.windows():
                user32.SetWindowPos(window, wintypes.HWND(1), 0, 0, 0, 0, 0x13)

    def close(self):
        if os.name == "nt":
            for window in self.windows():
                user32.PostMessageW(window, 0x10, 0, 0)
        else:
            try:
                os.killpg(self.process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

    def kill(self):
        if os.name == "nt":
            kernel32.TerminateJobObject(self.job, 1)
        else:
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        self.process.wait()
