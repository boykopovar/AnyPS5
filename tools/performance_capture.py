import hashlib
import json
import math
import os
import re
import stat
import zlib
from collections.abc import Mapping
from pathlib import Path

import performance_compare
import trace_digest


UINT64_MAX = 2**64 - 1
MAX_SUMMARY_BYTES = 1024 * 1024
MAX_TRACE_BYTES = 64 * 1024 * 1024
MAX_TRACE_JSON_BYTES = 256 * 1024 * 1024
MAX_EVENTS = 2_000_000
INSTRUMENTATION_PREFIXES = (
    "APS5_TRACE_", "APS5_PROFILE_", "APS5_DUMP_", "APS5_SAMPLE_", "APS5_CAPTURE_",
    "APS5_FRAME_CAPTURE", "APS5_GROUP_CAPTURE_",
)
INSTRUMENTATION_NAMES = {
    "APS5_PIPELINE_STATS", "APS5_LOG_STATS", "APS5_SHARED_PROFILE_COUNTERS",
    "APS5_LOG_SYNC", "APS5_LOG_TIMESTAMPS", "APS5_SAVEDATA_TRACE",
    "APS5_WRITE_EVIDENCE_SAMPLE", "APS5_HANG_DUMP_SECS", "APS5_PIPELINE_DRAIN_DUMPS",
    "APS5_PIPELINE_DRAIN_SAMPLE_DUMPS", "APS5_SYNC_SAMPLE_DUMPS",
    "APS5_VIDEOOUT_TELEMETRY_DIR", "APS5_VIDEOOUT_FLIP_TIMING",
    "ANYPS5_LOG_FILE",
}


class CapturePending(Exception):
    pass


def parse_window(value):
    if not isinstance(value, str) or re.fullmatch(r"[0-9]+:[0-9]+", value) is None:
        raise ValueError("window must be <start>:<length> in whole decimal numbers")
    parts = [part.lstrip("0") or "0" for part in value.split(":")]
    if any(len(part) > 20 for part in parts):
        raise ValueError("window start and length must fit uint64")
    start, length = map(int, parts)
    if start > UINT64_MAX or not 0 < length <= UINT64_MAX:
        raise ValueError("window start must fit uint64 and length must be between 1 and uint64 max")
    return start, length


def instrumentation_name(name):
    folded = name.upper()
    return folded.startswith(INSTRUMENTATION_PREFIXES) or folded in INSTRUMENTATION_NAMES


def clean_environment(base_env, extra_env, trace_dir, window_kind, window):
    if window_kind not in ("frames", "seconds"):
        raise ValueError("window kind must be frames or seconds")
    if not isinstance(window, (tuple, list)) or len(window) != 2 or any(type(value) is not int for value in window):
        raise ValueError("window must contain its parsed start and length")
    start, length = parse_window(f"{window[0]}:{window[1]}")
    child, removed = {}, set()
    for mapping in (base_env, extra_env):
        if not isinstance(mapping, Mapping):
            raise ValueError("environment must map names to strings")
        for name, value in mapping.items():
            if (not isinstance(name, str) or not name or "=" in name or "\x00" in name or
                    not isinstance(value, str) or "\x00" in value):
                raise ValueError("environment must contain valid string names and values")
            if instrumentation_name(name) or name.upper() == "APS5_PAD_RECORD":
                removed.add(name)
                continue
            if os.name == "nt":
                for previous in list(child):
                    if previous.upper() == name.upper():
                        del child[previous]
            child[name] = value
    child["APS5_TRACE_" + window_kind.upper()] = f"{start}:{length}"
    child["APS5_TRACE_DIR"] = str(Path(trace_dir).resolve())
    return child, sorted(removed)


def strict_json(raw, label):
    value = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=performance_compare.unique_json_object)
    pending = [value]
    while pending:
        node = pending.pop()
        if isinstance(node, dict):
            pending.extend(node.values())
        elif isinstance(node, list):
            pending.extend(node)
        elif type(node) is float and not math.isfinite(node):
            raise ValueError(label + " contains a non-finite number")
    return value


def read_regular(path, limit):
    info = path.lstat()
    if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or
            getattr(info, "st_file_attributes", 0) & 0x400):
        raise ValueError(path.name + " must be a regular file without links or reparse points")
    if info.st_size > limit:
        raise ValueError(path.name + " exceeds its byte limit")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if (opened.st_dev, opened.st_ino, opened.st_size, opened.st_mtime_ns) != (
                info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns):
            raise CapturePending(path.name + " changed while opening")
        raw = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    if len(raw) > limit:
        raise ValueError(path.name + " exceeds its byte limit")
    if (len(raw) != info.st_size or (after.st_size, after.st_mtime_ns) != (info.st_size, info.st_mtime_ns)):
        raise CapturePending(path.name + " is still being written")
    return raw, {"path": path.name, "bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}


def validate_summary(summary, window_kind, window, title_id, content_version):
    if not isinstance(summary, dict):
        raise ValueError("native summary must be an object")
    performance_compare.validate(summary)
    if not summary["commit"].strip() or len(summary["commit"]) > 1024:
        raise ValueError("native summary commit must be nonempty")
    for name in ("title_id", "title", "content_version"):
        if name in summary and not isinstance(summary[name], str):
            raise ValueError("native summary " + name + " must be a string")
    if title_id is not None and summary.get("title_id") != title_id:
        raise ValueError("native summary title identity differs from the requested title")
    if content_version and summary.get("content_version") != content_version:
        raise ValueError("native summary content version differs from the requested content")
    start, length = window
    if summary["window"]["spec"] != f"{window_kind} {start}:{length}":
        raise ValueError("native summary window differs from the requested window")
    if "interval_start_s" in summary or "interval_end_s" in summary:
        raise ValueError("periodic summaries are not window captures")
    if any(summary[key] > UINT64_MAX for key in ("presents",)) or summary["window"]["first_present"] > UINT64_MAX:
        raise ValueError("native present identities exceed uint64")
    if window_kind == "frames" and (summary["presents"] != length or summary["window"]["first_present"] != start):
        raise ValueError("native frame window has a different present range")
    if window_kind == "seconds" and summary["window"]["ms"] < length * 1000:
        raise ValueError("native seconds window is shorter than requested")
    env = summary["env"]
    selected = "APS5_TRACE_" + window_kind.upper()
    if env.get(selected) != f"{start}:{length}" or not env.get("APS5_TRACE_DIR"):
        raise ValueError("native summary does not record the requested tracing environment")
    prohibited = sorted(name for name in env if instrumentation_name(name) and name not in (selected, "APS5_TRACE_DIR"))
    if prohibited:
        raise ValueError("native summary records additional instrumentation: " + ", ".join(prohibited))
    phases = summary.get("phase_totals_ms")
    if not isinstance(phases, dict):
        raise ValueError("native summary phase_totals_ms must be an object")
    for name, value in phases.items():
        performance_compare.number(value, "phase_totals_ms." + name)
    for thread in summary.get("trace_dropped_threads", {}).values():
        if type(thread.get("totals_full")) is not bool:
            raise ValueError("native dropped thread must record totals_full")
    return summary


def validate_trace(raw, summary, process):
    if len(raw) < 2 and b"\x1f\x8b".startswith(raw):
        raise CapturePending("native gzip trace is still being written")
    if not raw.startswith(b"\x1f\x8b"):
        raise ValueError("native trace must be gzip data")
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    decoded = decoder.decompress(raw, MAX_TRACE_JSON_BYTES + 1)
    if len(decoded) > MAX_TRACE_JSON_BYTES or decoder.unconsumed_tail:
        raise ValueError("native trace decoded JSON exceeds its byte limit")
    if not decoder.eof:
        raise CapturePending("native gzip trace is incomplete")
    if decoder.unused_data:
        raise ValueError("native trace has trailing gzip members or data")
    document = strict_json(decoded, "native trace")
    if not isinstance(document, dict) or not isinstance(document.get("traceEvents"), list):
        raise ValueError("native trace must contain a traceEvents array")
    events = document["traceEvents"]
    if len(events) > MAX_EVENTS:
        raise ValueError("native trace exceeds its event limit")
    trace = trace_digest.Trace(document)
    process_names, presents = [], []
    threads, activity_threads, presenters, loss_threads = set(), set(), set(), set()
    for event in events:
        if type(event.get("pid")) is not int or event["pid"] != process:
            raise ValueError("native trace process identity differs from its filename")
        tid = event.get("tid")
        if type(tid) is not int or not 0 <= tid <= UINT64_MAX:
            raise ValueError("native trace thread identity must fit uint64")
        if type(event.get("ts")) not in (int, float) or not math.isfinite(event["ts"]):
            raise ValueError("native trace timestamp must be finite")
        args = event.get("args", {})
        if not isinstance(args, dict):
            raise ValueError("native trace event args must be an object")
        phase, name = event.get("ph"), event.get("name")
        if phase not in ("M", "X", "i", "C"):
            raise ValueError("native trace contains an unsupported event phase")
        if phase == "M":
            if event["ts"] != 0:
                raise ValueError("native trace metadata must be at the trace origin")
            if name == "process_name" and tid == 0:
                process_names.append(args.get("name"))
            elif name == "thread_name" and tid != 0 and tid not in threads:
                threads.add(tid)
            else:
                raise ValueError("native trace has invalid or duplicate process/thread metadata")
            continue
        activity_threads.add(tid)
        if phase == "C":
            if args.keys() != {"ms"} or args["ms"] < 0:
                raise ValueError("native folded counter must contain a nonnegative ms value")
        elif phase == "i" and name == "present":
            presenters.add(tid)
            present = args.get("present")
            if type(present) is not int or not 0 <= present <= UINT64_MAX:
                raise ValueError("native trace present identity must fit uint64")
            presents.append((event["ts"], present))
        elif phase == "i" and name == "trace buffer full":
            if tid in loss_threads or not 0 < args["events_lost"] <= UINT64_MAX:
                raise ValueError("native trace loss marker must be unique per thread and carry a positive uint64 count")
            loss_threads.add(tid)
        elif phase == "i":
            raise ValueError("native trace contains an unsupported instant event")
    if process_names != ["AnyPS5"]:
        raise ValueError("native trace must name one AnyPS5 process")
    if activity_threads - threads:
        raise ValueError("native trace contains events for an undeclared thread")
    if len(presenters) > 1:
        raise ValueError("native trace must use a single presenter thread")
    if len(presents) != summary["presents"]:
        raise ValueError("native trace present count differs from its summary")
    first = summary["window"]["first_present"]
    if any(present != first + index for index, (_, present) in enumerate(presents)):
        raise ValueError("native trace present range differs from its summary")
    if any(presents[index][0] > presents[index + 1][0] for index in range(len(presents) - 1)):
        raise ValueError("native trace presents are not ordered by time")
    if presents[0][0] != 0 or any(timestamp < 0 or timestamp > summary["window"]["ms"] * 1000 + 1
                                   for timestamp, _ in presents):
        raise ValueError("native trace present times differ from its summary window")
    if trace.losses and not summary["trace_dropped"]:
        raise ValueError("native trace reports event losses absent from its summary")
    return trace


def capture_warnings(summary, trace):
    warnings = []
    completeness = {"events": "complete", "totals": "complete"}
    if summary["dirty"] is None:
        warnings.append("Native build dirty state is unknown.")
    elif summary["dirty"]:
        warnings.append("Native build records tracked source changes.")
    threads = summary.get("trace_dropped_threads", {})
    lost = bool(trace.losses) or any(thread.get("events_lost", 0) for thread in threads.values())
    totals_full = any(thread["totals_full"] for thread in threads.values())
    if summary["trace_dropped"]:
        if lost:
            completeness["events"] = "incomplete"
            warnings.append("Trace events were dropped; slices describe retained events only.")
        if totals_full:
            completeness["totals"] = "incomplete"
            warnings.append("Native phase totals overflowed; recorded totals are incomplete.")
        if not lost and not totals_full:
            completeness = {"events": "unknown", "totals": "unknown"}
            warnings.append("Native trace reports dropped data without a classified event or totals loss.")
    elif lost or totals_full:
        raise ValueError("native dropped-thread details disagree with trace_dropped")
    return warnings, completeness


def inspect_capture(run_dir, window_kind, window, title_id, content_version=None, metadata_only=False):
    try:
        if window_kind not in ("frames", "seconds"):
            raise ValueError("window kind must be frames or seconds")
        if not isinstance(window, (tuple, list)) or len(window) != 2 or any(type(value) is not int for value in window):
            raise ValueError("window must contain its parsed start and length")
        window = parse_window(f"{window[0]}:{window[1]}")
        directory = Path(os.path.abspath(run_dir))
        for part in (directory, *directory.parents):
            if part.is_symlink() or (part.exists() and getattr(part.stat(), "st_file_attributes", 0) & 0x400):
                raise ValueError("capture directory cannot contain links or reparse points")
        raw, summary_artifact = read_regular(directory / "aps5-summary.jsonl", MAX_SUMMARY_BYTES)
        if not raw:
            raise CapturePending("native summary has not been written")
        lines = raw.splitlines()
        if len(lines) != 1:
            raise ValueError("capture requires exactly one native summary line")
        if not raw.endswith(b"\n"):
            raise CapturePending("native summary line is incomplete")
        summary = validate_summary(strict_json(raw, "native summary"), window_kind, window, title_id, content_version)
        name = summary["trace"]
        if any(character in name for character in "/\\:\r\n\x00") or len(name) > 2048:
            raise ValueError("native trace must be a basename inside the capture directory")
        match = re.fullmatch(r"aps5-trace-(.+)-([0-9]+)\.json\.gz", name)
        if match is None or match[1] != summary["commit"] or len(match[2]) > 20 or not 0 < int(match[2]) <= UINT64_MAX:
            raise ValueError("native trace filename must match its native commit and process")
        candidates = list(directory.glob("aps5-trace-*.json.gz"))
        if len(candidates) > 1 or any(path.name != name for path in candidates):
            raise ValueError("capture directory contains duplicate or unrelated native traces")
        if metadata_only:
            info = (directory / name).lstat()
            if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or
                    getattr(info, "st_file_attributes", 0) & 0x400):
                raise ValueError("native trace must be a regular file without links or reparse points")
            if info.st_size > MAX_TRACE_BYTES:
                raise ValueError("native trace exceeds its byte limit")
            return {"state": "ready", "reason": "Native metadata and trace file are present; trace validation is pending.",
                    "native_summary": summary, "process_id": int(match[2]), "warnings": []}
        trace_raw, trace_artifact = read_regular(directory / name, MAX_TRACE_BYTES)
        trace = validate_trace(trace_raw, summary, int(match[2]))
        warnings, completeness = capture_warnings(summary, trace)
        return {"state": "complete", "reason": "Native window summary and trace were collected.",
                "native_summary": summary, "process_id": int(match[2]), "warnings": warnings,
                "completeness": completeness, "artifacts": {"summary": summary_artifact, "trace": trace_artifact}}
    except (FileNotFoundError, CapturePending) as error:
        return {"state": "pending", "reason": str(error), "warnings": []}
    except (OSError, ValueError, TypeError, KeyError, RecursionError, OverflowError, zlib.error) as error:
        return {"state": "invalid", "reason": str(error), "warnings": []}
