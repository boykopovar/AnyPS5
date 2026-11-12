import argparse
import csv
import hashlib
import io
import json
import os
import stat
import sys
import subprocess
import tarfile
import tempfile
from datetime import date, datetime, timezone
from html import escape
from pathlib import Path

import progress

LIBRARY_SERIES = ("libraries", "System library functions", "#2f81f7")
SHADER_SERIES = (
    ("decoded", "Decoded shader forms", "#2ea043"),
    ("legacy", "Legacy shader opcode names", "#d29922"),
)
SHADER_METRICS = {metric for metric, _, _ in SHADER_SERIES}
WIDTH, LEFT, RIGHT, TITLE, PANEL, GAP, AXIS = 900, 56, 190, 34, 150, 16, 32
MAX_DOTS = 45
BACKGROUND, GRID, TEXT, MUTED = "#0d1117", "#30363d", "#c9d1d9", "#8b949e"
THEME = (
    '.history-theme,svg[role="img"]{color-scheme:light dark;--history-background:#fff;--history-grid:#d0d7de;'
    '--history-text:#1f2328;--history-muted:#59636e;--history-link:#0969da;--history-visited:#8250df;--history-hit:#000}'
    '@media(prefers-color-scheme:dark){.history-theme,svg[role="img"]{--history-background:#0d1117;--history-grid:#30363d;'
    '--history-text:#c9d1d9;--history-muted:#8b949e;--history-link:#58a6ff;--history-visited:#bc8cff;--history-hit:#fff}}'
)
SVG_STYLE = (
    '<style>' + THEME + 'svg[role="img"]{fill:var(--history-text)}'
    + ''.join(f'svg[role="img"] [fill="{color}"]{{fill:var(--history-{name})}}'
              for name, color in (("background", BACKGROUND), ("text", TEXT), ("muted", MUTED)))
    + ''.join(f'svg[role="img"] [stroke="{color}"]{{stroke:var(--history-{name})}}'
              for name, color in (("grid", GRID), ("muted", MUTED)))
    + 'svg[role="img"] .hit{fill:var(--history-hit)}</style>'
)


def git(*args):
    try:
        return subprocess.run(["git", "-C", str(progress.ROOT), *args], check=True, capture_output=True).stdout
    except subprocess.CalledProcessError as error:
        reason = error.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(f"git {' '.join(args)} failed: {reason}") from error


def daily_commits(ref):
    log = git("log", "--first-parent", "--format=%H %ct", ref)
    seen = {}
    for line in log.decode("utf-8").splitlines():
        sha, timestamp = line.split()
        seen.setdefault(datetime.fromtimestamp(int(timestamp), timezone.utc).date().isoformat(), sha)
    return sorted(seen.items())


def measure(sha):
    candidate_paths = [
        progress.PRX.relative_to(progress.ROOT).as_posix(),
        progress.OPCODES.relative_to(progress.ROOT).as_posix(),
        progress.shader_scan.RECOMPILER.rstrip("/"),
        progress.shader_scan.AGC_DRIVER.rstrip("/"),
    ]
    tree = git("ls-tree", "-r", "--name-only", sha, "--", *candidate_paths).decode("utf-8").splitlines()
    tree = set(tree)
    required_paths = candidate_paths[:2]
    if any(not any(name == path or name.startswith(path + "/") for name in tree) for path in required_paths):
        return None
    paths = required_paths + [
        path for path in candidate_paths[2:]
        if any(name == path or name.startswith(path + "/") for name in tree)
    ]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        archive = tarfile.open(fileobj=io.BytesIO(git("archive", sha, "--", *paths)))
        if hasattr(tarfile, "data_filter"):
            archive.extractall(root, filter="data")
        else:
            archive.extractall(root)
        saved = progress.PRX, progress.OPCODES
        progress.PRX, progress.OPCODES = root / paths[0], root / paths[1]
        try:
            libraries = progress.collect_libraries()
            try:
                shader_source = progress.collect_shader_source(root)
            except (OSError, AttributeError, progress.shader_scan.ScanError):
                shader_source = None
            if not shader_source:
                legacy = progress.collect_shaders()
        except (OSError, AttributeError):
            return None
        finally:
            progress.PRX, progress.OPCODES = saved
    if shader_source:
        instructions = shader_source["instructions"]
        shaders = {
            "done": instructions["decoded"],
            "total": instructions["total"],
            "percent": progress.shader_source_percent(shader_source),
        }
    else:
        shaders = {key: legacy[key] for key in ("done", "total", "percent")}
    return {
        "libraries": {key: libraries[key] for key in ("done", "total", "percent")},
        "shaders": shaders,
        "shader_metric": "decoded" if shader_source else "legacy",
    }


def cache_signature():
    digest = hashlib.sha256(repr(sys.version_info[:3]).encode("ascii"))
    for path in (
        Path(__file__),
        Path(progress.__file__),
        Path(progress.shader_scan.__file__),
        Path(progress.tokenizer_spec.origin),
        progress.ISA,
        progress.ISA_FORMS,
    ):
        content = path.read_bytes()
        digest.update(len(content).to_bytes(8, "little"))
        digest.update(content)
    digest.update(repr((
        progress.PRX.relative_to(progress.ROOT).as_posix(),
        progress.OPCODES.relative_to(progress.ROOT).as_posix(),
        progress.shader_scan.RECOMPILER,
        progress.shader_scan.AGC_DRIVER,
    )).encode("utf-8"))
    return digest.hexdigest()


def load_cache(path, signature):
    if not path.exists():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except RecursionError as error:
        raise ValueError(f"{path}: history cache JSON is too deeply nested") from error
    if not isinstance(data, dict) or type(data.get("schema_version")) is not int or data["schema_version"] != 2 or not isinstance(data.get("rules"), str) or not isinstance(data.get("commits"), dict):
        raise ValueError(f"{path}: invalid history cache")
    if data["rules"] != signature:
        return {}
    for sha, counts in data["commits"].items():
        if len(sha) != 40 or any(character not in "0123456789abcdef" for character in sha):
            raise ValueError(f"{path}: invalid cached commit")
        if counts is None:
            continue
        if not isinstance(counts, dict) or counts.keys() != {"libraries", "shaders", "shader_metric"}:
            raise ValueError(f"{path}: invalid cached counts for {sha}")
        if not isinstance(counts["shader_metric"], str) or counts["shader_metric"] not in SHADER_METRICS:
            raise ValueError(f"{path}: invalid cached shader metric for {sha}")
        for metric in (counts["libraries"], counts["shaders"]):
            if not isinstance(metric, dict) or metric.keys() != {"done", "total", "percent"}:
                raise ValueError(f"{path}: invalid cached metric for {sha}")
            done, total, percent = (metric[key] for key in ("done", "total", "percent"))
            if type(done) is not int or type(total) is not int or not 0 <= done <= total or type(percent) not in (int, float) or percent != (round(100 * done / total, 2) if total else 0):
                raise ValueError(f"{path}: inconsistent cached metric for {sha}")
    return data["commits"]


def save_cache(path, signature, commits):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", newline="\n", dir=path.parent,
                                         prefix=".history-", delete=False) as output:
            temporary = Path(output.name)
            json.dump({"schema_version": 2, "rules": signature, "commits": commits}, output, sort_keys=True)
            output.write("\n")
        if os.name != "nt" and path.exists():
            temporary.chmod(stat.S_IMODE(path.stat().st_mode))
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def utc_day(value):
    try:
        if date.fromisoformat(value).isoformat() == value:
            return value
    except ValueError:
        pass
    raise argparse.ArgumentTypeError("date bounds must be valid YYYY-MM-DD UTC days")


def collect(ref, cache=None, since=None, until=None):
    signature = cache_signature() if cache is not None else None
    records = load_cache(cache, signature) if cache is not None else {}
    points, changed = [], False
    for day, sha in daily_commits(ref):
        if (since is not None and day < since) or (until is not None and day > until):
            continue
        if sha not in records:
            records[sha] = measure(sha)
            changed = True
        counts = records[sha]
        if counts:
            points.append({"date": day, "commit": sha, **counts})
    if cache is not None and changed:
        save_cache(cache, signature, records)
    return points


def labelled(day, span):
    if span <= 14:
        return f"{day:%b} {day.day}"
    if span <= 150:
        return f"{day:%b} {day.day}" if day.weekday() == 0 else ""
    return f"{day:%b %Y}" if day.day <= 7 and day.weekday() == 0 else ""


def scale(peak):
    magnitude = 1
    while peak > 20 * magnitude:
        magnitude *= 10
    step = next(s * magnitude for s in (1, 2, 5) if peak <= s * magnitude * 4) if magnitude > 1 else 5
    return step, max(step, -(-peak // step) * step)


def spread(ys, gap=15):
    out = list(ys)
    order = sorted(range(len(ys)), key=ys.__getitem__)
    for above, below in zip(order, order[1:]):
        out[below] = max(out[below], out[above] + gap)
    return out


def shader_metric(point):
    return point.get("shader_metric", "decoded")


def used_shader_metrics(points):
    return [metric for metric, _, _ in SHADER_SERIES if any(shader_metric(p) == metric for p in points)]


def chart(points):
    days = [date.fromisoformat(p["date"]).toordinal() for p in points]
    first, span = days[0], max(days[-1] - days[0], 1)
    px = lambda d: LEFT + (d - first) / span * (WIDTH - LEFT - RIGHT)
    xs = [px(d) for d in days]
    edges = [LEFT] + [(a + b) / 2 for a, b in zip(xs, xs[1:])] + [WIDTH - RIGHT]
    labels = [(px(d), labelled(date.fromordinal(d), span)) for d in range(first, days[-1] + 1)]
    metrics = used_shader_metrics(points)
    panels = [(LIBRARY_SERIES[0], LIBRARY_SERIES[1], LIBRARY_SERIES[2], None)]
    panels += [("shaders", title, color, metric) for metric, title, color in SHADER_SERIES if metric in metrics]
    height = len(panels) * (TITLE + PANEL + GAP) - GAP + AXIS
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {height}" role="img" '
           'aria-label="Implemented and declared system library functions and shader metrics over time" '
           f'font-family="sans-serif" font-size="12" fill="{TEXT}">',
           '<style>.hit{fill:#fff;fill-opacity:0}.hit:hover{fill-opacity:0.06}</style>',
           f'<rect width="{WIDTH}" height="{height}" rx="6" fill="{BACKGROUND}"/>']
    for index, (name, title, color, metric) in enumerate(panels):
        top = index * (TITLE + PANEL + GAP) + TITLE
        base = top + PANEL
        shown = [(x, p) for x, p in zip(xs, points) if metric is None or shader_metric(p) == metric]
        step, peak = scale(max(p[name][key] for _, p in shown for key in ("done", "total")))
        py = lambda v: base - v / peak * PANEL
        out.append(f'<text x="{LEFT}" y="{top - 12}" font-size="13" font-weight="bold">{title}</text>')
        for tick in range(0, peak + 1, step):
            out.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{py(tick):.1f}" y2="{py(tick):.1f}" stroke="{GRID}"/>'
                       f'<text x="{LEFT - 6}" y="{py(tick) + 4:.1f}" text-anchor="end" fill="{MUTED}">{tick:,}</text>')
        for x, label in labels:
            if label:
                out.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="{top}" y2="{base}" stroke="{GRID}" stroke-width="0.5"/>')
        done = [(x, py(p[name]["done"])) for x, p in shown]
        total = [(x, py(p[name]["total"])) for x, p in shown]
        line = lambda coords: " ".join(f"{x:.1f},{y:.1f}" for x, y in coords)
        out.append(f'<polygon points="{shown[0][0]:.1f},{base} {line(done)} {shown[-1][0]:.1f},{base}" fill="{color}" opacity="0.15"/>')
        out.append(f'<polyline points="{line(total)}" fill="none" stroke="{MUTED}" stroke-width="2" stroke-dasharray="6 4"/>')
        out.append(f'<polyline points="{line(done)}" fill="none" stroke="{color}" stroke-width="2"/>')
        if len(shown) <= MAX_DOTS:
            out += [f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{color}"/>' for x, y in done]
        last = shown[-1][1][name]
        declared_y, implemented_y = spread([total[-1][1], done[-1][1]])
        out.append(f'<text x="{WIDTH - RIGHT + 8}" y="{declared_y + 4:.1f}" fill="{MUTED}">Declared {last["total"]:,}</text>'
                   f'<text x="{WIDTH - RIGHT + 8}" y="{implemented_y + 4:.1f}" fill="{TEXT}" font-weight="bold">'
                   f'<tspan fill="{color}">\u25cf</tspan> Implemented {last["done"]:,} ({last["percent"]:.0f}%)</text>')
        for (left, right), (_, p) in zip(zip(edges, edges[1:]), zip(xs, points)):
            if metric is not None and shader_metric(p) != metric:
                continue
            out.append(f'<rect class="hit" x="{left:.1f}" y="{top}" width="{right - left:.1f}" height="{PANEL}">'
                       f'<title>{p["date"]} · {title}\nImplemented {p[name]["done"]:,} of {p[name]["total"]:,} declared '
                       f'({p[name]["percent"]}%)</title></rect>')
    axis = height - AXIS + 18
    out += [f'<text class="x" x="{x:.1f}" y="{axis}" text-anchor="middle" font-size="11" fill="{MUTED}">{label}</text>'
            for x, label in labels if label]
    out.append(SVG_STYLE)
    out.append("</svg>")
    return "\n".join(out)


def gained(points, days):
    last = date.fromisoformat(points[-1]["date"])
    before = [p for p in points if (last - date.fromisoformat(p["date"])).days >= days]
    start = before[-1] if before else points[0]
    return points[-1]["libraries"]["done"] - start["libraries"]["done"]


def weekly_badge(points):
    value = f"{gained(points, 7):+,} functions"
    return progress.badge("implemented in 7 days", points[-1]["libraries"], value, "#2f81f7")


def daily(points):
    bars = [(p["date"], p["libraries"]["done"] - q["libraries"]["done"]) for q, p in zip(points, points[1:])]
    if not bars:
        return ""
    step, peak = scale(max(max(change for _, change in bars), 1))
    lowest = min(min(change for _, change in bars), 0)
    height, top = 214, 34
    base = top + 150 * peak / (peak - lowest)
    py = lambda v: base - v / (peak - lowest) * 150
    slot = (WIDTH - LEFT - RIGHT) / len(bars)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {height}" role="img" '
           'aria-label="System library functions implemented per day" '
           f'font-family="sans-serif" font-size="12" fill="{TEXT}">',
           f'<rect width="{WIDTH}" height="{height}" rx="6" fill="{BACKGROUND}"/>',
           f'<text x="{LEFT}" y="{top - 12}" font-size="13" font-weight="bold">System library functions implemented per day</text>']
    for tick in range(0, peak + 1, step):
        out.append(f'<line x1="{LEFT}" x2="{WIDTH - RIGHT}" y1="{py(tick):.1f}" y2="{py(tick):.1f}" stroke="{GRID}"/>'
                   f'<text x="{LEFT - 6}" y="{py(tick) + 4:.1f}" text-anchor="end" fill="{MUTED}">{tick:,}</text>')
    for index, (day, change) in enumerate(bars):
        x, y = LEFT + index * slot, py(max(change, 0))
        out.append(f'<rect x="{x + slot * 0.15:.1f}" y="{y:.1f}" width="{slot * 0.7:.1f}" height="{abs(py(change) - base):.1f}" '
                   f'fill="{LIBRARY_SERIES[2]}"><title>{day}: {change:+,} implemented</title></rect>')
        label = labelled(date.fromisoformat(day), len(bars))
        if label:
            out.append(f'<text x="{x + slot / 2:.1f}" y="{top + 168}" text-anchor="middle" font-size="11" fill="{MUTED}">{label}</text>')
    week = gained(points, 7)
    out.append(f'<text x="{WIDTH - RIGHT + 8}" y="{top + 16}" fill="{TEXT}" font-weight="bold">Last 7 days {week:+,}</text>'
               f'<text x="{WIDTH - RIGHT + 8}" y="{top + 34}" fill="{MUTED}">Last day {bars[-1][1]:+,}</text>')
    out.append(SVG_STYLE)
    out.append("</svg>")
    return "\n".join(out)


def page(points):
    metrics = used_shader_metrics(points)
    labels = {metric: title for metric, title, _ in SHADER_SERIES}
    changes = ["n/a"] + [f'{p["libraries"]["done"] - q["libraries"]["done"]:+,}' for q, p in zip(points, points[1:])]
    rows = "\n".join(
        f'<tr><td>{p["date"]}</td><td>{p["libraries"]["done"]}/{p["libraries"]["total"]}</td>'
        f'<td>{p["libraries"]["percent"]}%</td><td>{change}</td>'
        + "".join(f'<td>{p["shaders"]["done"]}/{p["shaders"]["total"]}</td><td>{p["shaders"]["percent"]}%</td>'
                  if shader_metric(p) == metric else '<td>—</td><td>—</td>' for metric in metrics)
        + '</tr>'
        for p, change in reversed(list(zip(points, changes))))
    return "\n".join([
        '<!DOCTYPE html>',
        '<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
        '<meta name="color-scheme" content="light dark"><title>AnyPS5 progress history</title>',
        '<style>:root{color-scheme:light dark}' + THEME +
        'body{font-family:sans-serif;max-width:1000px;margin:auto;padding:16px;'
        'background:var(--history-background);color:var(--history-text)}svg{max-width:100%;height:auto}'
        'a{color:var(--history-link)}a:visited{color:var(--history-visited)}'
        'table{border-collapse:collapse}th,td{border:1px solid var(--history-grid);padding:2px 8px}'
        'td+td,th+th{text-align:right}</style>',
        '</head><body class="history-theme">',
        '<h1>Progress history</h1>',
        chart(points),
        daily(points),
        f'<p>Each panel has its own scale: the solid line is what is implemented, the dashed line what is declared. Library '
        'functions are declared in the <code>prx</code> sources. Shader history keeps the metrics separate: '
        '"Decoded shader forms" counts the AMD RDNA 2 instruction forms decoded by the current scanner '
        f'(<a href="{progress.SOURCE}/tools/rdna2_isa_forms.txt">tools/rdna2_isa_forms.txt</a>); '
        '"Legacy shader opcode names" counts the archived decoder opcode table against the current AMD RDNA 1 + RDNA 2 '
        f'instruction list (<a href="{progress.SOURCE}/tools/rdna_isa.txt">tools/rdna_isa.txt</a>) and is used only when '
        'the current scanner cannot read the archived source. Their totals differ, so compare points only within the same '
        'shader panel. One point per UTC day with commits on <code>main</code>, taken at the last commit of the day. Every point '
        f'is measured with the current <a href="{progress.SOURCE}/tools/progress.py">tools/progress.py</a> and '
        f'<a href="{progress.SOURCE}/tools/shader_scan.py">tools/shader_scan.py</a> rules, '
        'so points within each series are comparable across days. The bars and the Functions implemented column show the net change '
        'in implemented library functions from one point to the next; the first point is n/a. Hover a day for the exact numbers. <a href="history.json">history.json</a> records the '
        'shader metric for each point. <a href="progress.html">Back to the current progress</a>.</p>',
        '<table><tr><th>Date</th><th>System library functions</th><th>%</th><th>Functions implemented</th>'
        + "".join(f'<th>{escape(labels[metric])}</th><th>%</th>' for metric in metrics) + '</tr>',
        rows,
        '</table>',
        '</body></html>',
    ]) + "\n"


def same_location(first, second):
    return os.path.normcase(os.path.realpath(first)) == os.path.normcase(os.path.realpath(second))


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--since", type=utc_day, help="first UTC day to measure, inclusive (YYYY-MM-DD)")
    parser.add_argument("--until", type=utc_day, help="last UTC day to measure, inclusive (YYYY-MM-DD)")
    parser.add_argument("output", type=Path)
    parser.add_argument("--ref", default="HEAD", help="branch or commit whose first-parent history is measured")
    parser.add_argument("--cache", type=Path, help="reuse per-commit counts while the scanner, ISA list and Python version match")
    parser.add_argument("--csv", action="store_true", help="also write history.csv with the measured JSON points")
    args = parser.parse_args(argv)
    outputs = [args.output / name for name in ("history.json", "history.svg", "history.html", "badge-week.svg")]
    if args.csv:
        outputs.append(args.output / "history.csv")
    try:
        for index, path in enumerate(outputs):
            if (path.exists() or path.is_symlink()) and not path.is_file():
                raise ValueError(f"{path}: report destination must be a regular file")
            for other in outputs[index + 1:]:
                if (path.exists() and other.exists() and path.samefile(other)) or same_location(path, other):
                    raise ValueError(f"generated reports cannot alias each other: {path.name} and {other.name}")
        if args.cache:
            if any(same_location(args.cache, path) or
                   (args.cache.exists() and path.exists() and args.cache.samefile(path)) for path in outputs):
                raise ValueError("history cache must differ from generated reports")
        if args.since and args.until and args.since > args.until:
            raise ValueError("--since must not be later than --until")
        points = collect(args.ref, args.cache, args.since, args.until)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if not points:
        parser.error("no commit with a measurable tree")
    try:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / "history.json").write_text(json.dumps(points, indent=2), encoding="utf-8")
        (args.output / "history.svg").write_text(chart(points) + "\n", encoding="utf-8")
        (args.output / "history.html").write_text(page(points), encoding="utf-8")
        (args.output / "badge-week.svg").write_text(weekly_badge(points), encoding="utf-8")
        if args.csv:
            with (args.output / "history.csv").open("w", encoding="utf-8", newline="") as output:
                writer = csv.writer(output)
                writer.writerow(["date", "commit"] + [f"{name}_{key}" for name in ("libraries", "shaders")
                                                      for key in ("done", "total", "percent")] + ["shader_metric"])
                writer.writerows([point["date"], point["commit"]] +
                                 [point[name][key] for name in ("libraries", "shaders") for key in ("done", "total", "percent")] +
                                 [shader_metric(point)] for point in points)
    except OSError as error:
        parser.error(f"{error.filename or args.output}: {error.strerror or error}")
    print(f"{len(points)} days, {points[0]['date']} .. {points[-1]['date']}")


if __name__ == "__main__":
    main()
