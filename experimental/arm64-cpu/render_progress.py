#!/usr/bin/env python3
"""Render an upstream-inspired treemap of the experimental CPU feature inventory."""
import argparse
import html
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent
COLORS = {"implemented": "#37b84e", "in_progress": "#e4b34b", "planned": "#666971"}
SHORT = {
    "CPU foundations": "CPU",
    "Instruction decoding": "Decode",
    "Platform integration": "Platform",
}

def render(data):
    categories = data["categories"]
    all_items = [item for cat in categories for item in cat["items"]]
    done = sum(state == "implemented" for _, state in all_items)
    total = len(all_items)
    pct = 100 * done / total if total else 0
    milestone_id = data["milestone"].split(" ", 1)[0]
    width, height = 1200, 570
    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" role="img" aria-label="AnyPS5 iOS ARM64 feature treemap">',
        '<rect width="1200" height="570" fill="#0d1117"/>',
        '<style>text{font-family:Arial,Helvetica,sans-serif}.heading{fill:#e6edf3;font-size:35px;font-weight:700}.body{fill:#e6edf3;font-size:18px}.muted{fill:#9da7b2;font-size:15px}.cell{fill:#10151d;font-size:12px;font-weight:600}</style>',
        '<text class="heading" x="36" y="60">Status — iOS / ARM64</text>',
        '<path d="M36 81 H1164" stroke="#30363d"/>',
        '<rect x="36" y="111" width="155" height="48" fill="#3d4249"/>',
        '<text class="body" x="55" y="142">CPU features*</text>',
        '<rect x="191" y="111" width="123" height="48" fill="#56c900"/>',
        f'<text class="body" x="209" y="142">{pct:.1f}%</text>',
        '<rect x="327" y="111" width="166" height="48" fill="#3d4249"/>',
        '<text class="body" x="343" y="142">Milestone</text>',
        '<rect x="493" y="111" width="108" height="48" fill="#56c900"/>',
        f'<text class="body" x="512" y="142">{html.escape(milestone_id)}</text>',
        f'<text class="body" x="36" y="204">Experimental CPU feature inventory: {pct:.1f}% ({done}/{total})</text>',
        '<text class="muted" x="36" y="229">Green = implemented · Grey = planned · Amber = in progress</text>',
    ]
    # Three category blocks use dense, small cells and category headings,
    # mirroring the original status graphic without implying upstream parity.
    x0, y0, gap, block_w, block_h = 36, 246, 12, 368, 264
    for ci, cat in enumerate(categories):
        x = x0 + ci * (block_w + gap)
        lines.append(f'<rect x="{x}" y="{y0}" width="{block_w}" height="{block_h}" fill="#242a31" stroke="#51565d"/>')
        lines.append(f'<rect x="{x}" y="{y0}" width="{block_w}" height="31" fill="#333941"/>')
        lines.append(f'<text class="body" x="{x+10}" y="{y0+22}">{html.escape(cat["name"])}</text>')
        items = cat["items"]
        cols = 4
        rows = (len(items) + cols - 1) // cols
        cell_gap = 3
        cell_w = (block_w - 16 - (cols-1)*cell_gap) / cols
        cell_h = (block_h - 47 - (rows-1)*cell_gap) / rows
        for i, (label, state) in enumerate(items):
            cx = x + 8 + (i % cols) * (cell_w + cell_gap)
            cy = y0 + 39 + (i // cols) * (cell_h + cell_gap)
            lines.append(f'<rect x="{cx:.1f}" y="{cy:.1f}" width="{cell_w:.1f}" height="{cell_h:.1f}" fill="{COLORS[state]}" stroke="#171c23" stroke-width="1"/>')
            # Wrapped short labels in two or three lines for narrow treemap cells.
            words = label.split()
            wrapped, current = [], ""
            for word in words:
                candidate = (current + " " + word).strip()
                if len(candidate) > 13 and current:
                    wrapped.append(current)
                    current = word
                else:
                    current = candidate
            if current:
                wrapped.append(current)
            for line_no, chunk in enumerate(wrapped[:4]):
                lines.append(f'<text class="cell" x="{cx+5:.1f}" y="{cy+17+line_no*15:.1f}">{html.escape(chunk)}</text>')
    lines.extend([
        '<text class="muted" x="36" y="545">*Percentage covers only the listed experimental features, NOT total PS5 emulator completion or game compatibility.</text>',
        f'<text class="muted" x="1020" y="545">{html.escape(data["updated"])}</text>',
        '</svg>'
    ])
    return "\n".join(lines) + "\n"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    svg = render(json.loads((ROOT / "progress.json").read_text(encoding="utf-8")))
    path = ROOT / "progress.svg"
    if args.check:
        if not path.exists():
            parser.error("progress.svg missing: run python3 render_progress.py")
        actual = path.read_text(encoding="utf-8")
        try:
            ET.fromstring(actual)
        except ET.ParseError as exc:
            parser.error(f"progress.svg is invalid XML: {exc}")
        data = json.loads((ROOT / "progress.json").read_text(encoding="utf-8"))
        entries = [entry for category in data["categories"] for entry in category["items"]]
        completed = sum(status == "implemented" for _, status in entries)
        expected = f"{100 * completed / len(entries):.1f}%" if entries else "0.0%"
        milestone = data["milestone"].split(" ", 1)[0]
        if (f">{expected}<" not in actual or
                f"({completed}/{len(entries)})" not in actual or
                f">{milestone}<" not in actual):
            parser.error("progress.svg summary is stale: run python3 render_progress.py")
    else:
        path.write_text(svg, encoding="utf-8")
