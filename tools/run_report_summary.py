import argparse
from collections import Counter, defaultdict
import json
import math
import re
import sys
from pathlib import Path

from performance_compare import unique_json_object


DIAGNOSTIC_PREVIEW_LIMIT = 5
MAX_REPORT_COUNT = (1 << 53) - 1


def text(value):
    if not isinstance(value, str) or not value:
        return "unknown"
    return "".join(character if character.isprintable() else " " for character in value)


def is_finite_number(value):
    return type(value) is int or (type(value) is float and math.isfinite(value))


def number_text(value):
    return str(value) if is_finite_number(value) else "unknown"


def count_text(value, noun):
    return f"{number_text(value)} {noun}{'' if value == 1 else 's'}"


def diagnostic_entries(report, section, key):
    entries = report[section].get(key, [])
    if not isinstance(entries, list) or any(not isinstance(entry, dict) for entry in entries):
        raise ValueError(f"report field '{section}.{key}' must be an array of objects")
    return entries


def total_count(items, key):
    counts = [item.get(key, 0) for item in items if isinstance(item, dict)]
    if any(not is_finite_number(count) for count in counts):
        return None
    try:
        total = sum(counts)
    except OverflowError:
        return None
    return total if is_finite_number(total) else None


def render(report):
    validate_report(report)
    game = report["game"]
    system = report["system"]
    run = report["run"]
    coverage = report["coverage"]
    shaders = report["shaders"]
    truncated = report["truncated"]
    stop = report["stop"]

    title = text(game.get("name"))
    if game.get("titleId"):
        title += f" ({text(game['titleId'])})"
    lines = ["AnyPS5 run summary", f"Generated: {text(report.get('generated'))}", f"Game: {title}"]

    result = text(run.get("exit"))
    if run.get("exitCode") is not None:
        result += f" (exit code {number_text(run['exitCode'])})"
    lines.append(f"Result: {result}")
    lines.append(f"Progress: {text(run.get('stage'))} after {number_text(run.get('seconds'))} seconds")

    frames = run.get("framesPresented")
    if coverage.get("milestones"):
        frames_valid = type(frames) is int and frames >= 0
        frame_summary = count_text(frames, "frame") if frames_valid else "unknown frame count"
        fps = run.get("averageFps")
        if frames_valid and frames > 1 and is_finite_number(fps) and fps >= 0:
            fps_text = f"{fps}.00" if type(fps) is int else f"{fps:.2f}"
            frame_summary += f", {fps_text} average FPS"
        elif frames_valid and frames > 1:
            frame_summary += ", average FPS unavailable"
        lines.append(f"Rendering: {frame_summary}")
    else:
        lines.append("Rendering: milestone data was not collected")

    gpu = system.get("gpu")
    gpu_name = gpu.get("name") if isinstance(gpu, dict) else None
    os_name = text(system.get("os"))
    system_summary = [{"linux": "Linux", "windows": "Windows", "macos": "macOS"}.get(os_name, os_name), text(system.get("osVersion"))]
    if system.get("cpu"):
        system_summary.append(text(system["cpu"]))
    if system.get("ramGiB") is not None:
        system_summary.append(f"{number_text(system['ramGiB'])} GiB RAM")
    if gpu_name:
        system_summary.append(text(gpu_name))
    lines.append("System: " + ", ".join(system_summary))

    stop_summary = text(stop.get("kind"))
    if stop.get("reason"):
        stop_summary += f" ({text(stop['reason'])})"
    if stop.get("message"):
        stop_summary += f": {text(stop['message'])}"
    lines.append(f"Stop: {stop_summary}")

    if coverage.get("missing"):
        functions = diagnostic_entries(report, "missing", "functions")
        calls = total_count(functions, "calls")
        lines.append(f"Missing functions: {count_text(calls, 'call')} across {count_text(len(functions), 'function')}")
        for item in functions[:DIAGNOSTIC_PREVIEW_LIMIT]:
            library = text(item.get("library"))
            lines.append(f"  - {library}::{text(item.get('name'))} ({count_text(item.get('calls', 0), 'call')})")
        if len(functions) > DIAGNOSTIC_PREVIEW_LIMIT:
            remaining = len(functions) - DIAGNOSTIC_PREVIEW_LIMIT
            lines.append(f"  - {remaining} more function{'' if remaining == 1 else 's'} in the full report")
        if truncated.get("functions", 0):
            lines.append(f"  - {count_text(truncated['functions'], 'additional function')} omitted")
    else:
        lines.append("Missing functions: not collected")

    if coverage.get("shaders"):
        lines.append(f"Shaders: {number_text(shaders.get('compiled', 0))} compiled, {count_text(shaders.get('failed', 0), 'failure')}")
    else:
        lines.append("Shaders: compilation data was not collected")
    if coverage.get("unsupported"):
        unsupported = diagnostic_entries(report, "shaders", "unsupported")
        total = total_count(unsupported, "count")
        lines.append(f"Unsupported shader instructions: {count_text(total, 'occurrence')} across {count_text(len(unsupported), 'instruction')}")
        for item in unsupported[:DIAGNOSTIC_PREVIEW_LIMIT]:
            opcode_value = item.get("opcodeValue")
            instruction = item.get("opcode") or (opcode_value if opcode_value is not None else "unknown instruction")
            lines.append(f"  - {text(item.get('encoding'))}: {text(str(instruction))} ({count_text(item.get('count', 0), 'occurrence')})")
        if len(unsupported) > DIAGNOSTIC_PREVIEW_LIMIT:
            remaining = len(unsupported) - DIAGNOSTIC_PREVIEW_LIMIT
            lines.append(f"  - {remaining} more instruction{'' if remaining == 1 else 's'} in the full report")
        if truncated.get("unsupported", 0):
            lines.append(f"  - {count_text(truncated['unsupported'], 'additional instruction')} omitted")
    else:
        lines.append("Unsupported shader instructions: not collected")

    if run.get("recovered"):
        lines.append("Warning: recovered report; the run ended without writing its normal report")
    if run.get("incomplete"):
        lines.append("Warning: one or more observations were dropped")
    if any(value for value in truncated.values() if isinstance(value, int) and not isinstance(value, bool)):
        lines.append("Warning: one or more diagnostic lists were truncated")
    lines.append("This summary uses only the fields present in the local run report; it does not upload data.")
    return "\n".join(lines)


def validate_report(report):
    if (not isinstance(report, dict) or report.get("format") != "anyps5-report"
            or type(report.get("version")) is not int or report["version"] != 1):
        raise ValueError("expected an AnyPS5 run report, version 1")
    for key in ("game", "system", "run", "coverage", "missing", "shaders", "truncated", "stop"):
        if not isinstance(report.get(key), dict):
            raise ValueError(f"report field {key!r} must be an object")
    for key in ("milestones", "missing", "shaders", "unsupported"):
        if type(report["coverage"].get(key)) is not bool:
            raise ValueError(f"report field 'coverage.{key}' must be a boolean")
    for key in ("recovered", "incomplete"):
        if type(report["run"].get(key)) is not bool:
            raise ValueError(f"report field 'run.{key}' must be a boolean")


def triage_text(section, field, pattern, nullable=False):
    key = field.rsplit(".", 1)[-1]
    value = section.get(key, ...)
    if nullable and value is None:
        return None
    if not isinstance(value, str) or not re.fullmatch(pattern, value):
        raise ValueError(f"invalid report field '{field}'")
    return value


def triage_choice(section, field, choices):
    value = section.get(field.rsplit(".", 1)[-1])
    if value not in choices:
        raise ValueError(f"invalid report field '{field}'")
    return value


def triage_count(value, field, minimum=0):
    if type(value) is not int or not minimum <= value <= MAX_REPORT_COUNT:
        raise ValueError(f"invalid report field '{field}'")
    return value


def triage_record(report):
    validate_report(report)
    build = report.get("anyps5")
    if not isinstance(build, dict):
        raise ValueError("report field 'anyps5' must be an object")
    modified = build.get("modified", ...)
    if modified is not None and type(modified) is not bool:
        raise ValueError("invalid report field 'anyps5.modified'")
    game = report["game"]
    identity = (
        triage_text(game, "game.titleId", r"[A-Z]{4}[0-9]{5}", nullable=True),
        triage_text(game, "game.contentVersion", r"[0-9A-Za-z._-]{1,32}", nullable=True),
        triage_text(build, "anyps5.commit", r"[0-9a-f]{40}", nullable=True),
        triage_text(build, "anyps5.release", r"v[0-9A-Za-z][0-9A-Za-z._-]{0,63}", nullable=True),
        modified,
    )
    run = report["run"]
    stop = report["stop"]
    run_id = triage_text(run, "run.id", r"[0-9a-f]{32}")
    outcome = (
        triage_choice(run, "run.exit", ("clean", "crash", "abort", "killed")),
        triage_choice(run, "run.stage", ("unknown", "loads", "starts", "first-draw", "presents")),
        triage_choice(stop, "stop.kind",
                      ("none", "missing-function", "unsupported-shader", "unsupported-format", "crash", "assert", "other")),
        triage_choice(stop, "stop.reason", ("guest-exit", "quit-requested", "runtime-stop", "crash", "unclean-unknown", "other")),
    )
    if not report["coverage"]["milestones"] and outcome[1] != "unknown":
        raise ValueError("report contains a recorded stage with coverage.milestones off")
    if "functions" not in report["missing"]:
        raise ValueError("report field 'missing.functions' is required")
    functions = Counter()
    for entry in diagnostic_entries(report, "missing", "functions"):
        library = triage_text(entry, "missing.functions.library", r"[A-Za-z0-9_.-]{1,64}", nullable=True)
        name = triage_text(entry, "missing.functions.name", r"[A-Za-z0-9_.$]{1,128}")
        functions[library, name] += triage_count(entry.get("calls"), "missing.functions.calls", minimum=1)
    omitted = triage_count(report["truncated"].get("functions"), "truncated.functions")
    if not report["coverage"]["missing"]:
        if functions or omitted:
            raise ValueError("report contains missing function data with coverage.missing off")
        coverage = "not_collected"
    elif omitted or run["recovered"] or run["incomplete"]:
        coverage = "partial"
    else:
        coverage = "complete"
    return run_id, identity, (outcome, coverage, run["recovered"], run["incomplete"], functions)


def summarize_group(identity, records):
    outcomes = Counter()
    coverage = dict.fromkeys(("complete", "complete_without_observations", "partial", "not_collected"), 0)
    calls = Counter()
    runs_by_function = Counter()
    recovered = incomplete = 0
    for outcome, status, was_recovered, was_incomplete, functions in records:
        outcomes[outcome] += 1
        coverage[status] += 1
        if status == "complete" and not functions:
            coverage["complete_without_observations"] += 1
        calls.update(functions)
        runs_by_function.update(functions.keys())
        recovered += was_recovered
        incomplete += was_incomplete

    title_id, content_version, commit, release, modified = identity
    return {
        "title_id": title_id, "content_version": content_version,
        "build_commit": commit, "build_release": release, "build_modified": modified,
        "runs": len(records), "recovered_runs": recovered, "incomplete_runs": incomplete,
        "outcomes": [
            {"exit": exit_kind, "stage": stage, "stop_kind": kind, "stop_reason": reason, "runs": count}
            for (exit_kind, stage, kind, reason), count in sorted(outcomes.items())
        ],
        "missing_function_coverage": coverage,
        "missing_functions": [
            {"library": library, "name": name, "observed_calls": calls[library, name], "runs": runs_by_function[library, name]}
            for library, name in sorted(calls, key=lambda key: (key[0] or "", key[1]))
        ],
    }


def read_report(path):
    return json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_json_object)


def triage(paths):
    seen = {}
    groups = defaultdict(list)
    for index, path in enumerate(paths, 1):
        try:
            report = read_report(path)
        except (OSError, UnicodeError) as error:
            raise ValueError(f"report {index}: could not read the input as UTF-8") from error
        except (ValueError, RecursionError) as error:
            raise ValueError(f"report {index}: invalid JSON") from error
        try:
            run_id, identity, record = triage_record(report)
        except ValueError as error:
            raise ValueError(f"report {index}: {error}") from error
        previous = seen.get(run_id)
        if previous is not None:
            if previous != (identity, record):
                raise ValueError(f"report {index}: conflicting evidence for an already seen run ID")
            continue
        seen[run_id] = (identity, record)
        groups[identity].append(record)
    identities = sorted(groups, key=lambda key: tuple((str(value), value is not None) for value in key))
    return {
        "format": "anyps5-run-triage", "version": 1,
        "input_reports": len(paths), "unique_runs": len(seen), "duplicate_reports": len(paths) - len(seen),
        "groups": [summarize_group(identity, groups[identity]) for identity in identities],
    }


def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Print a readable summary of a local AnyPS5 run report.")
    parser.add_argument("--triage", action="store_true", help="summarize multiple reports as JSON")
    parser.add_argument("report", type=Path, nargs="+", help="path to anyps5-report.json")
    args = parser.parse_args()
    if not args.triage and len(args.report) != 1:
        parser.error("multiple report paths require --triage")
    try:
        if hasattr(sys.stdout, "reconfigure"):
            sys.stdout.reconfigure(encoding="utf-8")
        if args.triage:
            print(json.dumps(triage(args.report), indent=2))
        else:
            print(render(read_report(args.report[0])))
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError, TypeError, AttributeError, KeyError, RecursionError) as error:
        print(f"run_report_summary: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
