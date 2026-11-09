import argparse
import html
import sys
import zlib
from pathlib import Path

import trace_digest

DESCRIPTION = """Timeline of an APS5_TRACE_FRAMES / APS5_TRACE_SECONDS capture (aps5-trace-*.json.gz) as one standalone
HTML file: one row per thread, one bar per traced slice, placed on the capture's time axis. Hover a bar for its name and
duration. Slices are clipped to --from-ms and --to-ms. At most --max-slices bars are drawn, longest first, and the page
says how many were left out.

Slices are only what the capture retained; buffer-full losses (see trace_digest.py) leave gaps that look like idle time."""

LABEL_WIDTH = 120
ROW_HEIGHT = 18


def window_slices(trace, start_us, end_us, threads=None):
    rows = []
    for tid, slices in trace.slices.items():
        name = trace.name(tid)
        if threads and name not in threads:
            continue
        for ts, dur, slice_name, _, _ in slices:
            begin = max(ts, start_us)
            end = min(ts + dur, end_us)
            if end > begin:
                rows.append({"thread": name, "ts": begin, "dur": end - begin, "name": slice_name})
    return rows


def layout(slices, start_us, end_us, width_px, max_slices=5000):
    span = max(end_us - start_us, 1e-9)
    threads = sorted({slice_["thread"] for slice_ in slices})
    row_of = {name: row for row, name in enumerate(threads)}
    kept = sorted(slices, key=lambda slice_: -slice_["dur"])[:max_slices]
    bars = []
    for slice_ in kept:
        x = LABEL_WIDTH + (slice_["ts"] - start_us) / span * width_px
        w = max(slice_["dur"] / span * width_px, 1.0)
        bars.append({"thread": slice_["thread"], "name": slice_["name"], "ms": slice_["dur"] / 1000.0,
                     "x": x, "w": w, "y": row_of[slice_["thread"]] * ROW_HEIGHT})
    return {"threads": threads, "bars": bars, "left_out": len(slices) - len(kept), "width": width_px + LABEL_WIDTH}


def svg(laid):
    height = max(len(laid["threads"]), 1) * ROW_HEIGHT
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{laid["width"]}" height="{height}">']
    for row, name in enumerate(laid["threads"]):
        parts.append(f'<text x="2" y="{row * ROW_HEIGHT + 13}" font-size="11">{html.escape(name)}</text>')
    for bar in laid["bars"]:
        title = html.escape(f'{bar["name"]} ({bar["ms"]:.3f} ms)')
        parts.append(f'<rect x="{bar["x"]:.2f}" y="{bar["y"] + 2}" width="{bar["w"]:.2f}" height="{ROW_HEIGHT - 4}" fill="#4a7fc1"><title>{title}</title></rect>')
    parts.append("</svg>")
    return "".join(parts)


def page(svg_text, title, notes):
    note_items = "".join(f"<li>{html.escape(note)}</li>" for note in notes)
    return ("<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>" + html.escape(title) + "</title>"
            "<style>body{font-family:sans-serif;margin:16px}svg{display:block;background:#f6f6f6}</style></head><body>"
            "<h1>" + html.escape(title) + "</h1><ul>" + note_items + "</ul>" + svg_text + "</body></html>\n")


def nonnegative_number(value):
    try:
        number = float(value)
    except ValueError:
        raise argparse.ArgumentTypeError("must be a number")
    if number < 0:
        raise argparse.ArgumentTypeError("must be a number")
    return number


