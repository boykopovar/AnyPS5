import argparse
import io
import json
import subprocess
import tarfile
import tempfile
from datetime import date
from html import escape
from pathlib import Path

import progress

SERIES = (("libraries", "System library functions", "#2f81f7"), ("shaders", "GPU shader instructions", "#2ea043"))
WIDTH, LEFT, RIGHT, TITLE, PANEL, GAP, AXIS = 900, 56, 190, 34, 150, 16, 32
MAX_DOTS = 45
BACKGROUND, GRID, TEXT, MUTED = "#0d1117", "#30363d", "#c9d1d9", "#8b949e"


def git(*args):
    return subprocess.run(["git", "-C", str(progress.ROOT), *args], check=True, capture_output=True).stdout


def daily_commits(ref):
    seen = {}
    for line in git("log", "--first-parent", "--format=%H %cs", ref).decode().splitlines():
        sha, day = line.split()
        seen.setdefault(day, sha)
    return sorted(seen.items())


def measure(sha):
    paths = [progress.PRX.relative_to(progress.ROOT).as_posix(), progress.OPCODES.relative_to(progress.ROOT).as_posix()]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        try:
            archive = tarfile.open(fileobj=io.BytesIO(git("archive", sha, "--", *paths)))
            if hasattr(tarfile, "data_filter"):
                archive.extractall(root, filter="data")
            else:
                archive.extractall(root)
        except subprocess.CalledProcessError:
            return None
        saved = progress.PRX, progress.OPCODES
        progress.PRX, progress.OPCODES = root / paths[0], root / paths[1]
        try:
            libraries, shaders = progress.collect_libraries(), progress.collect_shaders()
        except (OSError, AttributeError):
            return None
        finally:
            progress.PRX, progress.OPCODES = saved
    return {name: {key: data[key] for key in ("done", "total", "percent")}
            for name, data in (("libraries", libraries), ("shaders", shaders))}


def collect(ref):
    points = []
    for day, sha in daily_commits(ref):
        counts = measure(sha)
        if counts:
            points.append({"date": day, "commit": sha, **counts})
    return points


def labelled(day, span):
    if span <= 14:
        return f"{day:%b} {day.day}"
    if span <= 150:
        return f"{day:%b} {day.day}" if day.weekday() == 0 else ""
    return f"{day:%b %Y}" if day.day <= 7 and day.weekday() == 0 else ""


def scale(peak):
    step = next(s for s in (5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000) if peak <= s * 4)
    return step, max(step, -(-peak // step) * step)


def spread(ys, gap=15):
    out = list(ys)
    order = sorted(range(len(ys)), key=ys.__getitem__)
    for above, below in zip(order, order[1:]):
        out[below] = max(out[below], out[above] + gap)
    return out


def chart(points):
    days = [date.fromisoformat(p["date"]).toordinal() for p in points]
    first, span = days[0], max(days[-1] - days[0], 1)
    px = lambda d: LEFT + (d - first) / span * (WIDTH - LEFT - RIGHT)
    xs = [px(d) for d in days]
    edges = [LEFT] + [(a + b) / 2 for a, b in zip(xs, xs[1:])] + [WIDTH - RIGHT]
    labels = [(x, labelled(date.fromordinal(d), span)) for x, d in zip(xs, days)]
    height = len(SERIES) * (TITLE + PANEL + GAP) - GAP + AXIS
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {height}" role="img" '
           'aria-label="Implemented and declared system library functions and GPU shader instructions over time" '
           f'font-family="sans-serif" font-size="12" fill="{TEXT}">',
           '<style>.hit{fill:#fff;fill-opacity:0}.hit:hover{fill-opacity:0.06}</style>',
           f'<rect width="{WIDTH}" height="{height}" rx="6" fill="{BACKGROUND}"/>']
    for index, (name, title, color) in enumerate(SERIES):
        top = index * (TITLE + PANEL + GAP) + TITLE
        base = top + PANEL
        step, peak = scale(max(p[name][key] for p in points for key in ("done", "total")))
        py = lambda v: base - v / peak * PANEL
        out.append(f'<text x="{LEFT}" y="{top - 12}" font-size="13" font-weight="bold">{title}</text>')
        for tick in range(0, peak + 1, step):
            out.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{py(tick):.1f}" y2="{py(tick):.1f}" stroke="{GRID}"/>'
                       f'<text x="{LEFT - 6}" y="{py(tick) + 4:.1f}" text-anchor="end" fill="{MUTED}">{tick:,}</text>')
        for x, label in labels:
            if label:
                out.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{top}" y2="{base}" stroke="{GRID}" stroke-width="0.5"/>')
        done = [(x, py(p[name]["done"])) for x, p in zip(xs, points)]
        total = [(x, py(p[name]["total"])) for x, p in zip(xs, points)]
        line = lambda coords: " ".join(f"{x:.1f},{y:.1f}" for x, y in coords)
        out.append(f'<polygon points="{xs[0]:.1f},{base} {line(done)} {xs[-1]:.1f},{base}" fill="{color}" opacity="0.15"/>')
        out.append(f'<polyline points="{line(total)}" fill="none" stroke="{MUTED}" stroke-width="2" stroke-dasharray="6 4"/>')
        out.append(f'<polyline points="{line(done)}" fill="none" stroke="{color}" stroke-width="2"/>')
        if len(points) <= MAX_DOTS:
            out += [f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{color}"/>' for x, y in done]
        last = points[-1][name]
        declared_y, implemented_y = spread([total[-1][1], done[-1][1]])
        out.append(f'<text x="{WIDTH - RIGHT + 8}" y="{declared_y + 4:.1f}" fill="{MUTED}">Declared {last["total"]:,}</text>'
                   f'<text x="{WIDTH - RIGHT + 8}" y="{implemented_y + 4:.1f}" fill="{TEXT}" font-weight="bold">'
                   f'<tspan fill="{color}">\u25cf</tspan> Implemented {last["done"]:,} ({last["percent"]:.0f}%)</text>')
        for left, right, p in zip(edges, edges[1:], points):
            out.append(f'<rect class="hit" x="{left:.1f}" y="{top}" width="{right - left:.1f}" height="{PANEL}">'
                       f'<title>{p["date"]} · {title}\nImplemented {p[name]["done"]:,} of {p[name]["total"]:,} declared '
                       f'({p[name]["percent"]}%)</title></rect>')
    axis = height - AXIS + 18
    out += [f'<text class="x" x="{x:.1f}" y="{axis}" text-anchor="middle" font-size="11" fill="{MUTED}">{label}</text>'
            for x, label in labels if label]
    out.append("</svg>")
    return "\n".join(out)


def page(points):
    rows = "\n".join(f'<tr><td>{p["date"]}</td>' + "".join(
        f'<td>{p[name]["done"]}/{p[name]["total"]}</td><td>{p[name]["percent"]}%</td>' for name, _, _ in SERIES) + "</tr>"
        for p in reversed(points))
    return "\n".join([
        '<!DOCTYPE html>',
        '<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
        '<title>AnyPS5 progress history</title>',
        '<style>body{font-family:sans-serif;max-width:1000px;margin:auto;padding:16px}svg{max-width:100%;height:auto}'
        'table{border-collapse:collapse}th,td{border:1px solid #ccc;padding:2px 8px}td+td,th+th{text-align:right}</style>',
        '</head><body>',
        '<h1>Progress history</h1>',
        chart(points),
        f'<p>Each panel has its own scale: the solid line is what is implemented, the dashed line what is declared. Library '
        'functions are declared in the <code>prx</code> sources; the shader total is the AMD RDNA 1 + RDNA 2 instruction list '
        f'(<a href="{progress.SOURCE}/tools/rdna_isa.txt">tools/rdna_isa.txt</a>). '
        'One point per day with commits on <code>main</code>, taken at the last commit of the day. Every point is measured '
        f'with the current <a href="{progress.SOURCE}/tools/progress.py">tools/progress.py</a> rules and instruction list, '
        'so the series is comparable across days. Hover a day for the exact numbers. Raw numbers: <a href="history.json">history.json</a>. '
        '<a href="progress.html">Back to the current progress</a>.</p>',
        '<table><tr><th>Date</th>' + "".join(f'<th>{escape(label)}</th><th>%</th>' for _, label, _ in SERIES) + '</tr>',
        rows,
        '</table>',
        '</body></html>',
    ]) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--ref", default="HEAD", help="branch or commit whose first-parent history is measured")
    args = parser.parse_args()
    points = collect(args.ref)
    if not points:
        parser.error("no commit with a measurable tree")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "history.json").write_text(json.dumps(points, indent=2))
    (args.output / "history.svg").write_text(chart(points) + "\n")
    (args.output / "history.html").write_text(page(points))
    print(f"{len(points)} days, {points[0]['date']} .. {points[-1]['date']}")
