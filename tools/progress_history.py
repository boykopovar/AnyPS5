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

SERIES = (("libraries", "System libraries", "#2f81f7"), ("shaders", "GPU shader instructions", "#2ea043"))
LINES = (("libraries", "done", "Implemented", "#2f81f7", ""), ("libraries", "total", "Declared", "#8b949e", "6 4"),
         ("shaders", "done", "Shader instructions", "#2ea043", ""))
WIDTH, HEIGHT, LEFT, RIGHT, TOP, BOTTOM = 900, 380, 50, 170, 20, 60
MAX_DOTS = 45
BACKGROUND, GRID, TEXT = "#0d1117", "#30363d", "#c9d1d9"


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
    if span <= 31:
        return day.strftime("%m-%d")
    if span <= 150:
        return day.strftime("%m-%d") if day.weekday() == 0 else ""
    return day.strftime("%Y-%m") if day.day <= 7 and day.weekday() == 0 else ""


def chart(points):
    days = [date.fromisoformat(p["date"]).toordinal() for p in points]
    first, span = days[0], max(days[-1] - days[0], 1)
    peak = max(p[name][key] for p in points for name, key, _, _, _ in LINES)
    step = next(s for s in (10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000) if peak <= s * 8)
    top = max(step, -(-peak // step) * step)
    px = lambda d: LEFT + (d - first) / span * (WIDTH - LEFT - RIGHT)
    py = lambda v: TOP + (1 - v / top) * (HEIGHT - TOP - BOTTOM)
    base = py(0)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {HEIGHT}" role="img" '
           f'aria-label="Progress over time" font-family="sans-serif" font-size="12" fill="{TEXT}">',
           f'<rect width="{WIDTH}" height="{HEIGHT}" rx="6" fill="{BACKGROUND}"/>']
    for tick in range(0, top + 1, step):
        out.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{py(tick):.1f}" y2="{py(tick):.1f}" stroke="{GRID}"/>'
                   f'<text x="{LEFT - 6}" y="{py(tick) + 4:.1f}" text-anchor="end">{tick}</text>')
    for d in range(first, days[-1] + 1):
        x = px(d)
        label = labelled(date.fromordinal(d), span)
        if label:
            out.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{TOP}" y2="{base:.1f}" stroke="{GRID}" stroke-width="0.5"/>'
                       f'<text transform="translate({x + 4:.1f} {base + 8:.1f}) rotate(60)" font-size="10">{label}</text>')
    for name, key, label, color, dash in LINES:
        coords = [(px(d), py(p[name][key])) for d, p in zip(days, points)]
        line = " ".join(f"{x:.1f},{y:.1f}" for x, y in coords)
        if not dash:
            out.append(f'<polygon points="{coords[0][0]:.1f},{base:.1f} {line} {coords[-1][0]:.1f},{base:.1f}" fill="{color}" opacity="0.15"/>')
        out.append(f'<polyline points="{line}" fill="none" stroke="{color}" stroke-width="2"' + (f' stroke-dasharray="{dash}"' if dash else '') + '/>')
        for (x, y), p in zip(coords, points) if len(points) <= MAX_DOTS and not dash else ():
            out.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{color}"><title>{p["date"]}: {label} '
                       f'{p[name][key]} ({p[name]["done"]}/{p[name]["total"]}, {p[name]["percent"]}%)</title></circle>')
        out.append(f'<text x="{WIDTH - RIGHT + 8}" y="{coords[-1][1] + 4:.1f}" fill="{color}" font-weight="bold">'
                   f'{label} {points[-1][name][key]}</text>')
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
        f'<p>One point per day with commits on <code>main</code>, taken at the last commit of the day. Every point is measured '
        f'with the current <a href="{progress.SOURCE}/tools/progress.py">tools/progress.py</a> rules and instruction list, '
        'so the series is comparable across days. Hover a point for the exact numbers. Raw numbers: <a href="history.json">history.json</a>. '
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
