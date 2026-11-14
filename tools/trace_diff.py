import argparse
import json
import sys
import zlib
from pathlib import Path

import trace_digest

DESCRIPTION = """Compares two APS5_TRACE_FRAMES / APS5_TRACE_SECONDS captures (aps5-trace-*.json.gz) per frame: the busy
and wait time of each thread, and the largest slices of either capture. A thread or slice present in one capture
counts as zero in the other. Times are per frame, so captures with different frame counts compare directly.

The deltas are B minus A. A capture's digest is built by trace_digest.py, so the same caveats apply: buffer-full
losses make the slice timings describe retained events only."""


def compare(a_path, b_path, top=10):
    a = trace_digest.digest(trace_digest.load_trace(a_path), None, None, 0, top)
    b = trace_digest.digest(trace_digest.load_trace(b_path), None, None, 0, top)

    a_threads = {row["thread"]: row for row in a["threads"]}
    b_threads = {row["thread"]: row for row in b["threads"]}
    threads = []
    for name in sorted(a_threads.keys() | b_threads.keys()):
        left = a_threads.get(name, {})
        right = b_threads.get(name, {})
        busy_a = left.get("busy_ms_per_frame", 0.0)
        busy_b = right.get("busy_ms_per_frame", 0.0)
        wait_a = left.get("wait_ms_per_frame", 0.0)
        wait_b = right.get("wait_ms_per_frame", 0.0)
        threads.append({"thread": name, "a_busy_ms_per_frame": busy_a, "b_busy_ms_per_frame": busy_b,
                        "busy_delta_ms_per_frame": busy_b - busy_a,
                        "a_wait_ms_per_frame": wait_a, "b_wait_ms_per_frame": wait_b,
                        "wait_delta_ms_per_frame": wait_b - wait_a})

    a_slices = {row["name"]: row["ms"] for row in a["top_slices_ms_per_frame"]}
    b_slices = {row["name"]: row["ms"] for row in b["top_slices_ms_per_frame"]}
    slices = []
    for name in a_slices.keys() | b_slices.keys():
        slice_a = a_slices.get(name, 0.0)
        slice_b = b_slices.get(name, 0.0)
        slices.append({"name": name, "a_ms_per_frame": slice_a, "b_ms_per_frame": slice_b, "delta_ms_per_frame": slice_b - slice_a})
    slices.sort(key=lambda row: (-abs(row["delta_ms_per_frame"]), row["name"]))

    return {"a": str(a_path), "b": str(b_path), "frames": {"a": a["frames"], "b": b["frames"]},
            "threads": threads, "slices": slices[:top]}


def markdown(result):
    lines = [f"Frames: A {result['frames']['a']}, B {result['frames']['b']}", "",
             "| thread | A busy ms/frame | B busy ms/frame | delta busy | A wait ms/frame | B wait ms/frame | delta wait |",
             "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for row in result["threads"]:
        lines.append(f"| {row['thread']} | {row['a_busy_ms_per_frame']:.3f} | {row['b_busy_ms_per_frame']:.3f} | {row['busy_delta_ms_per_frame']:+.3f} | "
                     f"{row['a_wait_ms_per_frame']:.3f} | {row['b_wait_ms_per_frame']:.3f} | {row['wait_delta_ms_per_frame']:+.3f} |")
    lines += ["", "| slice | A ms/frame | B ms/frame | delta |", "| --- | ---: | ---: | ---: |"]
    for row in result["slices"]:
        lines.append(f"| {row['name']} | {row['a_ms_per_frame']:.3f} | {row['b_ms_per_frame']:.3f} | {row['delta_ms_per_frame']:+.3f} |")
    return "\n".join(lines) + "\n"


def nonnegative_integer(value):
    try:
        number = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    if number < 0:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    return number


def main(arguments=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("a", type=Path, help="the baseline capture (aps5-trace-*.json.gz or .json)")
    parser.add_argument("b", type=Path, help="the capture to compare with the baseline")
    parser.add_argument("--top", type=nonnegative_integer, default=10, help="slices to list (default 10)")
    parser.add_argument("--json", action="store_true", help="print the comparison as JSON")
    options = parser.parse_args(arguments)
    result = compare(options.a, options.b, options.top)
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else markdown(result))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (EOFError, OSError, ValueError, zlib.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(2)
