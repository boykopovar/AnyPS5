import csv
import hashlib
import io
import json
import math
import os
import re
import stat
import statistics
from decimal import Decimal
from pathlib import Path

from benchmark_compare import (
    MAX_FILE_BYTES,
    MAX_MANIFEST_BYTES,
    MAX_RUNS,
    UINT32_MAX,
    benchmark_display,
    benchmark_integer,
    benchmark_number,
    benchmark_text,
    reject_benchmark_constant,
)
from performance_compare import cell, delta, unique_json_object


SERIES_FORMAT = "aps5-presentmon-series/1"
REPORT_FORMAT = "aps5-presentmon-comparison/1"
ARMS = ("baseline", "candidate")
CONTEXT_FIELDS = ("title_id", "content_version", "scene", "cpu", "gpu", "driver", "os",
                  "resolution", "settings", "cache_state", "save", "input")
CSV_FIELDS = ("Application", "ProcessID", "SwapChainAddress", "Runtime", "Dropped",
              "TimeInSeconds", "msBetweenPresents")
LOSS_FIELDS = ("EtwEventsLost", "EtwBuffersLost", "OverflowedPresents")
METRICS = ("present_fps", "present_ms_p50", "present_ms_p99")
MAX_TIME = 10**9
MAX_TOTAL_BYTES = 256 * 1024 * 1024
NOTES = [
    "Present FPS is selected presents divided by the declared window duration. Dropped counts rows marked by PresentMon.",
    "Timing statistics use native intervals with both endpoints inside the capture-relative window. "
    "p50 is the median and p99 uses nearest rank.",
    "These are application presentation metrics. Displayed FPS, frame generation, GPU work and scene equivalence are unmeasured.",
    "Builds, machine, settings and scene are operator declarations. Input pins verify the bytes of the manifest and CSVs.",
    "Missing or zero collector loss counters and consistent intervals provide limited evidence of capture completeness.",
]


def require_fields(data, fields, name):
    if not isinstance(data, dict) or data.keys() != set(fields):
        raise ValueError(f"{name} must contain exactly {', '.join(fields)}")


def digest_value(value, name):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{64}", value):
        raise ValueError(f"{name} must be a SHA-256 digest")
    return value.lower()


def read_pinned(path, expected_sha256, limit):
    expected = digest_value(expected_sha256, f"{path} SHA-256")
    if not path.is_file():
        raise ValueError(f"{path}: input must be a regular file")
    with path.open("rb") as source:
        info = os.fstat(source.fileno())
        if not stat.S_ISREG(info.st_mode):
            raise ValueError(f"{path}: input must be a regular file")
        if info.st_size > limit:
            raise ValueError(f"{path}: input exceeds {limit} bytes")
        raw = source.read(limit + 1)
    if len(raw) > limit:
        raise ValueError(f"{path}: input exceeds {limit} bytes")
    actual = hashlib.sha256(raw).hexdigest()
    if actual != expected:
        raise ValueError(f"{path}: SHA-256 mismatch (expected {expected}, read {actual})")
    return raw, {"path": str(path), "sha256": actual, "bytes": len(raw)}, (info.st_dev, info.st_ino)


def parse_swapchain(value):
    if value == "0":
        return 0
    if not isinstance(value, str) or not re.fullmatch(r"0[xX][0-9a-fA-F]{1,16}", value):
        raise ValueError("swapchain must be a hexadecimal address or 0")
    return int(value, 16)


def csv_decimal(value, label, maximum=MAX_TIME):
    if len(value) > 64 or not re.fullmatch(r"[0-9]+(?:\.[0-9]+)?", value):
        raise ValueError(f"{label} must be a finite non-negative decimal")
    number = Decimal(value)
    if number > maximum:
        raise ValueError(f"{label} exceeds {maximum}")
    return number


def presentmon_stream(raw, run):
    reader = csv.reader(io.StringIO(raw.decode("utf-8-sig"), newline=""), strict=True)
    header = next(reader, [])
    if len(header) != len(set(header)) or not set(CSV_FIELDS).issubset(header):
        raise ValueError("expected distinct PresentMon v1 CSV columns: " + ", ".join(CSV_FIELDS))
    columns = [header.index(field) for field in CSV_FIELDS]
    loss_columns = [(field, header.index(field)) for field in LOSS_FIELDS if field in header]
    pid = run["pid"]
    chain = parse_swapchain(run["swapchain"])
    application = run["application"].casefold()
    for row in reader:
        if len(row) != len(header):
            raise ValueError(f"CSV line {reader.line_num} has the wrong column count")
        for field, index in loss_columns:
            counter = row[index]
            if not re.fullmatch(r"[0-9]{1,10}", counter) or int(counter) > UINT32_MAX:
                raise ValueError(f"CSV line {reader.line_num} {field} must be an unsigned 32-bit integer")
            if int(counter):
                raise ValueError(f"CSV line {reader.line_num} {field} reports collector loss")
        app, process, swapchain, runtime, dropped, timestamp, interval = (row[index] for index in columns)
        if not re.fullmatch(r"[0-9]{1,10}", process) or int(process) > UINT32_MAX:
            raise ValueError(f"CSV line {reader.line_num} ProcessID must be an unsigned 32-bit integer")
        if int(process) != pid or parse_swapchain(swapchain) != chain:
            continue
        if app.casefold() != application:
            raise ValueError(f"CSV line {reader.line_num} has a different application for the PID and swapchain")
        benchmark_text(runtime, f"CSV line {reader.line_num} Runtime", True)
        if dropped not in ("0", "1"):
            raise ValueError(f"CSV line {reader.line_num} Dropped must be 0 or 1")
        yield (
            runtime,
            dropped == "1",
            csv_decimal(timestamp, f"CSV line {reader.line_num} TimeInSeconds"),
            csv_decimal(interval, f"CSV line {reader.line_num} msBetweenPresents", MAX_TIME * 1000),
        )


def presentmon_metrics(raw, run, duration):
    start = Decimal(str(run["start_s"]))
    end = start + Decimal(str(duration))
    first = previous = runtime = None
    samples = []
    dropped = presents = 0
    try:
        for row_runtime, row_dropped, timestamp, interval in presentmon_stream(raw, run):
            if runtime is not None and row_runtime != runtime:
                raise ValueError("selected PID and swapchain have multiple Runtime values")
            runtime = row_runtime
            if previous is not None:
                if timestamp <= previous:
                    raise ValueError("selected TimeInSeconds must increase strictly")
                if previous < end and timestamp >= start:
                    elapsed_ms = float((timestamp - previous) * 1000)
                    if not interval or not math.isclose(float(interval), elapsed_ms, rel_tol=1e-7, abs_tol=0.001):
                        raise ValueError("msBetweenPresents disagrees with adjacent TimeInSeconds; "
                                         "use the original CSV with all presents")
            else:
                first = timestamp
            if start <= timestamp < end:
                if previous is not None and previous >= start:
                    samples.append(float(interval))
                presents += 1
                dropped += row_dropped
            previous = timestamp
    except csv.Error as error:
        raise ValueError(f"invalid PresentMon CSV: {error}") from error
    if first is None:
        raise ValueError("no presents for the selected PID and swapchain")
    if first > start or previous < end:
        raise ValueError("selected stream does not cover the entire window")
    if not samples:
        raise ValueError("window must contain at least two presents")
    fps = presents / duration
    if not math.isfinite(fps):
        raise ValueError("selected wall span yields a non-finite present rate")
    samples.sort()
    return {
        "runtime": runtime,
        "window": {"start_s": float(start), "end_s": float(end)},
        "metrics": {
            "presents": presents,
            "dropped": dropped,
            "intervals": len(samples),
            "present_fps": fps,
            "present_ms_p50": statistics.median(samples),
            "present_ms_p99": samples[(len(samples) * 99 + 99) // 100 - 1],
        },
    }


def read_presentmon_plan(raw):
    plan = json.loads(raw.decode("utf-8-sig"), object_pairs_hook=unique_json_object,
                      parse_constant=reject_benchmark_constant)
    if not isinstance(plan, dict) or plan.get("format") != SERIES_FORMAT:
        raise ValueError(f"expected PresentMon manifest format {SERIES_FORMAT}")
    require_fields(plan, ("format", "presentmon_version", "duration_s", "context", *ARMS, "runs"), "manifest")
    benchmark_text(plan["presentmon_version"], "presentmon_version", True)
    if not benchmark_number(plan["duration_s"], "duration_s", MAX_TIME):
        raise ValueError("duration_s must be positive")
    context = plan["context"]
    require_fields(context, CONTEXT_FIELDS, "context")
    for name in CONTEXT_FIELDS:
        benchmark_text(context[name], f"context.{name}", True)
    for name in ("save", "input"):
        if context[name] != "none":
            context[name] = digest_value(context[name], f"context.{name}")
    for arm in ARMS:
        build = plan[arm]
        require_fields(build, ("renderer", "revision", "executable_sha256"), arm)
        for key in ("renderer", "revision"):
            benchmark_text(build[key], f"{arm}.{key}", True)
        build["executable_sha256"] = digest_value(build["executable_sha256"], f"{arm}.executable_sha256")
    runs = plan["runs"]
    if not isinstance(runs, list) or not 2 <= len(runs) <= MAX_RUNS or len(runs) % 2:
        raise ValueError(f"runs must contain 2 to {MAX_RUNS} attempts in adjacent pairs")
    previous_arm = None
    ids = set()
    for index, run in enumerate(runs):
        label = f"runs[{index}]"
        if not isinstance(run, dict) or run.get("status") not in ("captured", "failed", "excluded"):
            raise ValueError(f"{label}.status must be captured, failed or excluded")
        fields = ("id", "arm", "status")
        if run["status"] == "captured":
            require_fields(run, (*fields, "capture", "sha256", "pid", "application", "swapchain", "start_s"), label)
            benchmark_text(run["capture"], f"{label}.capture", True)
            run["sha256"] = digest_value(run["sha256"], f"{label}.sha256")
            if not benchmark_integer(run["pid"], f"{label}.pid", UINT32_MAX):
                raise ValueError(f"{label}.pid must be positive")
            benchmark_text(run["application"], f"{label}.application", True)
            run["swapchain"] = f"{parse_swapchain(run['swapchain']):#x}"
            benchmark_number(run["start_s"], f"{label}.start_s", MAX_TIME)
        else:
            require_fields(run, (*fields, "reason"), label)
            benchmark_text(run["reason"], f"{label}.reason", True)
        run_id = benchmark_text(run["id"], f"{label}.id", True)
        if run_id in ids:
            raise ValueError(f"{label}: duplicate run id {run_id!r}")
        ids.add(run_id)
        arm = run["arm"]
        if arm not in ARMS or arm == previous_arm:
            raise ValueError(f"{label}: runs must alternate baseline and candidate")
        previous_arm = arm
    return plan


def load_presentmon_runs(runs, directory, duration):
    results = []
    seen_digests = set()
    seen_files = set()
    total_bytes = 0
    for run in runs:
        result = {**run, "metrics": None}
        if run["status"] == "captured":
            capture = directory / run["capture"]
            try:
                raw, source, identity = read_pinned(capture, run["sha256"], MAX_FILE_BYTES)
                if source["sha256"] in seen_digests or (identity[1] and identity in seen_files):
                    raise ValueError("capture is reused across attempts")
                seen_digests.add(source["sha256"])
                seen_files.add(identity)
                total_bytes += source["bytes"]
                if total_bytes > MAX_TOTAL_BYTES:
                    raise ValueError(f"captures exceed {MAX_TOTAL_BYTES} bytes in total")
                result.update(presentmon_metrics(raw, run, duration), source=source)
            except (ValueError, OSError) as error:
                raise ValueError(f"run {run['id']!r} ({capture}): {error}") from error
        results.append(result)
    return results


def compare_presentmon_pairs(runs):
    pairs = []
    for index in range(0, len(runs), 2):
        by_arm = {run["arm"]: run for run in runs[index:index + 2]}
        baseline, candidate = (by_arm[arm] for arm in ARMS)
        paired_metrics = {
            name: delta((baseline["metrics"] or {}).get(name), (candidate["metrics"] or {}).get(name))
            for name in METRICS
        }
        pairs.append({
            "id": index // 2 + 1,
            "baseline": baseline["id"],
            "candidate": candidate["id"],
            "complete": baseline["metrics"] is not None and candidate["metrics"] is not None,
            "metrics": paired_metrics,
        })
    return pairs


def presentmon_aggregate(pairs):
    metrics = {}
    for name in METRICS:
        values = [pair["metrics"][name]["change_pct"] for pair in pairs if pair["complete"]]
        metrics[name] = {
            "n": len(values),
            "mean_change_pct": statistics.mean(values) if values else None,
            "min_change_pct": min(values, default=None),
            "max_change_pct": max(values, default=None),
        }
    return {
        "complete_pairs": sum(pair["complete"] for pair in pairs),
        "total_pairs": len(pairs),
        "metrics": metrics,
    }


def compare_presentmon_series(path, sha256):
    path = Path(path)
    raw, source, _ = read_pinned(path, sha256, MAX_MANIFEST_BYTES)
    plan = read_presentmon_plan(raw)
    runs = load_presentmon_runs(plan["runs"], path.parent, plan["duration_s"])
    pairs = compare_presentmon_pairs(runs)
    aggregate = presentmon_aggregate(pairs)
    control = plan["baseline"] == plan["candidate"]
    warnings = []
    if aggregate["complete_pairs"] < 2:
        warnings.append("Characterizing run-to-run variation requires at least two complete measured pairs.")
    if aggregate["complete_pairs"] != len(pairs):
        warnings.append("Attempts without a measurement are retained. Only complete pairs enter aggregate effects.")
    runtimes = {run["runtime"] for run in runs if run["metrics"] is not None}
    if len(runtimes) > 1:
        warnings.append("Selected captures report different Runtime values. Their present observations can use different timing points.")
    if "Other" in runtimes:
        warnings.append("Runtime Other can observe presents first in the kernel and reports zero for an unknown swapchain. "
                        "Verify the intended activity and stream before interpreting timing.")
    if control:
        warnings.append("Both arms declare the same build. Treat the paired effects as an A/A control.")
    return {**plan, "format": REPORT_FORMAT, "source": source, "control": control,
            "runs": runs, "pairs": pairs, "aggregate": aggregate, "warnings": warnings, "notes": list(NOTES)}


def render_presentmon_comparison(report):
    source = report["source"]
    lines = [
        "# External PresentMon comparison",
        "",
        f"Manifest {cell(source['path'])} (SHA-256 {source['sha256']}).",
        f"Operator-declared PresentMon version {cell(report['presentmon_version'])}, "
        f"{report['duration_s']} s windows.",
        "",
    ]
    for arm in ARMS:
        build = report[arm]
        lines.append(f"- {arm}: {cell(build['renderer'])}, {cell(build['revision'])}, "
                     f"executable SHA-256 {build['executable_sha256']}.")
    lines.extend(f"- {name}: {cell(value)}" for name, value in report["context"].items())
    lines.extend(["", "| Run | Arm | Status | Presents | Dropped | Present FPS | p50 (ms) | p99 (ms) | Reason |",
                  "| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |"])
    for run in report["runs"]:
        metrics = run["metrics"] or {}
        values = [benchmark_display(metrics.get(name)) for name in ("presents", "dropped", *METRICS)]
        lines.append(f"| {cell(run['id'])} | {run['arm']} | {run['status']} | {' | '.join(values)} | "
                     f"{cell(run.get('reason', ''))} |")
    lines.extend(["", "## Capture selections", ""])
    for run in report["runs"]:
        if run["metrics"] is not None:
            source = run["source"]
            window = run["window"]
            lines.append(f"- {cell(run['id'])}: {cell(source['path'])}, SHA-256 {source['sha256']}, "
                         f"{cell(run['application'])} PID {run['pid']} swapchain {run['swapchain']} "
                         f"({cell(run['runtime'])}), [{window['start_s']}, {window['end_s']}) s.")
    lines.extend(["", "| Pair | Baseline | Candidate | Complete | Present FPS change (%) | p50 change (%) | p99 change (%) |",
                  "| --- | --- | --- | --- | ---: | ---: | ---: |"])
    for pair in report["pairs"]:
        values = [benchmark_display(pair["metrics"][name]["change_pct"]) for name in METRICS]
        lines.append(f"| {pair['id']} | {cell(pair['baseline'])} | {cell(pair['candidate'])} | {pair['complete']} | "
                     f"{' | '.join(values)} |")
    aggregate = report["aggregate"]
    lines.extend(["", f"{aggregate['complete_pairs']}/{aggregate['total_pairs']} complete measured pairs.", "",
                  "| Metric | Pairs | Mean change (%) | Min (%) | Max (%) |",
                  "| --- | ---: | ---: | ---: | ---: |"])
    for label, name in zip(("Present FPS", "p50", "p99"), METRICS):
        metrics = aggregate["metrics"][name]
        values = [benchmark_display(metrics[key]) for key in ("mean_change_pct", "min_change_pct", "max_change_pct")]
        lines.append(f"| {label} | {metrics['n']} | {' | '.join(values)} |")
    lines.extend(["", *(f"- {cell(note)}" for note in [*report["warnings"], *report["notes"]])])
    return "\n".join(lines) + "\n"
