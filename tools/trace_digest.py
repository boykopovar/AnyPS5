import argparse
import bisect
import collections
import gzip
import html
import json
import math
import sys
import zlib
from pathlib import Path

DESCRIPTION = """Digest of an APS5_TRACE_FRAMES / APS5_TRACE_SECONDS capture (aps5-trace-*.json.gz) for an issue or a
pull request: the summary line, the slowest frames with their largest slices, the busy and wait time of each
thread, the largest slices per frame, the longest waits, the largest GPU batches with the CPU slices that
recorded them (when the trace carries batch ids) and, with --stacks, the hottest functions of an
APS5_SAMPLE_WORKER collapsed-stack file. The summary line is read from aps5-summary.jsonl next to the trace
unless --summary names another file.

A summary file written by APS5_TRACE_SUMMARY_EVERY (aps5-summary.jsonl, or any .jsonl) is digested as a table with
one row per interval and a text sparkline of the frame p99 and of the GPU busy time per present; the lines of
window captures in the same file are skipped."""

RAMP = "._:-=+*#%@"


def is_wait(name):
    return "wait" in name.lower()


def _event_value(value, label, index):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"trace event {index}: {label} must be a finite number")
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError):
        raise ValueError(f"trace event {index}: {label} must be a finite number") from None
    if not math.isfinite(number) or abs(number) >= 1e300:
        raise ValueError(f"trace event {index}: {label} must be a finite number")
    return number


def _event_number(event, key, index):
    return _event_value(event.get(key), key, index)


def _event_tid(event, index):
    tid = event.get("tid")
    if isinstance(tid, bool) or not isinstance(tid, int):
        raise ValueError(f"trace event {index}: tid must be an integer")
    return tid


def _event_args(event, index):
    args = event.get("args", {})
    if not isinstance(args, dict):
        raise ValueError(f"trace event {index}: args must be an object")
    return args


class Trace:
    def __init__(self, document):
        self.threads = {}
        self.slices = collections.defaultdict(list)
        self.presents = []
        self.folded = collections.defaultdict(float)
        self.batched = []
        if not isinstance(document, (list, dict)):
            raise ValueError("trace must be an event array or object")
        events = document if isinstance(document, list) else document.get("traceEvents", [])
        if not isinstance(events, list):
            raise ValueError("traceEvents must be an array")
        for index, event in enumerate(events):
            if not isinstance(event, dict):
                raise ValueError(f"trace event {index} must be an object")
            phase = event.get("ph")
            if phase == "M" and event.get("name") == "thread_name":
                tid = _event_tid(event, index)
                name = _event_args(event, index).get("name")
                if not isinstance(name, str):
                    raise ValueError(f"trace event {index}: args.name must be a string")
                self.threads[tid] = name
            elif phase == "X":
                tid = _event_tid(event, index)
                name = event.get("name")
                if not isinstance(name, str):
                    raise ValueError(f"trace event {index}: name must be a string")
                ts = _event_number(event, "ts", index)
                dur = _event_number(event, "dur", index)
                self.slices[tid].append([ts, dur, name, 0, False])
                args = _event_args(event, index)
                if "batch" in args:
                    batch = args["batch"]
                    if isinstance(batch, bool) or not isinstance(batch, (int, str)):
                        raise ValueError(f"trace event {index}: args.batch must be an integer or a string")
                    self.batched.append((tid, ts, dur, name, batch))
            elif phase == "i" and event.get("name") == "present":
                self.presents.append((_event_number(event, "ts", index), _event_args(event, index).get("present")))
            elif phase == "C":
                tid = _event_tid(event, index)
                self.folded[tid] += sum(_event_value(value, "args value", index) for value in _event_args(event, index).values())
        self.presents.sort()
        for slices in self.slices.values():
            slices.sort(key=lambda entry: (entry[0], -entry[1]))
            stack = []
            for entry in slices:
                start = round(entry[0] * 1000)
                while stack and start >= round(stack[-1][0] * 1000) + round(stack[-1][1] * 1000):
                    stack.pop()
                entry[3] = len(stack)
                entry[4] = any(is_wait(parent[2]) for parent in stack)
                stack.append(entry)

    def select_threads(self, names):
        available = self.threads.keys() | self.slices.keys() | self.folded.keys()
        unknown = set(names) - {self.name(tid) for tid in available}
        if unknown:
            raise ValueError("unknown thread: " + ", ".join(sorted(unknown)))
        selected = {tid for tid in available if self.name(tid) in names}
        self.threads = {tid: name for tid, name in self.threads.items() if tid in selected}
        self.slices = {tid: slices for tid, slices in self.slices.items() if tid in selected}
        self.folded = {tid: value for tid, value in self.folded.items() if tid in selected}
        self.batched = [entry for entry in self.batched if entry[0] in selected]

    def name(self, tid):
        return self.threads.get(tid, f"tid {tid}")

    def frames(self, end):
        starts = [ts for ts, _ in self.presents]
        if end is None:
            end = starts[-1] if starts else 0.0
        bounds = starts + ([end] if not starts or end > starts[-1] else [])
        return [(index, bounds[index], bounds[index + 1], self.presents[index][1]) for index in range(len(bounds) - 1)]


def load_trace(path):
    opener = gzip.open if str(path).endswith(".gz") else open
    with opener(path, "rt", encoding="utf-8-sig") as handle:
        try:
            document = json.load(handle)
        except RecursionError as error:
            raise ValueError(f"{path}: trace JSON nesting is too deep: {error}") from error
        return Trace(document)


def load_summary_lines(path, numbered=False):
    records = []
    for number, line in enumerate(Path(path).read_text(encoding="utf-8-sig").split("\n"), 1):
        if not line.strip():
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(f"{path}:{number}: invalid JSON: {error.msg}") from error
        if not isinstance(record, dict):
            raise ValueError(f"{path}:{number}: summary record must be an object")
        records.append((number, record) if numbered else record)
    return records


def load_summary(path, trace_path):
    candidate = Path(path) if path else Path(trace_path).with_name("aps5-summary.jsonl")
    if not candidate.exists():
        return None
    lines = load_summary_lines(candidate)
    name = Path(trace_path).name
    alternate = name[:-3] if name.endswith(".gz") else name + ".gz"
    for filename in (name, alternate):
        for line in reversed(lines):
            if line.get("trace") == filename:
                return line
    return None


def load_stacks(path):
    stacks = []
    for number, line in enumerate(Path(path).read_text(encoding="utf-8-sig").splitlines(), 1):
        if line.startswith("#") or not line.strip():
            continue
        frames, _, count = line.rpartition(" ")
        try:
            weight = int(count)
        except ValueError as error:
            raise ValueError(f"{path}:{number}: sample count must be a positive integer") from error
        if weight <= 0 or any(not frame.strip() for frame in frames.split(";")):
            raise ValueError(f"{path}:{number}: expected nonempty frames and a positive sample count")
        stacks.append((frames.split(";"), weight))
    return stacks


def digest(trace, summary=None, stacks=None, worst=5, top=10):
    window_ms = summary["window"]["ms"] if summary else None
    frames = trace.frames(window_ms * 1000.0 if window_ms is not None else None)
    span_us = frames[-1][2] - frames[0][1] if frames else 0.0
    count = max(len(frames), 1)
    result = {"summary": summary, "frames": len(frames), "window_ms": span_us / 1000.0}

    slowest = []
    for index, begin, end, present in sorted(frames, key=lambda frame: frame[1] - frame[2])[:worst]:
        inside = collections.Counter()
        for tid, slices in trace.slices.items():
            for ts, dur, name, _, _ in slices:
                overlap = min(ts + dur, end) - max(ts, begin)
                if overlap > 0:
                    inside[(name, trace.name(tid))] += overlap
        slowest.append({"present": present, "ms": (end - begin) / 1000.0, "at_ms": begin / 1000.0,
                        "slices": [{"name": name, "thread": thread, "ms": us / 1000.0} for (name, thread), us in inside.most_common(4)]})
    result["slowest_frames"] = slowest

    threads = []
    totals = collections.Counter()
    waits = []
    for tid in sorted(trace.slices.keys() | trace.folded.keys()):
        slices = trace.slices.get(tid, [])
        traced = 0.0
        waited = 0.0
        leaders = collections.Counter()
        for ts, dur, name, depth, nested in slices:
            overlap = max(min(ts + dur, frames[-1][2]) - max(ts, frames[0][1]), 0.0) if frames else dur
            if overlap > 0:
                totals[name] += overlap
                if depth == 0:
                    traced += overlap
                    if not is_wait(name):
                        leaders[name] += overlap
                if is_wait(name) and not nested:
                    waited += overlap
            if is_wait(name) and not nested and (overlap > 0 or not frames):
                waits.append((overlap, name, trace.name(tid), max(ts, frames[0][1]) if frames else ts))
        threads.append({"thread": trace.name(tid), "busy_ms_per_frame": (traced - waited) / 1000.0 / count,
                        "wait_ms_per_frame": waited / 1000.0 / count,
                        "untraced_ms_per_frame": max(span_us - traced, 0.0) / 1000.0 / count,
                        "folded_ms_per_frame": trace.folded.get(tid, 0.0) / count,
                        "busy_pct": 100.0 * (traced - waited) / span_us if span_us else 0.0,
                        "wait_pct": 100.0 * waited / span_us if span_us else 0.0,
                        "top": [{"name": name, "ms_per_frame": us / 1000.0 / count} for name, us in leaders.most_common(2)]})
    result["threads"] = threads
    result["top_slices_ms_per_frame"] = [{"name": name, "ms": us / 1000.0 / count} for name, us in totals.most_common(top)]
    starts = [ts for ts, _ in trace.presents]
    result["longest_waits"] = [{"name": name, "thread": thread, "ms": dur / 1000.0, "at_ms": ts / 1000.0,
                                "present": trace.presents[bisect.bisect_right(starts, ts) - 1][1] if starts and ts >= starts[0] else None}
                               for dur, name, thread, ts in sorted(waits, reverse=True)[:top]]

    gpu = {tid for tid, name in trace.threads.items() if name == "gpu"}
    batches = [entry for entry in trace.batched if entry[0] in gpu and entry[3] == "batch"]
    if batches:
        ranges = collections.defaultdict(collections.Counter)
        recorded = collections.defaultdict(collections.Counter)
        for tid, ts, dur, name, batch in trace.batched:
            if tid in gpu:
                if name != "batch":
                    ranges[batch][name] += dur
            else:
                recorded[batch][(name, trace.name(tid))] += dur
        result["largest_gpu_batches"] = [{"batch": batch, "ms": dur / 1000.0, "at_ms": ts / 1000.0,
                                          "present": trace.presents[bisect.bisect_right(starts, ts) - 1][1] if starts and ts >= starts[0] else None,
                                          "gpu_ranges": [{"name": name, "ms": us / 1000.0} for name, us in ranges[batch].most_common(3)],
                                          "recorded_by": [{"name": name, "thread": thread, "ms": us / 1000.0} for (name, thread), us in recorded[batch].most_common(4)]}
                                         for _, ts, dur, _, batch in sorted(batches, key=lambda entry: -entry[2])[:worst]]

    if stacks is not None:
        samples = sum(count for _, count in stacks)
        own = collections.Counter()
        inclusive = collections.Counter()
        truncated = 0
        common = None
        for frames_of, number in stacks:
            if frames_of[0] == "[truncated]":
                truncated += number
                frames_of = frames_of[1:]
            elif frames_of:
                common = set(frames_of) if common is None else common & set(frames_of)
            if not frames_of:
                continue
            own[frames_of[-1]] += number
            for frame in set(frames_of):
                inclusive[frame] += number
        common = common or set()
        for frame in common:
            del inclusive[frame]
        share = lambda value: 100.0 * value / samples if samples else 0.0
        result["stacks"] = {"samples": samples, "truncated_pct": share(truncated), "common_frames": len(common),
                            "self": [{"frame": frame, "pct": share(value)} for frame, value in own.most_common(top)],
                            "total": [{"frame": frame, "pct": share(value)} for frame, value in inclusive.most_common(top)]}
    return result


def table_cell(value):
    return html.escape(str(value)).replace("|", "&#124;").replace("\r\n", "\n").replace("\r", "\n").replace("\n", "<br>")


def markdown(result):
    lines = []
    if result.get("thread_filter"):
        lines += ["Thread filter: " + ", ".join(table_cell(name) for name in result["thread_filter"])
                  + ". Frame bounds, summary and stack samples remain capture-wide.", ""]
    summary = result["summary"]
    if summary:
        frame = summary["frame_ms"]
        lines.append(f"**{summary['window']['spec']}** from present {summary['window']['first_present']}: {summary['presents']} presents, "
                     f"frame p50 {frame['p50']:.1f} / p90 {frame['p90']:.1f} / p99 {frame['p99']:.1f} / max {frame['max']:.1f} ms, "
                     f"GPU busy {summary['gpu_busy_ms_per_present']:.1f} ms per present")
        lines.append(f"{summary['gpu']}, {summary['driver']}, {summary['os']}, {summary['resolution']}, commit {summary['commit']}"
                     f"{' (dirty)' if summary.get('dirty') else ''}, dropped: {'yes' if summary['trace_dropped'] else 'no'}")
        lines.append("")
        lines.append("```json")
        lines.append(json.dumps(summary, separators=(",", ":")))
        lines.append("```")
    else:
        lines.append(f"{result['frames']} frames over {result['window_ms']:.1f} ms (no summary line found)")
    lines.append("")
    lines.append("**Slowest frames** (largest slices inside each, inclusive)")
    lines.append("")
    for frame in result["slowest_frames"]:
        parts = ", ".join(f"{entry['name']} ({entry['thread']}) {entry['ms']:.1f}" for entry in frame["slices"])
        lines.append(f"- present {frame['present']}: {frame['ms']:.1f} ms at {frame['at_ms']:.0f} ms: {parts}")
    lines.append("")
    lines.append("**Threads** (ms per frame)")
    lines.append("")
    lines.append("| thread | busy | wait | untraced | in slices < 20 us | largest |")
    lines.append("|---|---|---|---|---|---|")
    for thread in result["threads"]:
        largest = ", ".join(f"{entry['name']} {entry['ms_per_frame']:.1f}" for entry in thread["top"])
        lines.append(f"| {table_cell(thread['thread'])} | {thread['busy_ms_per_frame']:.1f} ({thread['busy_pct']:.0f} %) | {thread['wait_ms_per_frame']:.1f} ({thread['wait_pct']:.0f} %) "
                     f"| {thread['untraced_ms_per_frame']:.1f} | {thread['folded_ms_per_frame']:.1f} | {table_cell(largest)} |")
    lines.append("")
    lines.append("**Largest slices** (ms per frame, inclusive, all threads): " + ", ".join(f"{entry['name']} {entry['ms']:.1f}" for entry in result["top_slices_ms_per_frame"]))
    lines.append("")
    lines.append("**Longest waits**")
    lines.append("")
    for wait in result["longest_waits"]:
        lines.append(f"- {wait['name']} ({wait['thread']}) {wait['ms']:.1f} ms at {wait['at_ms']:.0f} ms, present {wait['present']}")
    if "largest_gpu_batches" in result:
        lines.append("")
        lines.append("**Largest GPU batches** (GPU ranges inside, and the CPU slices that recorded each, inclusive)")
        lines.append("")
        for batch in result["largest_gpu_batches"]:
            ranges = ", ".join(f"{entry['name']} {entry['ms']:.1f}" for entry in batch["gpu_ranges"]) or "no ranges"
            recorded = ", ".join(f"{entry['name']} ({entry['thread']}) {entry['ms']:.1f}" for entry in batch["recorded_by"]) or "no CPU slices"
            lines.append(f"- batch {batch['batch']}: {batch['ms']:.1f} ms at {batch['at_ms']:.0f} ms, present {batch['present']}: GPU {ranges}; recorded by {recorded}")
    if "stacks" in result:
        stacks = result["stacks"]
        lines.append("")
        lines.append(f"**Worker stacks** ({stacks['samples']} samples, {stacks['truncated_pct']:.1f} % truncated; total leaves out the {stacks['common_frames']} frames every complete stack shares)")
        lines.append("")
        lines.append("- self: " + ", ".join(f"{entry['frame']} {entry['pct']:.1f} %" for entry in stacks["self"]))
        lines.append("- total: " + ", ".join(f"{entry['frame']} {entry['pct']:.1f} %" for entry in stacks["total"]))
    return "\n".join(lines) + "\n"


def load_intervals(path):
    lines = []
    for number, record in load_summary_lines(path, numbered=True):
        if "interval_start_s" not in record:
            continue
        try:
            validate_interval(record)
        except ValueError as error:
            raise ValueError(f"{path}:{number}: {error}") from error
        lines.append(record)
    return lines


def interval_number(value, field):
    try:
        valid = type(value) in (int, float) and math.isfinite(value) and value >= 0
    except OverflowError:
        valid = False
    if not valid:
        raise ValueError(f"{field} must be a finite non-negative number")
    return value


def validate_interval(record):
    start = interval_number(record.get("interval_start_s"), "interval_start_s")
    end = interval_number(record.get("interval_end_s"), "interval_end_s")
    if end < start:
        raise ValueError("interval_end_s must not precede interval_start_s")
    if type(record.get("presents")) is not int or record["presents"] < 0:
        raise ValueError("presents must be a non-negative integer")
    frames = record.get("frame_ms")
    if not isinstance(frames, dict):
        raise ValueError("frame_ms must be an object")
    for field in ("p50", "p90", "p99", "max"):
        interval_number(frames.get(field), f"frame_ms.{field}")
    interval_number(record.get("gpu_busy_ms_per_present"), "gpu_busy_ms_per_present")
    for field in ("worker_busy_pct", "top_phases_ms_per_frame"):
        values = record.get(field, {})
        if not isinstance(values, dict):
            raise ValueError(f"{field} must be an object")
        for name, value in values.items():
            interval_number(value, f"{field}.{name}")
    if "trace_dropped" in record and type(record["trace_dropped"]) is not bool:
        raise ValueError("trace_dropped must be a boolean")


def sparkline(values):
    high = max(values, default=0.0)
    if high <= 0:
        return RAMP[0] * len(values)
    return "".join(RAMP[min(len(RAMP) - 1, int(value / high * (len(RAMP) - 1) + 0.5))] for value in values)


def digest_intervals(lines):
    rows = []
    for line in lines:
        workers = line.get("worker_busy_pct", {})
        busiest = max(workers.items(), key=lambda entry: entry[1]) if workers else None
        phases = line.get("top_phases_ms_per_frame", {})
        largest = max(phases.items(), key=lambda entry: entry[1]) if phases else None
        rows.append({"start_s": line["interval_start_s"], "end_s": line["interval_end_s"], "presents": line["presents"],
                     "frame_ms": line["frame_ms"], "gpu_busy_ms_per_present": line["gpu_busy_ms_per_present"],
                     "busiest_worker": {"thread": busiest[0], "pct": busiest[1]} if busiest else None,
                     "top_phase": {"name": largest[0], "ms_per_frame": largest[1]} if largest else None,
                     "trace_dropped": line.get("trace_dropped", False)})
    first = lines[0] if lines else {}
    keys = ("commit", "dirty", "env", "gpu", "driver", "os", "resolution",
            "cpu", "cpu_threads", "memory_mib", "title_id", "title", "content_version")
    changed = [index for index in range(1, len(lines)) if any(lines[index].get(key) != lines[index - 1].get(key) for key in keys)]
    return {"intervals": rows, "identity": {key: first.get(key) for key in keys}, "identity_changed_at": changed}


def markdown_intervals(result):
    rows = result["intervals"]
    identity = result["identity"]
    if not rows:
        return "no interval lines (interval_start_s) found\n"
    lines = [f"**{len(rows)} intervals**, {rows[0]['start_s']:.1f} to {rows[-1]['end_s']:.1f} s, {sum(row['presents'] for row in rows)} presents",
             f"{identity['gpu']}, {identity['driver']}, {identity['os']}, {identity['resolution']}, commit {identity['commit']}"
             f"{' (dirty)' if identity.get('dirty') else ''}"]
    if result["identity_changed_at"]:
        lines.append("build, environment, host, resolution or title metadata changed at interval " + ", ".join(str(index + 1) for index in result["identity_changed_at"]))
    lines += ["", "| # | seconds | presents | p50 | p90 | p99 | max | GPU busy | busiest worker | largest phase |", "|---|---|---|---|---|---|---|---|---|---|"]
    for index, row in enumerate(rows, 1):
        frame = row["frame_ms"]
        worker = f"{row['busiest_worker']['pct']:.0f} %" if row["busiest_worker"] else "-"
        phase = f"{row['top_phase']['name']} {row['top_phase']['ms_per_frame']:.1f}" if row["top_phase"] else "-"
        lines.append(f"| {index} | {row['start_s']:.0f}-{row['end_s']:.0f}{' (dropped)' if row['trace_dropped'] else ''} | {row['presents']} | {frame['p50']:.1f} | {frame['p90']:.1f} "
                     f"| {frame['p99']:.1f} | {frame['max']:.1f} | {row['gpu_busy_ms_per_present']:.1f} | {worker} | {table_cell(phase)} |")
    p99 = [row["frame_ms"]["p99"] for row in rows]
    gpu = [row["gpu_busy_ms_per_present"] for row in rows]
    lines += ["", "```", f"p99 ms  {sparkline(p99)}  0-{max(p99):.1f}", f"gpu ms  {sparkline(gpu)}  0-{max(gpu):.1f}", "```"]
    return "\n".join(lines) + "\n"


def nonnegative_integer(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    return number


def main(arguments=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("trace", help="aps5-trace-*.json.gz (or .json), or a .jsonl summary file with APS5_TRACE_SUMMARY_EVERY lines")
    parser.add_argument("--summary", help="file with the summary line (default: aps5-summary.jsonl next to the trace)")
    parser.add_argument("--stacks", help="collapsed-stack file of APS5_SAMPLE_WORKER (<file stem>.collapsed.txt)")
    parser.add_argument("--thread", action="append", default=[], metavar="NAME",
                        help="include event tracks with this exact thread name; repeat to select several tracks")
    parser.add_argument("--frames", type=nonnegative_integer, default=5, help="slowest frames to list (default 5)")
    parser.add_argument("--top", type=nonnegative_integer, default=10, help="entries per list (default 10)")
    parser.add_argument("--json", action="store_true", help="print the digest as JSON")
    options = parser.parse_args(arguments)
    if options.summary and not Path(options.summary).is_file():
        parser.error(f"summary file not found: {options.summary}")
    if options.thread and options.trace.endswith(".jsonl"):
        parser.error("--thread requires an event trace, not interval summaries")
    if options.trace.endswith(".jsonl"):
        result = digest_intervals(load_intervals(options.trace))
        sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else markdown_intervals(result))
        return 0
    try:
        stacks = load_stacks(options.stacks) if options.stacks else None
    except (OSError, ValueError) as error:
        parser.error(str(error))
    trace = load_trace(options.trace)
    if options.thread:
        trace.select_threads(options.thread)
    result = digest(trace, load_summary(options.summary, options.trace),
                    stacks, options.frames, options.top)
    if options.thread:
        result["thread_filter"] = sorted(set(options.thread))
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else markdown(result))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (EOFError, OSError, ValueError, zlib.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(2)
