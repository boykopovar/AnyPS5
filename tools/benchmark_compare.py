import argparse
import gzip
import hashlib
import io
import json
import math
import os
import re
import stat
import statistics
import sys
import zlib
from pathlib import Path

from performance_compare import cell, delta, unique_json_object


MAX_FILE_BYTES = 32 * 1024 * 1024
MAX_JSON_BYTES = 64 * 1024 * 1024
MAX_MANIFEST_BYTES = 1024 * 1024
MAX_FRAMES = 2_000_000
MAX_TOTAL_FRAMES = 8_000_000
MAX_INTERVALS = 240
MAX_RUNS = 64
UINT32_MAX = 2**32 - 1
UINT64_MAX = 2**64 - 1
IDENTITY_STRINGS = ("cpu", "title_id", "title", "content_version")
IDENTITY_INTEGERS = ("cpu_threads", "memory_mib")
METRICS = ("fps", "mean_frame_ms", "frame_ms.p50", "frame_ms.p90", "frame_ms.p99", "frame_ms.max")
NOTES = [
    "Native benchmark timing is instrumented; APS5_TRACE_SUMMARY_EVERY adds scope clock reads and GPU timestamps.",
    "FPS uses frame count divided by selected interval wall time. Raw frame durations are truncated to integer microseconds.",
    "Only completed intervals are retained; the open interval at exit is absent. Selection never removes startup or outliers automatically.",
    "Matching recorded metadata cannot verify the physical machine, scene, inputs, cache state or independent capture provenance.",
    "Resolution is recorded at each interval's closing present; it does not prove a constant resolution throughout the interval.",
]


def benchmark_number(value, name, maximum=1e15):
    if type(value) not in (int, float):
        raise ValueError(f"{name} must be a finite non-negative number")
    try:
        valid = math.isfinite(value) and 0 <= value <= maximum
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(f"{name} must be finite and between 0 and {maximum:g}")
    return value


def benchmark_integer(value, name, maximum=UINT64_MAX):
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError(f"{name} must be an integer between 0 and {maximum}")
    return value


def benchmark_text(value, name, nonempty=False):
    if not isinstance(value, str) or len(value) > 4096 or (nonempty and not value.strip()):
        raise ValueError(f"{name} must be {'a nonempty' if nonempty else 'a'} string of at most 4096 characters")
    return value


def reject_benchmark_constant(value):
    raise ValueError(f"non-finite JSON constant: {value}")


def read_benchmark_json(path, limit=MAX_FILE_BYTES, decoded_limit=None):
    path = Path(path)
    if not path.is_file():
        raise ValueError(f"{path}: input must be a regular file")
    decoded_limit = MAX_JSON_BYTES if decoded_limit is None else min(decoded_limit, MAX_JSON_BYTES)
    with path.open("rb") as source:
        info = os.fstat(source.fileno())
        if not stat.S_ISREG(info.st_mode):
            raise ValueError(f"{path}: input must be a regular file")
        if info.st_size > limit:
            raise ValueError(f"{path}: input exceeds {limit} bytes")
        raw = source.read(limit + 1)
    if len(raw) > limit:
        raise ValueError(f"{path}: input exceeds {limit} bytes")
    if raw.startswith(b"\x1f\x8b"):
        try:
            with gzip.GzipFile(fileobj=io.BytesIO(raw)) as stream:
                decoded = stream.read(decoded_limit + 1)
        except (OSError, EOFError, zlib.error) as error:
            raise ValueError(f"{path}: invalid gzip data: {error}") from error
    else:
        decoded = raw
    if len(decoded) > decoded_limit:
        raise ValueError(f"{path}: decoded JSON exceeds {decoded_limit} bytes")
    value = json.loads(decoded.decode("utf-8-sig"), object_pairs_hook=unique_json_object,
                       parse_constant=reject_benchmark_constant)
    return value, {
        "path": str(path),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "decoded_sha256": hashlib.sha256(decoded).hexdigest(),
        "bytes": len(raw),
        "_identity": (info.st_dev, info.st_ino),
    }


def validate_benchmark(data):
    if not isinstance(data, dict) or data.get("format") != "aps5-bench/1":
        raise ValueError("expected benchmark format aps5-bench/1")
    for name in ("commit", "gpu", "driver", "os"):
        benchmark_text(data.get(name), name)
    if "dirty" not in data or (data["dirty"] is not None and type(data["dirty"]) is not bool):
        raise ValueError("dirty must be a boolean or null")
    environment = data.get("env")
    if not isinstance(environment, dict) or len(environment) > 128:
        raise ValueError("env must be an object with at most 128 string fields")
    for name, value in environment.items():
        benchmark_text(name, "env key", True)
        benchmark_text(value, f"env.{name}")
    for name in IDENTITY_STRINGS:
        if name in data:
            benchmark_text(data[name], name)
    for name in IDENTITY_INTEGERS:
        if name in data:
            benchmark_integer(data[name], name)
    benchmark_integer(data.get("intervals_dropped"), "intervals_dropped")
    intervals = data.get("intervals")
    if not isinstance(intervals, list) or len(intervals) > MAX_INTERVALS:
        raise ValueError(f"intervals must be an array of at most {MAX_INTERVALS} intervals")
    total_frames = 0
    previous = None
    for index, interval in enumerate(intervals):
        prefix = f"intervals[{index}]"
        if not isinstance(interval, dict):
            raise ValueError(f"{prefix} must be an object")
        start = benchmark_number(interval.get("start_s"), f"{prefix}.start_s")
        end = benchmark_number(interval.get("end_s"), f"{prefix}.end_s")
        if end <= start:
            raise ValueError(f"{prefix}.end_s must be after start_s")
        first = benchmark_integer(interval.get("first_present"), f"{prefix}.first_present")
        benchmark_text(interval.get("resolution"), f"{prefix}.resolution", True)
        benchmark_number(interval.get("gpu_busy_ms"), f"{prefix}.gpu_busy_ms")
        frames = interval.get("frame_us")
        if not isinstance(frames, list) or len(frames) > MAX_FRAMES:
            raise ValueError(f"{prefix}.frame_us must be an array of at most {MAX_FRAMES} frames")
        total_frames += len(frames)
        if total_frames > MAX_FRAMES:
            raise ValueError(f"benchmark exceeds {MAX_FRAMES} frames")
        if first + len(frames) > UINT64_MAX:
            raise ValueError(f"{prefix}: last present exceeds uint64")
        for frame in frames:
            benchmark_integer(frame, f"{prefix}.frame_us", UINT32_MAX)
        if previous is not None:
            if start < previous["end_s"]:
                raise ValueError(f"{prefix}: overlapping or out-of-order interval times")
            if first < previous["first_present"] + len(previous["frame_us"]):
                raise ValueError(f"{prefix}: overlapping or out-of-order present numbers")
        previous = interval
    return data


def load_benchmark(path):
    try:
        data, source = read_benchmark_json(path, MAX_FILE_BYTES)
        return {"data": validate_benchmark(data), "source": source}
    except (ValueError, OSError, EOFError, RecursionError) as error:
        raise ValueError(f"{path}: {error}") from error


def benchmark_metrics(intervals):
    frames = sorted(frame for interval in intervals for frame in interval["frame_us"])
    count = len(frames)
    elapsed = math.fsum(interval["end_s"] - interval["start_s"] for interval in intervals)
    fps = count / elapsed if count and elapsed > 0 else None
    if fps is not None and not math.isfinite(fps):
        raise ValueError("selected wall span yields a non-finite FPS")
    saturated = frames.count(UINT32_MAX)
    quantiles = {}
    for name, percent in (("p50", 50), ("p90", 90), ("p99", 99), ("max", 100)):
        value = frames[min(count - 1, count * percent // 100)] if count else None
        quantiles[name] = value / 1000 if value is not None and value != UINT32_MAX else None
    return {
        "frames": count,
        "elapsed_s": elapsed,
        "fps": fps,
        "mean_frame_ms": sum(frames) / count / 1000 if count and not saturated else None,
        "frame_ms": quantiles,
        "gpu_busy_ms_per_frame": math.fsum(interval["gpu_busy_ms"] for interval in intervals) / count if count else None,
        "zero_frames": frames.count(0),
        "saturated_frames": saturated,
    }


def summarize_benchmark(loaded, intervals=None):
    data = loaded["data"]
    available = data["intervals"]
    if intervals is None:
        start, count = 0, len(available)
    else:
        if not isinstance(intervals, (list, tuple)) or len(intervals) != 2:
            raise ValueError("interval selection must be START:COUNT")
        start = benchmark_integer(intervals[0], "selection start", MAX_INTERVALS)
        count = benchmark_integer(intervals[1], "selection count", MAX_INTERVALS)
        if count == 0 or start + count > len(available):
            raise ValueError("interval selection is empty or outside retained coverage")
    warnings = []
    gaps = set()
    for index in range(1, len(available)):
        previous, current = available[index - 1:index + 1]
        if current["start_s"] != previous["end_s"] or current["first_present"] != previous["first_present"] + len(previous["frame_us"]):
            gaps.add(index)
            warnings.append(f"Capture has a time or present-number gap before retained interval {index}.")
    if any(start < index < start + count for index in gaps):
        raise ValueError("selected intervals contain a coverage gap")
    selected = available[start:start + count]
    metrics = benchmark_metrics(selected)
    metadata = {name: data[name] for name in
                ("commit", "dirty", "env", "gpu", "driver", "os", *IDENTITY_STRINGS, *IDENTITY_INTEGERS)
                if name in data}
    missing = [name for name in ("commit", "gpu", "driver", "os", *IDENTITY_STRINGS, *IDENTITY_INTEGERS)
               if not metadata.get(name)]
    if missing:
        warnings.append("Missing or empty recorded identity: " + ", ".join(missing) + ".")
    if data["dirty"] is not False:
        warnings.append("Source tree is dirty or its state is unknown; the commit alone does not identify the build.")
    if data["intervals_dropped"]:
        warnings.append(f"Producer dropped {data['intervals_dropped']} older intervals; only retained history is available.")
    if not metrics["frames"]:
        warnings.append("Selection contains no recorded frame intervals; timing metrics are unavailable.")
    if metrics["zero_frames"]:
        warnings.append(f"{metrics['zero_frames']} frame durations truncated to zero microseconds.")
    if metrics["saturated_frames"]:
        warnings.append(f"{metrics['saturated_frames']} frame durations are at UINT32_MAX and may be saturated; affected raw statistics are unavailable.")
    resolutions = {interval["resolution"] for interval in selected}
    if len(resolutions) > 1:
        warnings.append("Recorded closing-present resolution changes within the selection; pooled timings cover multiple resolutions.")
    rows = []
    for index, interval in enumerate(selected, start):
        current = benchmark_metrics([interval])
        raw_seconds = sum(interval["frame_us"]) / 1e6
        tolerance = (len(interval["frame_us"]) + 2000) / 1e6
        if not current["saturated_frames"] and abs(raw_seconds - current["elapsed_s"]) > tolerance:
            warnings.append(f"Retained interval {index}: raw durations and wall span differ beyond serialization precision.")
        rows.append({
            "index": index,
            "ordinal": data["intervals_dropped"] + index,
            "start_s": interval["start_s"],
            "end_s": interval["end_s"],
            "first_present": interval["first_present"],
            "last_present": interval["first_present"] + len(interval["frame_us"]),
            "resolution": interval["resolution"],
            "metrics": current,
        })
    selection = {
        "start": start, "count": count, "indices": list(range(start, start + count)),
        "start_s": selected[0]["start_s"] if selected else None,
        "end_s": selected[-1]["end_s"] if selected else None,
        "first_present": selected[0]["first_present"] if selected else None,
        "last_present": selected[-1]["first_present"] + len(selected[-1]["frame_us"]) if selected else None,
    }
    return {
        "format": "aps5-bench-summary/1",
        "source": {key: value for key, value in loaded["source"].items() if not key.startswith("_")},
        "metadata": metadata,
        "intervals_dropped": data["intervals_dropped"],
        "interval_count": len(available),
        "selection": selection,
        "intervals": rows,
        "metrics": metrics,
        "warnings": warnings,
        "notes": list(NOTES),
    }


def load_benchmark_series(path):
    data, source = read_benchmark_json(path, MAX_MANIFEST_BYTES, MAX_MANIFEST_BYTES)
    if not isinstance(data, dict) or data.get("format") != "aps5-bench-series/1":
        raise ValueError("expected series format aps5-bench-series/1")
    if data.keys() - {"format", "recipe", "runs"}:
        raise ValueError("unknown series fields: " + ", ".join(sorted(data.keys() - {"format", "recipe", "runs"})))
    recipe = data.get("recipe", {})
    if not isinstance(recipe, dict) or len(recipe) > 64:
        raise ValueError("recipe must be an object with at most 64 string fields")
    for name, value in recipe.items():
        benchmark_text(name, "recipe key", True)
        benchmark_text(value, f"recipe.{name}")
    runs = data.get("runs")
    if not isinstance(runs, list) or not 1 <= len(runs) <= MAX_RUNS:
        raise ValueError(f"runs must contain between 1 and {MAX_RUNS} attempts")
    ids, pairs = set(), set()
    for index, run in enumerate(runs):
        if not isinstance(run, dict):
            raise ValueError(f"runs[{index}] must be an object")
        unknown = run.keys() - {"id", "arm", "pair", "capture", "intervals", "sha256", "status", "reason"}
        if unknown:
            raise ValueError(f"runs[{index}]: unknown fields: {', '.join(sorted(unknown))}")
        for name in ("id", "pair"):
            benchmark_text(run.get(name), f"runs[{index}].{name}", True)
        if run["id"] in ids:
            raise ValueError(f"duplicate run id: {run['id']}")
        ids.add(run["id"])
        if run.get("arm") not in ("baseline", "candidate"):
            raise ValueError(f"runs[{index}].arm must be baseline or candidate")
        pair = (run["pair"], run["arm"])
        if pair in pairs:
            raise ValueError(f"duplicate {run['arm']} attempt in pair {run['pair']}")
        pairs.add(pair)
        status = run.get("status", "included")
        if status not in ("included", "failed", "excluded"):
            raise ValueError(f"runs[{index}].status must be included, failed or excluded")
        if status == "included":
            benchmark_text(run.get("capture"), f"runs[{index}].capture", True)
            if "reason" in run:
                raise ValueError("included attempts cannot carry an exclusion reason")
            if "intervals" in run:
                selection = run["intervals"]
                if not isinstance(selection, list) or len(selection) != 2:
                    raise ValueError("run intervals must be [start, count]")
                benchmark_integer(selection[0], "selection start", MAX_INTERVALS)
                benchmark_integer(selection[1], "selection count", MAX_INTERVALS)
                if not selection[1]:
                    raise ValueError("selection count must be positive")
            if "sha256" in run and (not isinstance(run["sha256"], str) or re.fullmatch(r"[0-9a-fA-F]{64}", run["sha256"]) is None):
                raise ValueError("sha256 must contain 64 hexadecimal characters")
        else:
            benchmark_text(run.get("reason"), f"runs[{index}].reason", True)
            if run.keys() & {"capture", "intervals", "sha256"}:
                raise ValueError("failed/excluded attempts record a reason, not an included capture")
    return {"data": data, "source": source}


def benchmark_context(summary):
    metadata = summary["metadata"]
    context = {name: metadata.get(name) for name in
               ("gpu", "driver", "os", *IDENTITY_STRINGS, *IDENTITY_INTEGERS)}
    context.update({f"env.{name}": value for name, value in metadata["env"].items()})
    context["selection.ordinals"] = [row["ordinal"] for row in summary["intervals"]]
    context["selection.resolutions"] = [row["resolution"] for row in summary["intervals"]]
    return context


def benchmark_metric_value(summary, key):
    value = summary["metrics"]
    for field in key.split("."):
        value = value[field]
    return value


def compare_benchmark_runs(path, require_matching_context=False):
    loaded_series = load_benchmark_series(path)
    data = loaded_series["data"]
    runs, pairs = [], {}
    seen_files, seen_hashes, seen_decoded = set(), set(), set()
    total_frames = 0
    reference = None
    arms = {}
    differences, warnings = [], []
    for attempt in data["runs"]:
        status = attempt.get("status", "included")
        row = {key: attempt[key] for key in ("id", "arm", "pair")}
        row["status"] = status
        if status != "included":
            row["reason"] = attempt["reason"]
            row["summary"] = None
        else:
            capture = Path(attempt["capture"])
            if not capture.is_absolute():
                capture = Path(path).parent / capture
            loaded = load_benchmark(capture)
            source = loaded["source"]
            if "sha256" in attempt and source["sha256"] != attempt["sha256"].lower():
                raise ValueError(f"{attempt['id']}: SHA-256 does not match original capture bytes")
            identity = source["_identity"]
            if identity in seen_files or source["sha256"] in seen_hashes or source["decoded_sha256"] in seen_decoded:
                raise ValueError(f"{attempt['id']}: duplicate capture evidence cannot count as another run")
            seen_files.add(identity)
            seen_hashes.add(source["sha256"])
            seen_decoded.add(source["decoded_sha256"])
            total_frames += sum(len(interval["frame_us"]) for interval in loaded["data"]["intervals"])
            if total_frames > MAX_TOTAL_FRAMES:
                raise ValueError(f"series exceeds {MAX_TOTAL_FRAMES} supplied frames")
            summary = summarize_benchmark(loaded, attempt.get("intervals"))
            summary["source"]["path"] = attempt["capture"]
            row["summary"] = summary
            warnings.extend(f"{row['id']}: {warning}" for warning in summary["warnings"])
            context = benchmark_context(summary)
            if reference is None:
                reference = context
            else:
                for field in sorted(reference.keys() | context.keys()):
                    if reference.get(field) != context.get(field):
                        differences.append({"run": row["id"], "field": field,
                                            "reference": reference.get(field), "value": context.get(field)})
            build = {name: summary["metadata"][name] for name in ("commit", "dirty")}
            previous_build = arms.setdefault(row["arm"], build)
            for field in build:
                if previous_build[field] != build[field]:
                    differences.append({"run": row["id"], "field": f"{row['arm']}.{field}",
                                        "reference": previous_build[field], "value": build[field]})
        runs.append(row)
        pair = pairs.setdefault(row["pair"], {})
        pair[row["arm"]] = row
    pair_rows = []
    for name, pair in pairs.items():
        baseline, candidate = pair.get("baseline"), pair.get("candidate")
        before = baseline.get("summary") if baseline else None
        after = candidate.get("summary") if candidate else None
        complete = bool(before and after and before["metrics"]["frames"] and after["metrics"]["frames"])
        metrics = {key: delta(benchmark_metric_value(before, key) if before else None,
                              benchmark_metric_value(after, key) if after else None)
                   for key in METRICS}
        pair_rows.append({"id": name, "baseline": baseline["id"] if baseline else None,
                          "candidate": candidate["id"] if candidate else None, "complete": complete,
                          "metrics": metrics})
        if not complete:
            warnings.append(f"Pair {name} has missing, failed, excluded or empty-frame evidence; it is not a complete timing pair.")
    aggregate = {"complete_pairs": sum(row["complete"] for row in pair_rows), "metrics": {}}
    for key in METRICS:
        effects = [row["metrics"][key]["change_pct"] for row in pair_rows
                   if row["complete"] and row["metrics"][key]["change_pct"] is not None]
        aggregate["metrics"][key] = {
            "n": len(effects),
            "mean_change_pct": statistics.mean(effects) if effects else None,
            "min_change_pct": min(effects) if effects else None,
            "max_change_pct": max(effects) if effects else None,
        }
    if differences:
        warnings.append("Recorded configuration or within-arm build identity differs; results do not isolate the code change.")
    if require_matching_context and (differences or not aggregate["complete_pairs"]):
        raise ValueError("recorded context differs or there are no complete timing pairs")
    return {
        "format": "aps5-bench-comparison/1",
        "manifest": {key: value for key, value in loaded_series["source"].items() if not key.startswith("_")},
        "recipe": data.get("recipe", {}),
        "runs": runs,
        "pairs": pair_rows,
        "aggregate": aggregate,
        "configuration_differences": differences,
        "warnings": warnings,
        "notes": [*NOTES,
                  "Recipe fields are operator declarations. Every supplied attempt is shown; the tool cannot discover omitted attempts.",
                  "Pair changes are candidate minus baseline. Aggregate effects are means of available run-pair percentages, not pooled frames or ratios of arm means.",
                  "Frames are correlated observations, not independent runs. These are descriptive results, not statistical significance or a causal speedup verdict.",
                  "An undefined relative change is unavailable, not zero; each metric reports its own number of contributing pairs.",
                  "Source hashes establish integrity of supplied files, not authenticity, source-to-binary provenance or independent runs."],
    }


def parse_benchmark_selection(value):
    match = re.fullmatch(r"([0-9]+):([0-9]+)", value)
    if match is None:
        raise argparse.ArgumentTypeError("intervals must be START:COUNT with a positive count")
    start, count = map(int, match.groups())
    if not 0 <= start < MAX_INTERVALS or not 1 <= count <= MAX_INTERVALS:
        raise argparse.ArgumentTypeError(f"interval selection must fit {MAX_INTERVALS} retained intervals")
    return start, count


def benchmark_display(value):
    return "n/a" if value is None else f"{value:.6g}"


def benchmark_report(report):
    if report["format"] == "aps5-bench-summary/1":
        metadata, metrics = report["metadata"], report["metrics"]
        selection = report["selection"]
        lines = ["# Native benchmark", "",
                 f"Capture: {cell(report['source']['path'])}; SHA-256 {report['source']['sha256']}.",
                 f"Commit: {cell(metadata['commit'])}; dirty: {cell(metadata['dirty'])}; "
                 f"GPU: {cell(metadata['gpu'])}; driver: {cell(metadata['driver'])}; OS: {cell(metadata['os'])}.",
                 f"Title: {cell(metadata.get('title'))}; ID: {cell(metadata.get('title_id'))}; "
                 f"content version: {cell(metadata.get('content_version'))}.",
                 f"CPU: {cell(metadata.get('cpu'))}; threads: {cell(metadata.get('cpu_threads'))}; "
                 f"memory: {cell(metadata.get('memory_mib'))} MiB; environment: {cell(metadata['env'])}.",
                 f"Selected retained intervals {selection['indices']}; wall span {selection['start_s']} to {selection['end_s']} s; "
                 f"{report['intervals_dropped']} older intervals dropped.",
                 f"Frames: {metrics['frames']}; wall FPS: {benchmark_display(metrics['fps'])}; "
                 f"raw p50/p90/p99: {' / '.join(benchmark_display(metrics['frame_ms'][key]) for key in ('p50', 'p90', 'p99'))} ms.",
                 f"Raw mean/max: {benchmark_display(metrics['mean_frame_ms'])} / {benchmark_display(metrics['frame_ms']['max'])} ms; "
                 f"GPU busy per frame: {benchmark_display(metrics['gpu_busy_ms_per_frame'])} ms.",
                 "", "| Retained interval | Ordinal | Start (s) | End (s) | Presents | Frames | Resolution at close | FPS | p99 (ms) |",
                 "| ---: | ---: | ---: | ---: | --- | ---: | --- | ---: | ---: |"]
        for row in report["intervals"]:
            lines.append(f"| {row['index']} | {row['ordinal']} | {row['start_s']} | {row['end_s']} | "
                         f"{row['first_present']}–{row['last_present']} | {row['metrics']['frames']} | {cell(row['resolution'])} | "
                         f"{benchmark_display(row['metrics']['fps'])} | {benchmark_display(row['metrics']['frame_ms']['p99'])} |")
    else:
        lines = ["# Native benchmark comparison", "",
                 f"Series SHA-256: {report['manifest']['sha256']}.",
                 f"Complete run pairs: {report['aggregate']['complete_pairs']}.", ""]
        if report["recipe"]:
            lines.extend(["## Declared recipe", ""])
            lines.extend(f"- {cell(key)}: {cell(value)}" for key, value in report["recipe"].items())
            lines.append("")
        lines.extend(["| Order | Run | Arm | Pair | Status | Frames | FPS | p50 (ms) | p99 (ms) | Reason |",
                      "| ---: | --- | --- | --- | --- | ---: | ---: | ---: | ---: | --- |"])
        for index, row in enumerate(report["runs"], 1):
            metrics = row["summary"]["metrics"] if row["summary"] else None
            values = ([str(metrics["frames"]), benchmark_display(metrics["fps"]),
                       benchmark_display(metrics["frame_ms"]["p50"]), benchmark_display(metrics["frame_ms"]["p99"])]
                      if metrics else ["n/a"] * 4)
            lines.append(f"| {index} | {cell(row['id'])} | {row['arm']} | {cell(row['pair'])} | {row['status']} | "
                         f"{' | '.join(values)} | {cell(row.get('reason', ''))} |")
        lines.extend(["", "## Run-pair changes", "",
                      "| Pair | Baseline | Candidate | Complete | FPS change (%) | p99 change (%) |",
                      "| --- | --- | --- | --- | ---: | ---: |"])
        for row in report["pairs"]:
            lines.append(f"| {cell(row['id'])} | {cell(row['baseline'])} | {cell(row['candidate'])} | {row['complete']} | "
                         f"{benchmark_display(row['metrics']['fps']['change_pct'])} | "
                         f"{benchmark_display(row['metrics']['frame_ms.p99']['change_pct'])} |")
        lines.extend(["", "## Descriptive run-pair effects", "",
                      "| Metric | Pairs | Mean change (%) | Min (%) | Max (%) |",
                      "| --- | ---: | ---: | ---: | ---: |"])
        for name, values in report["aggregate"]["metrics"].items():
            lines.append(f"| {cell(name)} | {values['n']} | {benchmark_display(values['mean_change_pct'])} | "
                         f"{benchmark_display(values['min_change_pct'])} | {benchmark_display(values['max_change_pct'])} |")
        lines.extend(["", "## Capture selections and hashes", ""])
        for row in report["runs"]:
            if row["summary"]:
                summary = row["summary"]
                lines.append(f"- {cell(row['id'])}: {cell(summary['source']['path'])}; "
                             f"SHA-256 {summary['source']['sha256']}; retained intervals {summary['selection']['indices']}; "
                             f"wall span {summary['selection']['start_s']} to {summary['selection']['end_s']} s; "
                             f"{summary['intervals_dropped']} older intervals dropped.")
        lines.extend(["", "## Recorded build and title", "",
                      "| Run | Commit | Dirty | Title ID | Title | Content version |",
                      "| --- | --- | --- | --- | --- | --- |"])
        for row in report["runs"]:
            if row["summary"]:
                metadata = row["summary"]["metadata"]
                values = [cell(metadata.get(key)) for key in
                          ("commit", "dirty", "title_id", "title", "content_version")]
                lines.append(f"| {cell(row['id'])} | {' | '.join(values)} |")
        lines.extend(["", "## Recorded host", "",
                      "| Run | GPU | Driver | OS | CPU | CPU threads | Memory (MiB) |",
                      "| --- | --- | --- | --- | --- | ---: | ---: |"])
        for row in report["runs"]:
            if row["summary"]:
                metadata = row["summary"]["metadata"]
                values = [cell(metadata.get(key)) for key in
                          ("gpu", "driver", "os", "cpu", "cpu_threads", "memory_mib")]
                lines.append(f"| {cell(row['id'])} | {' | '.join(values)} |")
        lines.extend(["", "## Recorded environment", ""])
        for row in report["runs"]:
            if row["summary"]:
                lines.append(f"- {cell(row['id'])}: {cell(row['summary']['metadata']['env'])}")
        if report["configuration_differences"]:
            lines.extend(["", "## Recorded configuration differences", "",
                          "| Run | Field | Reference | Value |", "| --- | --- | --- | --- |"])
            for row in report["configuration_differences"]:
                lines.append(f"| {cell(row['run'])} | {cell(row['field'])} | {cell(row['reference'])} | {cell(row['value'])} |")
    lines.extend(["", *[f"- {cell(note)}" for note in [*report["warnings"], *report["notes"]]]])
    return "\n".join(lines) + "\n"


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Read and compare native aps5-bench/1 captures offline.")
    commands = parser.add_subparsers(dest="command", required=True)
    summarize = commands.add_parser("summarize", help="summarize selected complete retained intervals")
    summarize.add_argument("capture", type=Path)
    summarize.add_argument("--intervals", type=parse_benchmark_selection, metavar="START:COUNT")
    summarize.add_argument("--json", action="store_true")
    compare = commands.add_parser("compare", help="compare an ordered aps5-bench-series/1 manifest")
    compare.add_argument("series", type=Path)
    compare.add_argument("--json", action="store_true")
    compare.add_argument("--require-matching-context", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.command == "summarize":
            report = summarize_benchmark(load_benchmark(args.capture), args.intervals)
        else:
            report = compare_benchmark_runs(args.series, args.require_matching_context)
        output = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n" if args.json else benchmark_report(report)
    except (ValueError, OSError, EOFError, RecursionError, OverflowError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
