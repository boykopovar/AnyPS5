import copy
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import re
import stat
import statistics


MAX_JSON_BYTES = 16 * 1024 * 1024
MAX_LOG_BYTES = 32 * 1024 * 1024
MAX_TREE_BYTES = 64 * 1024 * 1024 * 1024
MAX_TREE_FILES = 50000
MAX_FRAMES = 1000000
INTEGER_FIELDS = (
    "frames", "kept_reservations", "submits", "park_restores", "pace_timeouts",
    "stale_registrations", "failed_registrations", "threads", "released_waits",
    "of_them_from_earlier_writes", "of_them_synthesized", "unfinished", "frames_compared",
)
NUMBER_FIELDS = (
    "replay_s", "fps", "delta_mib", "pace_wait_s", "skipped_mib", "pixels_identical",
    "load_s", "journal_s",
)
HOST_ENVIRONMENT_PREFIXES = ("APS5_", "ANYPS5_", "VK_", "SDL_")
HOST_KEYS = {
    "system": {"os", "cpu", "cpu_threads", "memory_mib", "cpu_gate"},
    "sdl": {"linked", "compiled"},
    "gpu": {"device", "type", "vendor_id", "device_id", "vulkan_api", "driver",
            "driver_info", "driver_version", "driver_version_raw", "subgroup_size",
            "device_local_mib", "candidates", "present_modes", "present_mode_requested"},
    "shader_cache": {"state", "size_mib"},
}
HOST_SOURCES = {"redactions", "os", "cpu", "cpu_threads", "memory", "cpu_gate",
                "sdl_linked", "sdl_compiled", "shader_cache", "environment"}
LIMITATIONS = (
    "Replay throughput is not title FPS or a measurement of unrecorded title work.",
    "Replay times are printed to 0.001 s; per-frame each values are truncated whole milliseconds.",
    "skipped_mib is rounded to 0.1 MiB; zero does not prove that no memory was skipped.",
    "Host format 1 does not identify a physical-device UUID or the compiled dirty state.",
    "Final display-buffer comparisons do not establish every-frame or temporal correctness.",
    "Descriptive observations do not establish statistical significance or a trusted speedup.",
)


def _replay_unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _replay_reject_constant(value):
    raise ValueError(f"non-finite JSON value: {value}")


def load_replay_json(data, max_bytes=MAX_JSON_BYTES):
    if type(max_bytes) is not int or max_bytes <= 0:
        raise ValueError("JSON byte limit must be positive")
    if not isinstance(data, (str, bytes)):
        raise ValueError("JSON input must be UTF-8 text or bytes")
    try:
        raw = data.encode("utf-8") if isinstance(data, str) else data
        if len(raw) > max_bytes:
            raise ValueError(f"JSON exceeds {max_bytes} bytes")
        value = json.loads(raw.decode("utf-8"), object_pairs_hook=_replay_unique_object,
                           parse_constant=_replay_reject_constant)
        pending = [value]
        while pending:
            current = pending.pop()
            if isinstance(current, dict):
                pending.extend(current.values())
            elif isinstance(current, list):
                pending.extend(current)
            elif isinstance(current, float) and not math.isfinite(current):
                raise ValueError("non-finite JSON number")
        return value
    except (UnicodeError, json.JSONDecodeError, RecursionError, OverflowError) as error:
        raise ValueError(f"invalid replay JSON: {error}") from error


def parse_replay_host_report(stderr, expected_commit=None):
    if not isinstance(stderr, str) or len(stderr.encode("utf-8")) > MAX_LOG_BYTES:
        raise ValueError("host report log is not bounded UTF-8 text")
    if expected_commit is not None and not re.fullmatch(r"[0-9a-fA-F]{40}", expected_commit):
        raise ValueError("expected commit must be a full 40-character SHA")
    markers = list(re.finditer(r"^AnyPS5 host report \(format ([^\r\n]+)\)\r?$", stderr, re.MULTILINE))
    if not markers:
        raise ValueError("actual invocation host report is missing")
    if len(markers) > 16:
        raise ValueError("too many invocation host reports")
    reports = []
    raw_blocks = []
    for index, marker in enumerate(markers):
        if marker.group(1) != "1":
            raise ValueError("unsupported invocation host report format")
        end = markers[index + 1].start() if index + 1 < len(markers) else len(stderr)
        portion = stderr[marker.start():end]
        source_lines = portion.splitlines(keepends=True)
        lines = [line.rstrip("\r\n") for line in source_lines[1:]]
        if not lines or not lines[0].startswith("commit: "):
            raise ValueError("host report commit is missing")
        report = {"commit": lines[0][8:], "unavailable": []}
        raw_end = len(source_lines[0]) + len(source_lines[1])
        section = None
        sections = []
        for line, source_line in zip(lines[1:], source_lines[2:]):
            if line.startswith("[") and line.endswith("]"):
                name = line[1:-1]
                if name not in (*HOST_KEYS, "vulkan", "environment", "unavailable"):
                    break
                if name in sections:
                    raise ValueError(f"duplicate host report section: {name}")
                section = name
                sections.append(name)
                if name != "unavailable":
                    report[name] = {}
                raw_end += len(source_line)
                continue
            if section is None:
                raise ValueError("host report has text before its first section")
            if section == "unavailable":
                if not re.fullmatch(r"[A-Za-z0-9_]+", line) and line.partition(": ")[0] not in HOST_SOURCES:
                    break
                report[section].append(line)
            elif section == "environment" and line == "none":
                if report[section]:
                    raise ValueError("host report environment mixes none and values")
                report[section]["none"] = ""
            else:
                if ": " not in line:
                    break
                key, value = line.split(": ", 1)
                if section == "environment" and not key.startswith(HOST_ENVIRONMENT_PREFIXES):
                    break
                if section in HOST_KEYS and key not in HOST_KEYS[section]:
                    raise ValueError(f"unknown host report {section} field: {key}")
                if not re.fullmatch(r"[A-Za-z0-9_]+", key) or (not value and section != "environment") or len(value) > 16384:
                    break
                if key in report[section] or "none" in report[section]:
                    raise ValueError(f"duplicate host report field: {section}.{key}")
                report[section][key] = value
            raw_end += len(source_line)
        required_sections = {"system", "sdl", "gpu", "shader_cache", "environment"}
        if not required_sections.issubset(sections):
            raise ValueError("host report is incomplete")
        for section in ("system", "sdl"):
            if set(report[section]) != HOST_KEYS[section]:
                raise ValueError(f"host report {section} is incomplete")
        if "device" not in report["gpu"] or "state" not in report["shader_cache"]:
            raise ValueError("host report GPU or cache state is incomplete")
        if report["gpu"]["device"] != "none selected":
            if set(report["gpu"]) != HOST_KEYS["gpu"] or "vulkan" not in report:
                raise ValueError("selected GPU host report is incomplete")
        if not report["environment"]:
            raise ValueError("host report environment is incomplete")
        if report["environment"] == {"none": ""}:
            report["environment"] = {}
        if reports and report != reports[0]:
            raise ValueError("contradictory invocation host reports")
        reports.append(report)
        raw_blocks.append(portion[:raw_end])
    host = reports[0]
    host["raw"] = "".join(raw_blocks)
    host["sha256"] = hashlib.sha256(host["raw"].encode("utf-8")).hexdigest()
    host["count"] = len(reports)
    host["issues"] = []
    host["unknowns"] = [LIMITATIONS[3]]
    commit = host["commit"]
    if not re.fullmatch(r"[0-9a-fA-F]{7,40}", commit):
        host["issues"].append("compiled AGC commit is unavailable or malformed")
    elif expected_commit is not None and not expected_commit.lower().startswith(commit.lower()):
        host["issues"].append("compiled AGC commit does not match the requested arm")
    if host["gpu"]["device"] == "none selected":
        host["issues"].append("invocation selected no GPU")
    for key in ("device", "driver", "driver_info", "driver_version", "driver_version_raw"):
        if host["gpu"].get(key) in (None, "unknown", "unavailable"):
            host["unknowns"].append(f"selected GPU {key} is unavailable")
    for key, value in host["system"].items():
        if value == "unavailable":
            host["unknowns"].append(f"host system {key} is unavailable")
    if host["system"]["cpu_gate"].startswith("failed"):
        host["issues"].append("host CPU requirements failed")
    if len(reports) > 1:
        host["unknowns"].append("identical host reports were emitted more than once; emitter PID is not recorded")
    return host


def parse_replay_result(stdout, stderr="", expected_commit=None):
    result = {"native": None, "host": None, "issues": [], "unknowns": list(LIMITATIONS)}
    try:
        try:
            native = load_replay_json(stdout)
        except ValueError:
            if isinstance(stdout, bytes):
                source = stdout.decode("utf-8")
            elif isinstance(stdout, str):
                source = stdout
            else:
                raise ValueError("native replay stdout must be UTF-8 text or bytes")
            if len(source.encode("utf-8")) > MAX_JSON_BYTES:
                raise ValueError("native replay stdout exceeds its byte limit")
            candidates = [line for line in source.splitlines() if line.lstrip().startswith("{")]
            if len(candidates) != 1:
                raise ValueError("native replay stdout must contain exactly one result JSON object")
            native = load_replay_json(candidates[0])
        if not isinstance(native, dict):
            raise ValueError("native replay result must be one JSON object")
        for key in INTEGER_FIELDS:
            if type(native.get(key)) is not int or not 0 <= native[key] <= (1 << 64) - 1:
                raise ValueError(f"native replay {key} must be a nonnegative integer")
        for key in NUMBER_FIELDS:
            if type(native.get(key)) not in (int, float) or not math.isfinite(native[key]) or not 0 <= native[key] <= 1e15:
                raise ValueError(f"native replay {key} must be a finite nonnegative number")
        if not 0 <= native["pixels_identical"] <= 1:
            raise ValueError("pixels_identical must be between zero and one")
        if type(native.get("max_channel_diff")) is not int or not 0 <= native["max_channel_diff"] <= 255:
            raise ValueError("max_channel_diff must be an integer between zero and 255")
        frame_ms = native.get("frame_ms")
        if not isinstance(frame_ms, dict):
            raise ValueError("native replay frame_ms must be an object")
        for key in ("p50", "p90", "max"):
            if type(frame_ms.get(key)) not in (int, float) or not math.isfinite(frame_ms[key]) or not 0 <= frame_ms[key] <= 1e15:
                raise ValueError(f"frame_ms.{key} must be a finite nonnegative number")
        if not frame_ms["p50"] <= frame_ms["p90"] <= frame_ms["max"]:
            raise ValueError("frame_ms percentiles are out of order")
        each = frame_ms.get("each")
        if not isinstance(each, list) or len(each) > MAX_FRAMES or any(type(value) is not int or not 0 <= value <= (1 << 63) - 1 for value in each):
            raise ValueError("frame_ms.each must be bounded nonnegative whole milliseconds")
        passes = native.get("passes")
        if not isinstance(passes, list) or not 1 <= len(passes) <= 1000:
            raise ValueError("native replay passes must be a bounded nonempty array")
        for entry in passes:
            if not isinstance(entry, dict):
                raise ValueError("native replay pass must be an object")
            for key in ("frames", "pace_timeouts", "synthesized"):
                if type(entry.get(key)) is not int or not 0 <= entry[key] <= (1 << 64) - 1:
                    raise ValueError(f"pass.{key} must be a nonnegative integer")
            for key in ("replay_s", "p50"):
                if type(entry.get(key)) not in (int, float) or not math.isfinite(entry[key]) or not 0 <= entry[key] <= 1e15:
                    raise ValueError(f"pass.{key} must be a finite nonnegative number")
        if len(passes) == 1 and (passes[0]["frames"] != native["frames"] or len(each) != max(0, native["frames"] - 1)):
            raise ValueError("native replay pass/frame counts disagree")
        result["native"] = native
        for key in ("stale_registrations", "failed_registrations", "pace_timeouts", "of_them_synthesized", "unfinished"):
            if native[key]:
                result["issues"].append(f"native replay {key} is nonzero ({native[key]})")
        if len(passes) != 1:
            result["issues"].append("native replay used multiple passes instead of a fresh process with repeat 1")
        if native["frames"] < 2:
            result["issues"].append("native replay has fewer than two ready frames; frame percentiles are unavailable")
        if native["replay_s"] == 0:
            result["unknowns"].append("rounded replay duration is zero; throughput cannot be independently recomputed")
            result["issues"].append("native replay duration is not positive")
        if native["fps"] == 0:
            result["issues"].append("native replay throughput is not positive")
        if native["skipped_mib"]:
            result["issues"].append("native replay skipped memory ranges")
        if native["frames_compared"] == 0:
            result["unknowns"].append("no display reference was compared; pixel fidelity is unavailable")
            result["issues"].append("native replay has no compared display reference")
        elif native["pixels_identical"] < 1 or native["max_channel_diff"]:
            result["issues"].append("replayed display buffers differ from capture references; temporal effects may contribute")
        if native["released_waits"] or native["of_them_from_earlier_writes"]:
            result["unknowns"].append("replay released waits; the captured title's original scheduling is not reproduced")
        if any(entry["pace_timeouts"] or entry["synthesized"] for entry in passes):
            result["issues"].append("a native replay pass reported pacing timeouts or synthesized releases")
    except (ValueError, TypeError, OverflowError) as error:
        result["issues"].append(f"native replay result: {error}")
    try:
        host = parse_replay_host_report(stderr, expected_commit)
        result["host"] = host
        result["issues"].extend(host["issues"])
        result["unknowns"].extend(host["unknowns"])
    except (ValueError, TypeError, UnicodeError) as error:
        result["issues"].append(f"invocation host report: {error}")
    if isinstance(stderr, str) and "the capture is truncated" in stderr:
        result["issues"].append("native replay reported a truncated capture")
    diagnostics = (stream[:MAX_LOG_BYTES].decode("utf-8", errors="replace") if isinstance(stream, bytes)
                   else stream[:MAX_LOG_BYTES] if isinstance(stream, str) else ""
                   for stream in (stdout, stderr))
    if any(re.search(r"(?:Validation Error|VUID-[A-Za-z0-9_-]+|SYNC-HAZARD-[A-Za-z0-9_-]+)", stream) for stream in diagnostics):
        result["issues"].append("Vulkan validation diagnostics were recorded; inspect the raw stdout/stderr logs")
    result["issues"] = list(dict.fromkeys(result["issues"]))
    result["unknowns"] = list(dict.fromkeys(result["unknowns"]))
    return result


def replay_tree_manifest(root, max_bytes=MAX_TREE_BYTES, max_files=MAX_TREE_FILES, *, exclude=()):
    if type(max_bytes) is not int or max_bytes < 0 or type(max_files) is not int or max_files < 1:
        raise ValueError("tree limits must be nonnegative bytes and positive file count")
    if not isinstance(exclude, (tuple, list, set)) or any(not isinstance(path, str) or not path or PurePosixPath(path).is_absolute() or any(part in ("", ".", "..") for part in path.split("/")) or ":" in path or "\\" in path for path in exclude):
        raise ValueError("tree exclusions must be relative paths")
    supplied = Path(root)
    if ".." in supplied.parts:
        raise ValueError("tree root may not contain parent traversal")
    root = Path(os.path.abspath(supplied))
    ancestors = {}
    for path in (root, *root.parents):
        info = path.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400 or not stat.S_ISDIR(info.st_mode):
            raise ValueError(f"tree root has an alias or non-directory ancestor: {path}")
        ancestors[path] = (info.st_dev, info.st_ino, info.st_mode)
    observed = {}
    manifest = []
    for phase in (0, 1):
        pending = [root]
        current = {}
        identities = set()
        portable_names = set()
        total = 0
        files = 0
        while pending:
            directory = pending.pop()
            info = directory.lstat()
            if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400 or not stat.S_ISDIR(info.st_mode):
                raise ValueError(f"tree contains an aliased directory: {directory}")
            relative = directory.relative_to(root).as_posix()
            current[relative] = (info.st_dev, info.st_ino, info.st_mode, info.st_size, info.st_mtime_ns, info.st_ctime_ns)
            identity = (info.st_dev, info.st_ino)
            if identity in identities:
                raise ValueError("tree contains duplicate directory identities")
            identities.add(identity)
            entries = []
            with os.scandir(directory) as scan:
                for entry in scan:
                    entries.append(entry)
                    if len(current) + len(entries) + len(pending) > max_files * 2 + 1000:
                        raise ValueError("tree exceeds its entry limit")
            for entry in sorted(entries, key=lambda item: item.name):
                path = Path(entry.path)
                relative = path.relative_to(root).as_posix()
                if any(ord(character) < 32 for character in relative) or ":" in relative or "\\" in relative:
                    raise ValueError("tree contains an unsupported relative filename")
                if relative.casefold() in portable_names:
                    raise ValueError("tree contains aliased relative filenames")
                portable_names.add(relative.casefold())
                info = path.lstat()
                if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
                    raise ValueError(f"tree contains a symbolic link or reparse point: {relative}")
                if stat.S_ISDIR(info.st_mode):
                    pending.append(path)
                    continue
                if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
                    raise ValueError(f"tree contains a nonregular or hardlinked file: {relative}")
                identity = (info.st_dev, info.st_ino)
                if identity in identities:
                    raise ValueError("tree contains duplicate file identities")
                identities.add(identity)
                files += 1
                total += info.st_size
                if files > max_files or total > max_bytes:
                    raise ValueError("tree exceeds its file or byte limit")
                mode = info.st_mode & ~0o111 if os.name == "nt" else info.st_mode
                created = getattr(info, "st_birthtime_ns", None) if os.name == "nt" else info.st_ctime_ns
                signature = (info.st_dev, info.st_ino, mode, info.st_size, info.st_mtime_ns, created)
                current[relative] = signature
                if phase == 0:
                    digest = hashlib.sha256()
                    flags = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0)
                    descriptor = os.open(path, flags)
                    with os.fdopen(descriptor, "rb") as stream:
                        before = os.fstat(stream.fileno())
                        before_mode = before.st_mode & ~0o111 if os.name == "nt" else before.st_mode
                        before_created = getattr(before, "st_birthtime_ns", None) if os.name == "nt" else before.st_ctime_ns
                        if (before.st_dev, before.st_ino, before_mode, before.st_size, before.st_mtime_ns, before_created) != signature:
                            raise ValueError(f"tree file changed before hashing: {relative}")
                        size = 0
                        while block := stream.read(1024 * 1024):
                            size += len(block)
                            if size > info.st_size or size > max_bytes:
                                raise ValueError(f"tree file grew during hashing: {relative}")
                            digest.update(block)
                        after = os.fstat(stream.fileno())
                    after_mode = after.st_mode & ~0o111 if os.name == "nt" else after.st_mode
                    after_created = getattr(after, "st_birthtime_ns", None) if os.name == "nt" else after.st_ctime_ns
                    if size != info.st_size or after.st_ctime_ns != before.st_ctime_ns or (after.st_dev, after.st_ino, after_mode, after.st_size, after.st_mtime_ns, after_created) != signature:
                        raise ValueError(f"tree file changed during hashing: {relative}")
                    manifest.append({"path": relative, "bytes": size, "sha256": digest.hexdigest()})
        if phase == 0:
            observed = current
        elif current != observed:
            raise ValueError("tree changed while its manifest was created")
    for path, expected in ancestors.items():
        info = path.lstat()
        if getattr(info, "st_file_attributes", 0) & 0x400 or (info.st_dev, info.st_ino, info.st_mode) != expected:
            raise ValueError("tree ancestor changed while its manifest was created")
    return sorted((row for row in manifest if row["path"] not in exclude), key=lambda row: row["path"])


def verify_replay_bundle(root, manifest, *, exclude=(), max_bytes=MAX_TREE_BYTES, max_files=MAX_TREE_FILES):
    if not isinstance(manifest, list) or len(manifest) > max_files:
        raise ValueError("bundle manifest must be a bounded array")
    expected = {}
    for row in manifest:
        if not isinstance(row, dict) or set(row) != {"path", "bytes", "sha256"}:
            raise ValueError("bundle manifest entry is malformed")
        path = row["path"]
        if not isinstance(path, str) or not path or PurePosixPath(path).is_absolute() or any(part in ("", ".", "..") for part in path.split("/")) or ":" in path or "\\" in path:
            raise ValueError("bundle manifest path is not a relative regular path")
        if path in expected or type(row["bytes"]) is not int or row["bytes"] < 0 or not isinstance(row["sha256"], str) or not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]):
            raise ValueError("bundle manifest has duplicate paths or invalid file evidence")
        expected[path] = row
    if not isinstance(exclude, (tuple, list, set)) or any(not isinstance(path, str) or not path or PurePosixPath(path).is_absolute() or any(part in ("", ".", "..") for part in path.split("/")) or ":" in path or "\\" in path for path in exclude):
        raise ValueError("bundle exclusions must be relative paths")
    actual = replay_tree_manifest(root, max_bytes=max_bytes, max_files=max_files)
    actual = {row["path"]: row for row in actual if row["path"] not in exclude}
    expected = {path: row for path, row in expected.items() if path not in exclude}
    if actual != expected:
        raise ValueError("bundle files differ from their pinned manifest")
    return True


def replay_comparison_report(report, markdown=False):
    if not isinstance(report, dict) or report.get("format") != "aps5-frame-replay-comparison/1" or not isinstance(report.get("runs"), list):
        raise ValueError("expected aps5-frame-replay-comparison/1 with runs")
    result = copy.deepcopy(report)
    for name in ("issues", "unknowns", "notes"):
        if not isinstance(result.get(name, []), list) or any(not isinstance(item, str) for item in result.get(name, [])):
            raise ValueError(f"comparison {name} must be an array of text")
    for row in result["runs"]:
        if not isinstance(row, dict) or row.get("arm") not in ("baseline", "candidate") or row.get("phase") not in ("warmup", "trial"):
            raise ValueError("comparison run arm or phase is malformed")
        if type(row.get("iteration")) is not int or row["iteration"] < 0:
            raise ValueError("comparison run iteration must be nonnegative")
        if row.get("returncode") is not None and type(row["returncode"]) is not int:
            raise ValueError("comparison returncode must be an integer or null")
        if any(type(row.get(key)) is not bool for key in ("timed_out", "log_limit")):
            raise ValueError("comparison run outcome flags must be booleans")
        assessment = row.get("assessment")
        if not isinstance(assessment, dict) or not isinstance(assessment.get("issues"), list) or not isinstance(assessment.get("unknowns"), list):
            raise ValueError("comparison run assessment is malformed")
        if any(not isinstance(item, str) for key in ("issues", "unknowns") for item in assessment[key]):
            raise ValueError("comparison assessment diagnostics must be text")
    observations = {}
    healthy = {}
    for arm in ("baseline", "candidate"):
        runs = [row for row in result["runs"] if row.get("arm") == arm and row.get("phase") == "trial"]
        eligible = [row for row in runs if row.get("returncode") == 0 and not row.get("timed_out") and not row.get("log_limit") and isinstance(row.get("assessment"), dict) and not row["assessment"].get("issues") and isinstance(row["assessment"].get("native"), dict)]
        healthy[arm] = eligible
        metrics = {}
        for key in ("replay_s", "fps", "p50_ms"):
            values = []
            for row in eligible:
                native = row["assessment"]["native"]
                value = native.get("frame_ms", {}).get("p50") if key == "p50_ms" else native.get(key)
                if type(value) in (int, float) and math.isfinite(value) and value > 0:
                    values.append(value)
            metrics[key] = {"n": len(values), "median": statistics.median(values) if values else None,
                            "min": min(values) if values else None, "max": max(values) if values else None}
        observations[arm] = {"attempted_trials": len(runs), "healthy_trials": len(eligible), "metrics": metrics}
    inputs = result.get("inputs", {})
    recipe = result.get("recipe", {})
    arms = recipe.get("arms", {}) if isinstance(recipe, dict) else {}
    settings = [arms.get(arm) for arm in ("baseline", "candidate")] if isinstance(arms, dict) else [None, None]
    same_settings = all(isinstance(arm, dict) and isinstance(arm.get("environment"), dict) for arm in settings)
    if same_settings:
        same_settings = all(settings[0].get(key) == settings[1].get(key) for key in ("environment", "replay", "library_dirs", "commit"))
    self_baseline = bool(inputs.get("baseline")) and inputs.get("baseline") == inputs.get("candidate") and same_settings
    result["observations"] = observations
    result["self_baseline"] = self_baseline
    pairs = []
    trials = [row for row in result["runs"] if row["phase"] == "trial"]
    for iteration in sorted({row["iteration"] for row in trials}):
        pair = {"iteration": iteration, "issues": [], "metrics": {}}
        members = {}
        for arm in ("baseline", "candidate"):
            found = [row for row in trials if row["iteration"] == iteration and row["arm"] == arm]
            if len(found) != 1 or found[0] not in healthy[arm]:
                pair["issues"].append(f"{arm} does not have exactly one healthy completed trial")
            else:
                members[arm] = found[0]
        if result.get("complete") is not True or result.get("issues"):
            pair["issues"].append("comparison is incomplete or has global evidence issues")
        if len(members) == 2:
            contexts = []
            for member in members.values():
                host = member["assessment"].get("host")
                if not isinstance(host, dict) or not all(isinstance(host.get(key), dict) for key in ("system", "gpu")):
                    pair["issues"].append("observed invocation host context is unavailable")
                    continue
                contexts.append({section: {key: host[section].get(key) for key in keys} for section, keys in (
                    ("system", ("os", "cpu", "cpu_threads", "memory_mib")),
                    ("gpu", ("device", "vendor_id", "device_id", "driver", "driver_info", "driver_version_raw", "vulkan_api")))})
            if len(contexts) == 2 and contexts[0] != contexts[1]:
                pair["issues"].append("observed invocation host contexts differ")
            for key in ("replay_s", "fps", "p50_ms"):
                values = []
                for arm in ("baseline", "candidate"):
                    native = members[arm]["assessment"]["native"]
                    value = native.get("frame_ms", {}).get("p50") if key == "p50_ms" else native.get(key)
                    values.append(value if type(value) in (int, float) and math.isfinite(value) and value > 0 else None)
                before, after = values
                percentage = (after / before - 1) * 100 if not pair["issues"] and None not in values else None
                if percentage is not None and not math.isfinite(percentage):
                    percentage = None
                pair["metrics"][key] = {"baseline": before, "candidate": after,
                                         "change": after - before if not pair["issues"] and None not in values else None,
                                         "change_pct": percentage}
        pairs.append(pair)
    result["pairs"] = pairs
    paired = {}
    for key in ("replay_s", "fps", "p50_ms"):
        values = [pair["metrics"][key]["change_pct"] for pair in pairs if key in pair["metrics"] and pair["metrics"][key]["change_pct"] is not None]
        paired[key] = {"n": len(values), "median_change_pct": statistics.median(values) if values else None,
                       "min_change_pct": min(values) if values else None, "max_change_pct": max(values) if values else None}
    result["paired_observations"] = paired
    result["notes"] = list(dict.fromkeys([*result.get("notes", []), *LIMITATIONS,
                                         "Only healthy completed trial records contribute; omitted and failed attempts remain in runs.",
                                         "A relative change is unavailable when rounded metrics are nonpositive, missing or cannot be represented finitely.",
                                         "Self-baselines characterize observed repeatability, not a software speedup." if self_baseline else "Different artifact manifests do not by themselves prove different source behavior."]))
    unknown_runs = {}
    for row in result["runs"]:
        label = f"{row['arm']} {row['phase']} {row['iteration']}"
        for item in row["assessment"]["unknowns"]:
            if item not in result["notes"]:
                unknown_runs.setdefault(item, []).append(label)
    result["unknowns"] = list(dict.fromkeys([*result.get("unknowns", []),
                                            *[f"{item} (runs: {', '.join(labels)})" for item, labels in unknown_runs.items()]]))
    if not markdown:
        return result
    escape = lambda value: str(value).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("|", "\\|").replace("`", "\\`").replace("\r", " ").replace("\n", " ")
    lines = ["# Frame replay observations", "", f"Self-baseline: {'yes' if self_baseline else 'no'}.", "",
             "| Arm | Completed healthy / attempted trials | Replay seconds median [min, max] | Replay throughput median [min, max] | Ready-frame p50 ms median [min, max] |",
             "| --- | --- | --- | --- | --- |"]
    for arm, observation in observations.items():
        cells = []
        for metric in observation["metrics"].values():
            cells.append("unavailable" if metric["median"] is None else f"{metric['median']:g} [{metric['min']:g}, {metric['max']:g}]")
        lines.append(f"| {arm} | {observation['healthy_trials']} / {observation['attempted_trials']} | " + " | ".join(cells) + " |")
    lines.extend(["", "| Run | Arm | Phase | Exit | Evidence issues |", "| --- | --- | --- | --- | --- |"])
    for index, row in enumerate(result["runs"], 1):
        issues = row.get("assessment", {}).get("issues", [])
        lines.append("| " + " | ".join(escape(value) for value in (index, row.get("arm", "unavailable"), row.get("phase", "unavailable"), row.get("returncode", "unavailable"), "; ".join(issues) or "none recorded")) + " |")
    lines.extend(["", "| Trial pair | Replay seconds change | Throughput change | Ready-frame p50 change | Evidence issues |",
                  "| --- | --- | --- | --- | --- |"])
    for pair in pairs:
        cells = []
        for key in ("replay_s", "fps", "p50_ms"):
            value = pair["metrics"].get(key, {}).get("change_pct")
            cells.append("unavailable" if value is None else f"{value:+.3g}%")
        lines.append(f"| {pair['iteration']} | " + " | ".join(cells) + " | " + escape("; ".join(pair["issues"]) or "none recorded") + " |")
    for label in ("issues", "unknowns", "notes"):
        lines.extend(["", f"{label.capitalize()}:", ""])
        lines.extend(f"- {escape(value)}" for value in result.get(label, []))
    return "\n".join(lines) + "\n"
