import argparse
import datetime
import json
import os
import re
import shutil
import signal
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import pad_input
import pad_timeline
import unwrap_self
from package_release import windows_runtime
OUTCOMES = ("relink-failed", "crashed", "exited", "stalled", "frame-unreadable", "blank", "rendering")
TITLE_ID = re.compile(r"[A-Z]{4}[0-9]{5}")
TITLE_FIELDS = {"relink": list, "fixed_base": str, "executables": list, "seconds": int, "env": dict, "pad": dict, "files": dict}
FILE_ROOTS = ("app0", "data", "download0")
FIXED_BASE_FLAGS = {"windows": "--windows-fixed-base", "linux": "--linux-fixed-base"}
PAD_FIELDS = {"events", "after_s", "repeat", "gap_ms"}
PAD_AFTER_SECONDS = 5
REPORT_FILE = "anyps5-report.json"
SHADER_COUNTS = ("compiled", "failed", "cacheHits")
UNSUPPORTED_KEYS = ("encoding", "opcode", "opcodeValue", "modifiers")


def pad_text(pad, sequence):
    return pad_timeline.compile_timeline({"version": 1, "events": pad["events"]}, sequence, pad.get("repeat", 1), pad.get("gap_ms", 200))


def check_pad(pad):
    if "events" not in pad or not set(pad) <= PAD_FIELDS:
        raise ValueError(f"pad must hold events and may hold {', '.join(sorted(PAD_FIELDS - {'events'}))}")
    pad_timeline.integer(pad.get("after_s", PAD_AFTER_SECONDS), "pad.after_s", 0, 3600)
    pad_text(pad, 1)


def inside(path, roots=()):
    pure = PurePosixPath(path)
    if "\\" in path or ":" in path or pure.is_absolute() or not pure.parts or ".." in pure.parts:
        return False
    return not roots or (len(pure.parts) > 1 and pure.parts[0] in roots)


def check_files(files):
    for destination, source in files.items():
        if not inside(destination, FILE_ROOTS):
            raise ValueError(f"files: {destination} must be a relative path under {', '.join(FILE_ROOTS)}")
        if not isinstance(source, dict) or len(source) != 1 or next(iter(source)) not in ("text", "content") or not isinstance(next(iter(source.values())), str):
            raise ValueError(f"files: {destination} must be {{\"text\": ...}} or {{\"content\": ...}}")
        if "content" in source and not inside(source["content"]):
            raise ValueError(f"files: {destination}: content must be a relative path inside --content")


def stage_files(files, work, content):
    missing = []
    for destination, source in files.items():
        target = work / destination
        target.parent.mkdir(parents=True, exist_ok=True)
        target.unlink(missing_ok=True)
        if "text" in source:
            target.write_text(source["text"], encoding="utf-8", newline="\n")
        elif content and (content / source["content"]).is_file():
            shutil.copyfile(content / source["content"], target)
        else:
            missing.append(source["content"])
    return missing


def load_titles(path):
    titles = json.loads(Path(path).read_text(encoding="utf-8"))
    for title_id, entry in titles.items():
        if not TITLE_ID.fullmatch(title_id):
            raise ValueError(f"{path}: {title_id} is not a title ID")
        for key, value in entry.items():
            if key not in TITLE_FIELDS:
                raise ValueError(f"{path}: {title_id}: unknown field {key}")
            if key == "seconds":
                if type(value) is not int or value < 1:
                    raise ValueError(f"{path}: {title_id}: seconds must be a positive integer")
            elif not isinstance(value, TITLE_FIELDS[key]):
                raise ValueError(f"{path}: {title_id}: {key} must be {TITLE_FIELDS[key].__name__}")
            if key in ("pad", "files"):
                try:
                    check_pad(value) if key == "pad" else check_files(value)
                except ValueError as error:
                    raise ValueError(f"{path}: {title_id}: {error}") from None
                continue
            items = value.values() if isinstance(value, dict) else value if isinstance(value, list) else []
            if not all(isinstance(item, str) for item in items):
                raise ValueError(f"{path}: {title_id}: {key} must hold strings")
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


ERROR = re.compile(r"what\(\):|FAIL:|FATAL|not implemented|unhandled exception|DEVICE_LOST|has not reached submission|Failed to load module:")
WARNINGS = ("destroyed with GPU results pending", "guest memory changed while GPU results were pending")
STALL_SECONDS = 20


def first_error(text):
    lines = [line.strip() for line in text.splitlines()]
    notifications = [line for line in lines if line.startswith("[notification] ")]
    return next((line for line in lines if ERROR.search(line)), notifications[-1] if notifications else "")


def warning_counts(text):
    return {warning: text.count(warning) for warning in WARNINGS}


def read_summary(path):
    lines = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if path.is_file() else []:
        try:
            decoded = json.loads(line)
        except ValueError:
            continue
        if isinstance(decoded, dict):
            lines.append(decoded)
    presents = sum(line.get("presents", 0) for line in lines)
    seconds = sum(line.get("window", {}).get("ms", 0) for line in lines) / 1000
    last = lines[-1] if lines else {}
    return {"presents": presents, "fps": round(presents / seconds, 1) if seconds else 0.0,
            "gpu": last.get("gpu", ""), "driver": last.get("driver", ""), "os": last.get("os", "")}


def section(document, name):
    value = document.get(name)
    return value if isinstance(value, dict) else {}


def read_report(path):
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if not isinstance(document, dict) or document.get("format") != "anyps5-report":
        return None
    coverage, run, shaders = section(document, "coverage"), section(document, "run"), section(document, "shaders")
    missing, truncated = section(document, "missing"), section(document, "truncated")
    functions, unsupported = missing.get("functions"), shaders.get("unsupported")
    functions = [entry for entry in functions if isinstance(entry, dict)] if isinstance(functions, list) else functions
    unsupported = [entry for entry in unsupported if isinstance(entry, dict)] if isinstance(unsupported, list) else unsupported
    return {"averageFps": run.get("averageFps"),
            "shaders": {name: shaders.get(name) for name in SHADER_COUNTS} if coverage.get("shaders") else None,
            "missing": functions if coverage.get("missing") and isinstance(functions, list) else None,
            "unsupported": unsupported if coverage.get("unsupported") and isinstance(unsupported, list) else None,
            "truncated": {"functions": truncated.get("functions", 0), "unsupported": truncated.get("unsupported", 0)}}


def take_report(work, run_dir):
    source = work / REPORT_FILE
    if not source.is_file():
        return None
    shutil.move(str(source), str(run_dir / REPORT_FILE))
    return read_report(run_dir / REPORT_FILE)


def frame_is_uniform(path, tolerance=8):
    data = path.read_bytes()
    if data[:2] != b"BM":
        return None
    try:
        offset = struct.unpack_from("<I", data, 10)[0]
        header_size = struct.unpack_from("<I", data, 14)[0]
        width, height = struct.unpack_from("<ii", data, 18)
        planes = struct.unpack_from("<H", data, 26)[0]
        bits = struct.unpack_from("<H", data, 28)[0]
        compression = struct.unpack_from("<I", data, 30)[0]
    except struct.error:
        return None
    if (header_size < 40 or offset < 14 + header_size or width <= 0 or height == 0 or
            planes != 1 or compression != 0 or bits not in (24, 32)):
        return None
    channels = bits // 8
    stride = (width * channels + 3) & ~3
    if len(data) < offset + stride * abs(height) - (stride - width * channels):
        return None
    first = data[offset:offset + 3]
    for row in range(abs(height)):
        pixels = data[offset + row * stride:offset + row * stride + width * channels]
        for index in range(0, len(pixels), channels):
            if max(abs(pixels[index + channel] - first[channel]) for channel in range(3)) > tolerance:
                return False
    return True


def classify(relinked, exit_code, summary_age, framed, last_frame_uniform):
    if not relinked:
        return "relink-failed"
    if exit_code is not None:
        return "crashed" if exit_code != 0 else "exited"
    if summary_age is None or summary_age > STALL_SECONDS or not framed:
        return "stalled"
    if last_frame_uniform is None:
        return "frame-unreadable"
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


def last_run(title):
    return title["runs"][-1]


def pair(before, now):
    return {"before": before, "now": now}


def fps_of(run):
    return None if run.get("outcome") == "relink-failed" else run.get("fps")


def report_field(run, name):
    return (run.get("report") or {}).get(name)


def shader_count(run, name):
    return (report_field(run, "shaders") or {}).get(name)


def distinct(entries, keys):
    return None if entries is None else len({json.dumps([entry.get(name) for name in keys]) for entry in entries})


def tally(entries, keys, count):
    totals = {}
    for entry in entries:
        key = json.dumps([entry.get(name) for name in keys])
        value = entry.get(count)
        totals[key] = totals.get(key, 0) + (value if isinstance(value, int) else 0)
    return totals


def diff_entries(before, now, keys, count):
    if before is None or now is None:
        return None
    left, right = tally(before, keys, count), tally(now, keys, count)

    def fields(key, **values):
        return {**dict(zip(keys, json.loads(key))), **values}

    return {"onlyBefore": [fields(key, **{count: left[key]}) for key in sorted(left) if key not in right],
            "onlyNow": [fields(key, **{count: right[key]}) for key in sorted(right) if key not in left],
            "both": [fields(key, before=left[key], now=right[key]) for key in sorted(left) if key in right]}


def missing_functions(run):
    return report_field(run, "missing")


def truncated_functions(run):
    return (report_field(run, "truncated") or {}).get("functions")


def unsupported_instructions(run):
    return report_field(run, "unsupported")


def truncated_unsupported(run):
    return (report_field(run, "truncated") or {}).get("unsupported")


def compare_runs(summary, previous):
    compared = {}
    for title_id in sorted(set(summary["titles"]) & set(previous["titles"])):
        before, now = last_run(previous["titles"][title_id]), last_run(summary["titles"][title_id])
        compared[title_id] = {"name": summary["titles"][title_id]["name"], "fps": pair(fps_of(before), fps_of(now)),
                              "averageFps": pair(report_field(before, "averageFps"), report_field(now, "averageFps")),
                              "shaders": {name: pair(shader_count(before, name), shader_count(now, name)) for name in SHADER_COUNTS},
                              "missingFunctions": diff_entries(missing_functions(before), missing_functions(now), ("library", "name"), "calls"),
                              "missingFunctionsCount": pair(distinct(missing_functions(before), ("library", "name")), distinct(missing_functions(now), ("library", "name"))),
                              "missingFunctionsTruncated": pair(truncated_functions(before), truncated_functions(now)),
                              "unsupportedInstructions": diff_entries(unsupported_instructions(before), unsupported_instructions(now), UNSUPPORTED_KEYS, "count"),
                              "unsupportedInstructionsCount": pair(distinct(unsupported_instructions(before), UNSUPPORTED_KEYS),
                                                                   distinct(unsupported_instructions(now), UNSUPPORTED_KEYS)),
                              "unsupportedInstructionsTruncated": pair(truncated_unsupported(before), truncated_unsupported(now))}
    return compared


def display(value):
    if value is None:
        return "not recorded"
    return f"{value:.1f}" if isinstance(value, float) else str(value)


def label(entry):
    if "name" in entry:
        return f"{entry['library']}/{entry['name']}" if entry.get("library") else entry["name"]
    return f"{entry.get('opcode') or 'unnamed'} ({entry.get('encoding') or 'unknown encoding'}, opcode value {entry.get('opcodeValue')})"


def names(entries):
    return ", ".join(label(entry) for entry in entries)


def render_compare(compared):
    columns = ["Title", "ID", "Measure", "Before", "Now"]
    lines = ["| " + " | ".join(columns) + " |", "|" + "---|" * len(columns)]
    details = []
    for title_id, title in compared.items():
        measures = [("FPS", title["fps"]), ("Average FPS (report)", title["averageFps"]),
                    ("Shaders compiled", title["shaders"]["compiled"]), ("Shaders failed", title["shaders"]["failed"]),
                    ("Shader cache hits", title["shaders"]["cacheHits"])]
        for label, key in (("Missing functions", "missingFunctions"), ("Unsupported instructions", "unsupportedInstructions")):
            diff = title[key]
            measures.append((label, title[key + "Count"]))
            if diff and (diff["onlyBefore"] or diff["onlyNow"] or diff["both"]):
                details.append(f"{title['name']} ({title_id}), {label.lower()}: only before: {names(diff['onlyBefore']) or 'none'}; "
                               f"only now: {names(diff['onlyNow']) or 'none'}; in both: {len(diff['both'])}")
        for label, values in measures:
            lines.append("| " + " | ".join([title["name"], title_id, label, display(values["before"]), display(values["now"])]) + " |")
    return lines + [""] + details


def compare_json(summary, previous):
    return json.dumps({"format": "title-run-compare", "version": 1, "titles": compare_runs(summary, previous)}, indent=2)


def render_markdown(summary, previous=None):
    build, machine = summary["build"], summary["machine"]
    header = f"AnyPS5 `{build['commit']}`" + (" (dirty)" if build["dirty"] else "") + (f", {build['note']}" if build["note"] else "")
    parts = [machine["os"], machine["gpu"], machine["driver"] and f"driver {machine['driver']}", machine["cpu"] and f"CPU {machine['cpu']}"]
    known = ", ".join(part for part in parts if part)
    header += f". {known}." if known else "."
    header += f" {summary['runs']} run{'' if summary['runs'] == 1 else 's'} per title, {summary['seconds']} s each."
    lines = [header, ""]
    lacking = [f"{title['name']} ({title_id}) {', '.join(title['missing_content'])}" for title_id, title in sorted(summary["titles"].items()) if title.get("missing_content")]
    if lacking:
        lines += [f"Ran without content: {'; '.join(lacking)}", ""]
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
    lines += ["| " + " | ".join(columns) + " |", "|" + "---|" * len(columns)]
    for title_id, title in sorted(summary["titles"].items()):
        runs = title["runs"]
        error = next((entry["first_error"] for entry in runs if entry["outcome"] != "rendering" and entry["first_error"]), "")
        row = [title["name"], title_id, title["version"], cell(runs, "cold"), cell(runs, "warm"), f"{runs[-1]['fps']:.1f}",
               error[:120].replace("|", "\\|")]
        if previous:
            old = previous["titles"].get(title_id)
            row.append(worst(old["runs"]) if old else "new")
        lines.append("| " + " | ".join(row) + " |")
    if previous and (compared := compare_runs(summary, previous)):
        lines += ["", *render_compare(compared)]
    return "\n".join(lines + [""])


def discover(directories, wanted=()):
    found = {}
    for directory in directories:
        for param in sorted(Path(directory).glob("**/sce_sys/param.json")):
            package = param.parent.parent
            if not (package / "eboot.bin").is_file():
                continue
            data = json.loads(param.read_text(encoding="utf-8-sig"))
            title_id = data.get("titleId")
            if not isinstance(title_id, str):
                raise ValueError(f"{param}: no titleId")
            if not TITLE_ID.fullmatch(title_id):
                raise ValueError(f"{param}: titleId {title_id!r} is not a title ID")
            if wanted and title_id not in wanted:
                continue
            if title_id in found:
                raise ValueError(f"{title_id} found more than once: {found[title_id]['package']} and {package}")
            localized = data.get("localizedParameters", {})
            name = localized.get(localized.get("defaultLanguage", "en-US"), {}).get("titleName", title_id)
            found[title_id] = {"package": package, "version": data.get("contentVersion", ""), "name": name}
    return found


def unwrap_or_copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as stream:
        magic = stream.read(4)
    if magic == b"\x7fELF":
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


def lock_work(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("a+b")
    try:
        if os.name == "nt":
            import msvcrt
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
        else:
            import fcntl
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        handle.close()
        return None
    return handle


def module_dirs(entry):
    relink = entry.get("relink", [])
    return [relink[index + 1] for index in range(len(relink) - 1) if relink[index] == "--module-dir"]


MODULE_MAGICS = (b"\x7fELF", b"\x4f\x15\x3d\x1d", b"\x54\x14\xf5\xee")


def is_module(path):
    with path.open("rb") as file:
        return file.read(4) in MODULE_MAGICS


def stage_inputs(package, entry, source):
    (source / "sce_module").mkdir(parents=True)
    unwrap_or_copy(package / "eboot.bin", source / "eboot.elf")
    executables = [path for path in (package / "eboot.bin", *(package / name for name in entry.get("executables", []))) if path.is_file()]
    for directory in ["sce_module", *module_dirs(entry)]:
        if not (package / directory).is_dir():
            continue
        (source / directory).mkdir(parents=True, exist_ok=True)
        for module in sorted(path for path in (package / directory).iterdir()
                             if path.is_file() and is_module(path) and not any(path.samefile(executable) for executable in executables)):
            unwrap_or_copy(module, source / directory / module.name)
    for executable in entry.get("executables", []):
        unwrap_or_copy(package / executable, source / executable)


def prepare(title_id, found, entry, platform, build, work):
    package = found["package"]
    shutil.rmtree(work, ignore_errors=True)
    source = work / "input"
    stage_inputs(package, entry, source)
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
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    kernel32.QueryFullProcessImageNameW.argtypes = (wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD))
    kernel32.GetProcessTimes.argtypes = (wintypes.HANDLE, *(ctypes.POINTER(ctypes.c_ulonglong),) * 4)
    kernel32.QueryInformationJobObject.argtypes = (wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p)
    WindowCallback = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = (WindowCallback, wintypes.LPARAM)
    user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
    user32.IsWindowVisible.argtypes = (wintypes.HWND,)
    user32.SetWindowPos.argtypes = (wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT)
    user32.GetSystemMetrics.argtypes = (ctypes.c_int,)
    user32.PostMessageW.argtypes = (wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)
    user32.GetWindowRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
    user32.GetAncestor.restype = wintypes.HWND
    user32.GetAncestor.argtypes = (wintypes.HWND, wintypes.UINT)
    WinEventCallback = ctypes.WINFUNCTYPE(None, wintypes.HANDLE, wintypes.DWORD, wintypes.HWND, wintypes.LONG, wintypes.LONG, wintypes.DWORD, wintypes.DWORD)
    user32.SetWinEventHook.restype = wintypes.HANDLE
    user32.SetWinEventHook.argtypes = (wintypes.DWORD, wintypes.DWORD, wintypes.HMODULE, WinEventCallback, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD)
    user32.UnhookWinEvent.argtypes = (wintypes.HANDLE,)
    user32.GetMessageW.argtypes = (ctypes.POINTER(wintypes.MSG), wintypes.HWND, wintypes.UINT, wintypes.UINT)
    user32.PostThreadMessageW.argtypes = (wintypes.DWORD, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM)

    def send_to_back(window):
        user32.SetWindowPos(window, wintypes.HWND(1), 0, 0, 0, 0, 0x4013)

    class ProcessIdList(ctypes.Structure):
        _fields_ = [("assigned", wintypes.DWORD), ("listed", wintypes.DWORD), ("ids", ctypes.c_size_t * 256)]


class Session:
    def __init__(self, command, cwd, env, stdout, stderr, owned=None):
        self.owned = os.path.normcase(os.path.realpath(owned or Path(command[0]).parent))
        if os.name == "nt":
            self.process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stdout, stderr=stderr, creationflags=subprocess.CREATE_NO_WINDOW)
            self.handles, self.images = {}, {}
            self.job = kernel32.CreateJobObjectW(None, None)
            if not self.job or (not kernel32.AssignProcessToJobObject(self.job, int(self.process._handle)) and self.process.poll() is None):
                error = ctypes.get_last_error()
                self.process.kill()
                raise ctypes.WinError(error)
            self.placed = set()
            ready = threading.Event()
            threading.Thread(target=self.watch_windows, args=(ready,), daemon=True).start()
            ready.wait()
        else:
            self.process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stdout, stderr=stderr, start_new_session=True)

    def job_process_ids(self):
        listing = ProcessIdList()
        kernel32.QueryInformationJobObject(self.job, 3, ctypes.byref(listing), ctypes.sizeof(listing), None)
        return set(listing.ids[:listing.listed])

    def process_ids(self):
        ids = self.job_process_ids()
        for pid in ids - self.handles.keys():
            self.handles[pid] = kernel32.OpenProcess(0x1000, False, pid)
            self.images[pid] = self.image_path(self.handles[pid]) if self.handles[pid] else ""
        return ids

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

    def watch_windows(self, ready):
        def created(hook, event, window, kind, child, thread, at):
            owner = wintypes.DWORD()
            if (kind == 0 and child == 0 and user32.GetAncestor(window, 2) == window
                    and user32.GetWindowThreadProcessId(window, ctypes.byref(owner)) and owner.value in self.job_process_ids()):
                send_to_back(window)

        callback = WinEventCallback(created)
        hook = user32.SetWinEventHook(0x8000, 0x8002, None, callback, 0, 0, 2)
        self.watcher = kernel32.GetCurrentThreadId()
        ready.set()
        message = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(message), None, 0, 0) > 0:
            pass
        user32.UnhookWinEvent(hook)

    def alive(self):
        self.process.poll()
        if os.name == "nt":
            return bool(self.process_ids())
        try:
            os.killpg(self.process.pid, 0)
            return True
        except PermissionError:
            return True
        except ProcessLookupError:
            return False

    def image_path(self, handle):
        size = wintypes.DWORD(32768)
        buffer = ctypes.create_unicode_buffer(size.value)
        return os.path.normcase(os.path.realpath(buffer.value)) if kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)) else ""

    def exit_code(self):
        if os.name == "nt" and not self.process_ids():
            last = None
            for pid, handle in self.handles.items():
                code, times = wintypes.DWORD(), [ctypes.c_ulonglong() for _ in range(4)]
                if (handle and self.images[pid].startswith(self.owned) and kernel32.GetExitCodeProcess(handle, ctypes.byref(code)) and code.value != 259
                        and kernel32.GetProcessTimes(handle, *map(ctypes.byref, times)) and (last is None or times[0].value >= last[0])):
                    last = (times[0].value, code.value)
            if last:
                return last[1]
        return self.process.poll()

    def send_new_windows_to_back(self):
        if os.name == "nt":
            for window in set(self.windows()) - self.placed:
                send_to_back(window)
                self.placed.add(window)

    def close(self):
        if os.name == "nt":
            for window in self.windows():
                user32.PostMessageW(window, 0x10, 0, 0)
        else:
            try:
                os.killpg(self.process.pid, signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
                pass

    def kill(self):
        if os.name == "nt":
            kernel32.TerminateJobObject(self.job, 1)
            user32.PostThreadMessageW(self.watcher, 0x12, 0, 0)
        else:
            try:
                os.killpg(self.process.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass
        self.process.wait()


CLOSE_SECONDS = 15
FRAME_ENV = {"APS5_DUMP_FRAMES": "100000", "APS5_DUMP_FRAMES_EVERY": "30", "APS5_DUMP_SCALE": "8"}


def collect_frames(work, destination):
    frames = sorted(work.glob("frame_*.bmp"), key=lambda path: int(path.stem.split("_")[1]))
    keep = {frames[0], frames[len(frames) // 2], frames[-1]} if frames else set()
    destination.mkdir(parents=True, exist_ok=True)
    kept = []
    for frame in frames:
        if frame in keep:
            shutil.move(frame, destination / frame.name)
            kept.append(frame.name)
        else:
            frame.unlink()
    return kept


def run_title(executable, work, run_dir, seconds, extra_env, pad=None):
    run_dir.mkdir(parents=True)
    (work / REPORT_FILE).unlink(missing_ok=True)
    inherited = {key: value for key, value in os.environ.items() if key not in ("ANYPS5_SHADER_CACHE_DIR", "ANYPS5_NO_SHADER_CACHE", "APS5_PAD_INPUT_FILE")}
    env = {**inherited, "SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN": "1",
           "APS5_TRACE_SUMMARY_EVERY": "10", "APS5_TRACE_DIR": str(run_dir), **FRAME_ENV,
           "MESA_SHADER_CACHE_DIR": str(work / "driver-cache"), "__GL_SHADER_DISK_CACHE_PATH": str(work / "driver-cache"), **extra_env}
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    pad_file, queued = work / "pad-input.txt", None
    if pad:
        pad_file.unlink(missing_ok=True)
        env["APS5_PAD_INPUT_FILE"] = str(pad_file)
    started = time.monotonic()
    with (run_dir / "stdout.log").open("wb") as stdout, (run_dir / "stderr.log").open("wb") as stderr:
        session = Session([str(executable)], work, env, stdout, stderr, owned=work)
        try:
            while session.alive() and time.monotonic() - started < seconds:
                if pad and queued is None and time.monotonic() - started >= pad.get("after_s", PAD_AFTER_SECONDS):
                    pad_timeline.queue_timeline(pad_file, pad_text(pad, pad_input.next_sequence(pad_file)))
                    queued = round(time.monotonic() - started, 1)
                session.send_new_windows_to_back()
                time.sleep(0.1 if time.monotonic() - started < 10 else 0.5)
            elapsed = round(time.monotonic() - started, 1)
            alive, ended = session.alive(), time.time()
            exit_code = None if alive else session.exit_code()
            if alive:
                session.close()
                deadline = time.monotonic() + CLOSE_SECONDS
                while session.alive() and time.monotonic() < deadline:
                    time.sleep(0.5)
        finally:
            session.kill()
    text = "".join((run_dir / name).read_text(encoding="utf-8", errors="replace") for name in ("stdout.log", "stderr.log"))
    summary_path = run_dir / "aps5-summary.jsonl"
    summary = read_summary(summary_path)
    report = take_report(work, run_dir)
    frames = collect_frames(work, run_dir / "frames")
    uniform = frame_is_uniform(run_dir / "frames" / frames[-1]) if frames else None
    age = ended - summary_path.stat().st_mtime if summary_path.is_file() else None
    cpu = re.search(r"host CPU (.+?): has", text)
    return {"outcome": classify(True, exit_code, age, bool(frames), uniform), "exit_code": exit_code, "seconds": elapsed,
            "presents": summary["presents"], "fps": summary["fps"], "first_error": first_error(text),
            "warnings": warning_counts(text), "frames": frames, "report": report, "gpu": summary["gpu"], "driver": summary["driver"],
            "os": summary["os"], "cpu": cpu.group(1) if cpu else "", **({"pad_queued_s": queued} if pad else {})}


def source_root(build):
    cache = Path(build) / "CMakeCache.txt"
    if cache.is_file():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CMAKE_HOME_DIRECTORY:"):
                return Path(line.split("=", 1)[1])
    return ROOT


def git(root, *args):
    try:
        done = subprocess.run(["git", "-C", str(root), *args], capture_output=True, text=True)
    except OSError as error:
        raise SystemExit(f"cannot run git {' '.join(args)} in {root}: {error}")
    if done.returncode:
        raise SystemExit(f"git {' '.join(args)} failed in {root} (exit {done.returncode}): {done.stderr.strip()}")
    return done.stdout.strip()


def positive(text):
    try:
        value = int(text)
    except ValueError:
        value = 0
    if value < 1:
        raise argparse.ArgumentTypeError(f"{text!r} is not a positive integer")
    return value


def last_line(text):
    return next((line.strip() for line in reversed(text.splitlines()) if line.strip()), "")


def write_summary(output, summary):
    (output / "summary.json").write_text(json.dumps(summary, indent=2, default=str), encoding="utf-8")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Run titles repeatably and report each run's outcome.")
    parser.add_argument("ids", nargs="*", help="title IDs to run (default: every title found)")
    parser.add_argument("--games", action="append", type=Path, required=True, help="directory searched for extracted packages")
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--runs", type=positive, default=2)
    parser.add_argument("--seconds", type=positive, help="run length for every title (default: the title's, else 60)")
    parser.add_argument("--note", default="", help="free text for the report, e.g. 'main + #6814'")
    parser.add_argument("--compare", type=Path, help="summary.json of an earlier run; the comparison uses the last run of each title in both")
    parser.add_argument("--json", action="store_true", help="with --compare, print the comparison as JSON instead of the table")
    parser.add_argument("--titles", type=Path, default=ROOT / "tools/titles.json")
    parser.add_argument("--content", type=Path, help="directory holding the files that titles.json names with \"content\"")
    args = parser.parse_args(argv)
    args.out = args.out or args.build / "title-runs"
    if args.json and not args.compare:
        parser.error("--json needs --compare")
    previous = json.loads(args.compare.read_text(encoding="utf-8")) if args.compare else None
    platform = "windows" if os.name == "nt" else "macos" if sys.platform == "darwin" else "linux"
    titles = load_titles(args.titles)
    found = discover(args.games, args.ids)
    ids = args.ids or sorted(found)
    missing = [title_id for title_id in ids if title_id not in found]
    if missing:
        raise SystemExit(f"not found under --games: {', '.join(missing)}")
    locks = {title_id: lock_work(args.out.resolve() / "work" / f"{title_id}.lock") for title_id in ids}
    busy = [title_id for title_id, handle in locks.items() if handle is None]
    if busy:
        raise SystemExit(f"already running in {args.out.resolve() / 'work'}: {', '.join(busy)}")
    if previous and args.ids:
        previous["titles"] = {title_id: title for title_id, title in previous["titles"].items() if title_id in ids}
    source = source_root(args.build)
    commit = git(source, "rev-parse", "--short", "HEAD")
    output = args.out.resolve() / f"{commit}-{datetime.datetime.now():%Y%m%d-%H%M%S}"
    summary = {"build": {"commit": commit, "dirty": bool(git(source, "status", "--porcelain", "--untracked-files=no")), "note": args.note},
               "machine": {"os": "", "gpu": "", "driver": "", "cpu": ""}, "runs": args.runs, "seconds": args.seconds or 60,
               "driver_cache": "isolated" if platform == "linux" else "shared (not isolated)", "titles": {}}
    for title_id in ids:
        entry = titles.get(title_id, {})
        seconds = args.seconds or entry.get("seconds", 60)
        work = args.out.resolve() / "work" / title_id
        missing = []
        try:
            executable = prepare(title_id, found[title_id], entry, platform, args.build, work)
            missing = stage_files(entry.get("files", {}), work, args.content) if executable else []
        except (unwrap_self.Refused, OSError, RuntimeError, TypeError, ValueError) as error:
            work.mkdir(parents=True, exist_ok=True)
            (work / "relink.log").write_text(f"{type(error).__name__}: {error}\n", encoding="utf-8")
            executable = None
        if missing:
            print(f"{title_id}: --content lacks {', '.join(missing)}; running without", flush=True)
        (output / title_id).mkdir(parents=True, exist_ok=True)
        shutil.copyfile(work / "relink.log", output / title_id / "relink.log")
        runs = []
        for number in range(1, args.runs + 1):
            if executable is None:
                log = (work / "relink.log").read_text(encoding="utf-8", errors="replace")
                result = {"outcome": "relink-failed", "exit_code": None, "seconds": 0, "presents": 0, "fps": 0.0,
                          "first_error": first_error(log) or last_line(log),
                          "warnings": {}, "frames": [], "report": None}
            else:
                result = run_title(executable, work, output / title_id / f"run{number}", seconds, entry.get("env", {}), entry.get("pad"))
                for key in ("os", "gpu", "driver", "cpu"):
                    value = result.pop(key)
                    summary["machine"][key] = summary["machine"][key] or value
            runs.append({"run": number, "cache": "cold" if number == 1 else "warm", **result})
            print(f"{title_id} run{number}: {result['outcome']}", flush=True)
        summary["titles"][title_id] = {"name": found[title_id]["name"], "version": found[title_id]["version"], "seconds": seconds, "missing_content": missing, "runs": runs}
        write_summary(output, summary)
    write_summary(output, summary)
    report = render_markdown(summary, previous)
    (output / "summary.md").write_text(report, encoding="utf-8")
    if args.json:
        print(compare_json(summary, previous))
    else:
        print(report)
    print(f"written to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
