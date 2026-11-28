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
losses make the slice timings describe retained events only. Use --a-frames or --b-frames to select the matching
scene in each capture. Both captures need at least one complete frame between recorded presents."""


class FrameWindowError(ValueError):
    pass


def select_scene_presents(trace, path, window):
    start, count = window
    if start < 0 or count <= 0:
        raise FrameWindowError(f"{path}: a frame window needs a nonnegative start and a positive count")
    end = start + count
    available = max(0, len(trace.presents) - 1)
    if end > available:
        raise FrameWindowError(f"{path}: frame window {start}:{count} exceeds the complete frame count of {available}")
    selected = trace.presents[start:end + 1]
    if any(selected[index - 1][0] >= selected[index][0] for index in range(1, len(selected))):
        raise FrameWindowError(f"{path}: selected frames need strictly increasing present timestamps")
    trace.presents = selected
    return {"start": start, "count": count, "start_ms": selected[0][0] / 1000, "end_ms": selected[-1][0] / 1000}


def digest_capture(path, window):
    trace = trace_digest.load_trace(path)
    selected = select_scene_presents(trace, path, window) if window is not None else None
    return trace_digest.digest(trace, None, None, 0, None), selected


def compare(a_path, b_path, top=10, *, a_frames=None, b_frames=None):
    a, a_window = digest_capture(a_path, a_frames)
    b, b_window = digest_capture(b_path, b_frames)
    windowed = a_window is not None or b_window is not None
    for path, digest in ((a_path, a), (b_path, b)):
        if not digest["frames"]:
            raise FrameWindowError(f"{path}: comparison needs at least one complete frame in each capture")

    a_threads = thread_timings(a["threads"])
    b_threads = thread_timings(b["threads"])
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

    result = {"a": str(a_path), "b": str(b_path), "frames": {"a": a["frames"], "b": b["frames"]},
              "threads": threads, "slices": slices[:top]}
    if windowed:
        result["windows"] = {"a": a_window, "b": b_window}
    event_loss = {"a": a.get("event_loss", []), "b": b.get("event_loss", [])}
    if windowed or any(event_loss.values()):
        result["event_loss"] = event_loss
    return result


def thread_timings(rows):
    timings = {}
    for row in rows:
        totals = timings.setdefault(row["thread"], {"busy_ms_per_frame": 0.0, "wait_ms_per_frame": 0.0})
        totals["busy_ms_per_frame"] += row["busy_ms_per_frame"]
        totals["wait_ms_per_frame"] += row["wait_ms_per_frame"]
    return timings


def markdown(result):
    lines = [f"Frames: A {result['frames']['a']}, B {result['frames']['b']}"]
    windows = result.get("windows", {})
    event_loss = result.get("event_loss", {})
    if windows or any(event_loss.values()):
        lines.append("")
        for side in ("a", "b"):
            if side in windows:
                window = windows[side]
                description = (f"{window['start']}:{window['count']} ({window['start_ms']:g} to {window['end_ms']:g} ms in the capture)"
                               if window is not None else "whole capture")
                lines.append(f"- {side.upper()} window: {description}")
            elif event_loss.get(side):
                lines.append(f"- {side.upper()} capture:")
            for loss in event_loss.get(side, []):
                name = trace_digest.markdown_text(loss["thread"])
                lines.append(f"  - {name}: {loss['events_lost']} events lost; buffer full at {loss['at_ms']:g} ms in the capture.")
        if any(event_loss.values()):
            lines += ["", "Timings describe retained events only. Loss markers refer to the original captures."]
    lines += ["", "| thread | A busy ms/frame | B busy ms/frame | delta busy | A wait ms/frame | B wait ms/frame | delta wait |",
              "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for row in result["threads"]:
        name = trace_digest.table_cell(row["thread"])
        lines.append(f"| {name} | {row['a_busy_ms_per_frame']:.3f} | {row['b_busy_ms_per_frame']:.3f} | {row['busy_delta_ms_per_frame']:+.3f} | "
                     f"{row['a_wait_ms_per_frame']:.3f} | {row['b_wait_ms_per_frame']:.3f} | {row['wait_delta_ms_per_frame']:+.3f} |")
    lines += ["", "| slice | A ms/frame | B ms/frame | delta |", "| --- | ---: | ---: | ---: |"]
    for row in result["slices"]:
        name = trace_digest.table_cell(row["name"])
        lines.append(f"| {name} | {row['a_ms_per_frame']:.3f} | {row['b_ms_per_frame']:.3f} | {row['delta_ms_per_frame']:+.3f} |")
    return "\n".join(lines) + "\n"


def nonnegative_integer(value):
    try:
        number = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    if number < 0:
        raise argparse.ArgumentTypeError("must be a nonnegative integer")
    return number


def frame_window(value):
    try:
        start, count = map(int, value.split(":"))
    except ValueError:
        raise argparse.ArgumentTypeError("must be START:COUNT") from None
    if start < 0 or count <= 0:
        raise argparse.ArgumentTypeError("needs a nonnegative start and a positive count")
    return start, count


def main(arguments=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("a", type=Path, help="the baseline capture (aps5-trace-*.json.gz or .json)")
    parser.add_argument("b", type=Path, help="the capture to compare with the baseline")
    parser.add_argument("--a-frames", type=frame_window, metavar="START:COUNT",
                        help="baseline scene window, starting at a zero-based complete frame (default: whole capture)")
    parser.add_argument("--b-frames", type=frame_window, metavar="START:COUNT",
                        help="candidate scene window, starting at a zero-based complete frame (default: whole capture)")
    parser.add_argument("--top", type=nonnegative_integer, default=10, help="slices to list (default 10)")
    parser.add_argument("--json", action="store_true", help="print the comparison as JSON")
    options = parser.parse_args(arguments)
    try:
        result = compare(options.a, options.b, options.top, a_frames=options.a_frames, b_frames=options.b_frames)
    except FrameWindowError as error:
        parser.error(str(error))
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else markdown(result))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (EOFError, OSError, ValueError, zlib.error) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(2)
