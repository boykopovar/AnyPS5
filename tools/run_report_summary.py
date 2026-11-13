import argparse
import json
import sys
from pathlib import Path


def text(value):
    if not isinstance(value, str) or not value:
        return "unknown"
    return "".join(character if character.isprintable() else " " for character in value)


def number_text(value):
    return str(value) if type(value) in (int, float) else "unknown"


def count_text(value, noun):
    return f"{number_text(value)} {noun}{'' if value == 1 else 's'}"


def total_count(items, key):
    counts = [item.get(key, 0) for item in items if isinstance(item, dict)]
    if any(type(count) not in (int, float) for count in counts):
        return None
    return sum(counts)


def render(report):
    if (not isinstance(report, dict) or report.get("format") != "anyps5-report"
            or type(report.get("version")) is not int or report["version"] != 1):
        raise ValueError("expected an AnyPS5 run report, version 1")
    for key in ("game", "system", "run", "coverage", "missing", "shaders", "truncated", "stop"):
        if not isinstance(report.get(key), dict):
            raise ValueError(f"report field {key!r} must be an object")

    game = report["game"]
    system = report["system"]
    run = report["run"]
    coverage = report["coverage"]
    missing = report["missing"]
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
        frame_summary = count_text(frames, "frame") if isinstance(frames, int) and not isinstance(frames, bool) else "unknown frame count"
        fps = run.get("averageFps")
        if isinstance(fps, (int, float)) and not isinstance(fps, bool):
            frame_summary += f", {fps:.2f} average FPS"
        elif isinstance(frames, int) and not isinstance(frames, bool) and frames > 1:
            frame_summary += ", average FPS unavailable"
        lines.append(f"Rendering: {frame_summary}")
    else:
        lines.append("Rendering: milestone data was not collected")

    gpu = system.get("gpu")
    gpu_name = gpu.get("name") if isinstance(gpu, dict) else None
    system_summary = [text(system.get("os")).capitalize(), text(system.get("osVersion"))]
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
        functions = missing.get("functions", [])
        calls = total_count(functions, "calls")
        lines.append(f"Missing functions: {count_text(calls, 'call')} across {count_text(len(functions), 'function')}")
        for item in functions[:5]:
            if isinstance(item, dict):
                library = text(item.get("library"))
                lines.append(f"  - {library}::{text(item.get('name'))} ({count_text(item.get('calls', 0), 'call')})")
        if truncated.get("functions", 0):
            lines.append(f"  - {count_text(truncated['functions'], 'additional function')} omitted")
    else:
        lines.append("Missing functions: not collected")

    if coverage.get("shaders"):
        lines.append(f"Shaders: {number_text(shaders.get('compiled', 0))} compiled, {count_text(shaders.get('failed', 0), 'failure')}")
    else:
        lines.append("Shaders: compilation data was not collected")
    if coverage.get("unsupported"):
        unsupported = shaders.get("unsupported", [])
        total = total_count(unsupported, "count")
        lines.append(f"Unsupported shader instructions: {count_text(total, 'occurrence')} across {count_text(len(unsupported), 'instruction')}")
        for item in unsupported[:5]:
            if isinstance(item, dict):
                instruction = item.get("opcode") or item.get("opcodeValue") or "unknown instruction"
                lines.append(f"  - {text(item.get('encoding'))}: {text(str(instruction))} ({count_text(item.get('count', 0), 'occurrence')})")
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


def main():
    parser = argparse.ArgumentParser(description="Print a readable summary of a local AnyPS5 run report.")
    parser.add_argument("report", type=Path, help="path to anyps5-report.json")
    args = parser.parse_args()
    try:
        report = json.loads(args.report.read_text(encoding="utf-8"))
        if hasattr(sys.stdout, "reconfigure"):
            sys.stdout.reconfigure(encoding="utf-8")
        print(render(report))
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError, TypeError, AttributeError, KeyError) as error:
        print(f"run_report_summary: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
