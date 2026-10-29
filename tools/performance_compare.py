import argparse
import json
import math
import re
import sys
from pathlib import Path


CONTEXT = ("gpu", "driver", "os", "resolution")
OPTIONAL_STRINGS = ("cpu", "title_id", "title", "content_version")
OPTIONAL_INTEGERS = ("cpu_threads", "memory_mib")
PERCENTILES = ("p50", "p90", "p99", "max")


def number(value, name):
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and value >= 0
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(f"{name} must be a finite, non-negative number")
    return value


def validate(summary):
    for key in ("commit", "trace", *CONTEXT):
        if not isinstance(summary.get(key), str):
            raise ValueError(f"{key} must be a string")
    for key in OPTIONAL_STRINGS:
        if key in summary and not isinstance(summary[key], str):
            raise ValueError(f"{key} must be a string")
    for key in OPTIONAL_INTEGERS:
        if key in summary and (type(summary[key]) is not int or summary[key] < 0):
            raise ValueError(f"{key} must be a non-negative integer")
    if type(summary.get("trace_dropped")) is not bool:
        raise ValueError("trace_dropped must be a boolean")
    if "trace_dropped_threads" in summary:
        threads = summary["trace_dropped_threads"]
        if not isinstance(threads, dict):
            raise ValueError("trace_dropped_threads must be an object")
        for name, thread in threads.items():
            if not isinstance(thread, dict):
                raise ValueError(f"trace_dropped_threads.{name} must be an object")
            if "totals_full" in thread and type(thread["totals_full"]) is not bool:
                raise ValueError(f"trace_dropped_threads.{name}.totals_full must be a boolean")
            if "events_lost" in thread and (type(thread["events_lost"]) is not int or thread["events_lost"] < 0):
                raise ValueError(f"trace_dropped_threads.{name}.events_lost must be a non-negative integer")
    if "dirty" not in summary or (summary["dirty"] is not None and type(summary["dirty"]) is not bool):
        raise ValueError("dirty must be a boolean or null")
    env = summary.get("env")
    if not isinstance(env, dict) or any(not isinstance(v, str) for v in env.values()):
        raise ValueError("env must map names to strings")
    window = summary.get("window")
    if not isinstance(window, dict) or not isinstance(window.get("spec"), str):
        raise ValueError("window must contain a string spec")
    if number(window.get("ms"), "window.ms") == 0:
        raise ValueError("window.ms must be positive")
    if type(window.get("first_present")) is not int or window["first_present"] < 0:
        raise ValueError("window.first_present must be a non-negative integer")
    if type(summary.get("presents")) is not int or summary["presents"] <= 0:
        raise ValueError("presents must be a positive integer")
    number(summary["presents"], "presents")
    frames = summary.get("frame_ms")
    if not isinstance(frames, dict):
        raise ValueError("frame_ms must be an object")
    values = [number(frames.get(key), f"frame_ms.{key}") for key in PERCENTILES]
    if values != sorted(values):
        raise ValueError("frame_ms must satisfy p50 <= p90 <= p99 <= max")
    number(summary.get("gpu_busy_ms_per_present"), "gpu_busy_ms_per_present")
    phases = summary.get("top_phases_ms_per_frame")
    if not isinstance(phases, dict):
        raise ValueError("top_phases_ms_per_frame must be an object")
    for name, value in phases.items():
        number(value, f"top_phases_ms_per_frame.{name}")
    return summary


def load_summary(path, trace=None):
    records = []
    with Path(path).open(encoding="utf-8-sig") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
                if not isinstance(record, dict):
                    raise ValueError("summary must be an object")
            except (ValueError, RecursionError) as error:
                raise ValueError(f"{path}:{line_number}: {error}") from error
            if trace is None or record.get("trace") == trace:
                records.append((line_number, record))
    if len(records) != 1:
        selection = f" matching trace {trace!r}" if trace is not None else ""
        raise ValueError(f"{path}: expected one summary{selection}, found {len(records)}; "
                         "select a unique trace with --baseline-trace/--candidate-trace "
                         "or provide a file containing one summary line")
    line_number, record = records[0]
    try:
        return validate(record)
    except ValueError as error:
        raise ValueError(f"{path}:{line_number}: {error}") from error


def delta(baseline, candidate):
    change = candidate - baseline if baseline is not None and candidate is not None else None
    percent = change / baseline * 100 if change is not None and baseline else None
    if change is not None and not math.isfinite(change):
        raise ValueError("metric difference is outside the finite numeric range")
    if percent is not None and not math.isfinite(percent):
        raise ValueError("metric percentage change is outside the finite numeric range")
    return {"baseline": baseline, "candidate": candidate, "change": change, "change_pct": percent}


def compare(baseline, candidate):
    metrics = {f"frame_ms.{key}": delta(baseline["frame_ms"][key], candidate["frame_ms"][key])
               for key in PERCENTILES}
    metrics["mean_frame_ms"] = delta(baseline["window"]["ms"] / baseline["presents"],
                                      candidate["window"]["ms"] / candidate["presents"])
    metrics["gpu_busy_ms_per_present"] = delta(baseline["gpu_busy_ms_per_present"], candidate["gpu_busy_ms_per_present"])
    before_phases = baseline["top_phases_ms_per_frame"]
    after_phases = candidate["top_phases_ms_per_frame"]
    phases = {name: delta(before_phases.get(name), after_phases.get(name))
              for name in sorted(before_phases.keys() | after_phases.keys())}
    contexts = []
    for summary in (baseline, candidate):
        context = {key: summary[key] for key in CONTEXT}
        context.update({key: summary[key] for key in (*OPTIONAL_STRINGS, *OPTIONAL_INTEGERS) if key in summary})
        context["window.spec"] = summary["window"]["spec"]
        if re.fullmatch(r"seconds [0-9]+:[0-9]+", summary["window"]["spec"]) is None:
            context["window.first_present"] = summary["window"]["first_present"]
        context.update({f"env.{key}": value for key, value in summary["env"].items()})
        contexts.append(context)
    differences = {key: {"baseline": contexts[0].get(key), "candidate": contexts[1].get(key)}
                   for key in sorted(contexts[0].keys() | contexts[1].keys())
                   if contexts[0].get(key) != contexts[1].get(key)}
    notes = ["Compare captures from the same machine, title, content version and scene; matching recorded metadata cannot verify the physical machine or scene.",
             "Changes are candidate minus baseline. A negative frame-time change means a shorter frame interval.",
             "A missing top phase is unreported, not zero; its change is unavailable."]
    if differences:
        notes.append("Recorded configuration differs; these measurements do not isolate the code change.")
    for label, summary in (("baseline", baseline), ("candidate", candidate)):
        threads = summary.get("trace_dropped_threads", {})
        totals_full = any(thread.get("totals_full", False) for thread in threads.values())
        if totals_full:
            notes.append(f"{label}: phase totals are incomplete because a thread's totals table overflowed.")
        if summary["trace_dropped"] and (not threads or any(thread.get("events_lost", 0) for thread in threads.values())):
            if totals_full:
                notes.append(f"{label}: trace events were dropped; the event trace is incomplete.")
            else:
                notes.append(f"{label}: trace events were dropped; the event trace omits some of the work included in phase totals.")
        if summary["dirty"] is not False:
            notes.append(f"{label}: source tree is dirty or its state is unknown; the commit alone does not identify the build.")
        missing = [key for key in (*CONTEXT, *OPTIONAL_STRINGS, *OPTIONAL_INTEGERS) if not summary.get(key)]
        if missing:
            notes.append(f"{label}: missing configuration: {', '.join(missing)}.")
    return {"baseline": baseline, "candidate": candidate, "configuration_differences": differences,
            "metrics_ms": metrics, "top_phases_ms_per_frame": phases, "notes": notes}


def cell(value):
    text = json.dumps(value, ensure_ascii=False) if not isinstance(value, str) else value
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("|", "&#124;").replace("\r", " ").replace("\n", " ")


def markdown(report):
    lines = ["# Performance comparison", "", "| Capture | Commit | Trace | Presents | Window (ms) |",
             "| --- | --- | --- | ---: | ---: |"]
    for label in ("baseline", "candidate"):
        summary = report[label]
        lines.append(f"| {label} | {cell(summary['commit'])} | {cell(summary['trace'])} | "
                     f"{summary['presents']} | {summary['window']['ms']:.3f} |")
    lines.extend(["", *[f"- {note}" for note in report["notes"]]])
    if report["configuration_differences"]:
        lines.extend(["", "## Configuration differences", "", "| Setting | Baseline | Candidate |", "| --- | --- | --- |"])
        for key, values in report["configuration_differences"].items():
            lines.append(f"| {cell(key)} | {cell(values['baseline'])} | {cell(values['candidate'])} |")
    for title, key in (("Frame and GPU times (ms)", "metrics_ms"), ("Top phases (ms per frame)", "top_phases_ms_per_frame")):
        lines.extend(["", f"## {title}", "", "| Metric | Baseline | Candidate | Change | Change (%) |",
                      "| --- | ---: | ---: | ---: | ---: |"])
        for name, values in report[key].items():
            columns = ["n/a" if values[field] is None else format(values[field], "+.3f" if field.startswith("change") else ".3f")
                       for field in ("baseline", "candidate", "change", "change_pct")]
            lines.append(f"| {cell(name)} | {' | '.join(columns)} |")
    return "\n".join(lines) + "\n"


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare two aps5-summary.jsonl captures. "
                                     "Reports numerical differences, not statistical significance or an automatic pass/fail verdict.")
    parser.add_argument("baseline", type=Path, help="Baseline summary JSONL file")
    parser.add_argument("candidate", type=Path, help="Candidate summary JSONL file")
    parser.add_argument("--require-matching-context", action="store_true",
                        help="reject differences in recorded configuration, including optional machine/title metadata; scene identity is not checked")
    parser.add_argument("--baseline-trace", help="Exact trace field to select in the baseline file")
    parser.add_argument("--candidate-trace", help="Exact trace field to select in the candidate file")
    parser.add_argument("--json", action="store_true", help="Write JSON instead of Markdown")
    args = parser.parse_args(argv)
    try:
        report = compare(load_summary(args.baseline, args.baseline_trace), load_summary(args.candidate, args.candidate_trace))
        if args.require_matching_context and report["configuration_differences"]:
            raise ValueError("recorded configuration differs: " + ", ".join(report["configuration_differences"]))
        if args.json:
            try:
                output = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
            except RecursionError as error:
                raise ValueError(f"JSON report: {error}") from error
        else:
            output = markdown(report)
    except (OSError, ValueError, OverflowError, RecursionError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    sys.stdout.write(output)


if __name__ == "__main__":
    main()
