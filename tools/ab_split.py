import argparse
import json
import math
import re
import sys
from pathlib import Path

DESCRIPTION = """Per-phase means of an in-run A/B (APS5_AB) log. The driver prints "[ab] phase A|B <k> begins at present
<n> (t <s> s)" when a phase starts and "[ab] phase A|B <k> settled at present <n> (t <s> s)" APS5_AB_SETTLE seconds
later. Each periodic report covers the time since the previous report of its kind, so a report counts for phase k
only when the previous report of its kind came after phase k's settled line and the report came before phase k+1
began. Presents per 10 s and frame time come from the [ab] lines (settled line to the next phase's begin), GPU busy
and idle gaps per frame from [present], GPU time per present from [gputime] (APS5_PROFILE_GPU), the committer and
drains from [draw] pipelined, unlocked hook waits from [syncwait] (APS5_PROFILE_DRAW). The noise column compares
each B phase with the mean of its neighbouring A phases, which cancels a linear drift: mean difference, standard
error, standard deviation and count."""

PHASE = re.compile(r"^\[ab\] phase ([AB]) (\d+) (begins|settled) at present (\d+) \(t ([\d.]+) s\)")
KINDS = (("present", "[present] "), ("gputime", "[gputime] "), ("draw", "[draw] pipelined"), ("syncwait", "[syncwait] "))


def number(pattern, text):
    match = re.search(pattern, text)
    return float(match.group(1)) if match else None


def parse_report(kind, text):
    if kind == "present":
        presents, dropped = number(r"^\[present\] (\d+) presents", text), number(r"\((\d+) dropped", text)
        busy, gaps = number(r"GPU busy ([\d.]+) ms", text), number(r"idle gaps ([\d.]+) ms", text)
        if None in (presents, dropped, busy, gaps):
            return None
        return {"frames": presents + dropped, "busy": busy, "gaps": gaps}
    if kind == "gputime":
        total, presents = number(r"^\[gputime\] ([\d.]+) ms of GPU time", text), number(r"; (\d+) presents;", text)
        if total is None or presents is None or "; by class:" not in text:
            return None
        programs_text, classes_text = text.split("by program:", 1)[-1].split("; by class:", 1)
        programs, classes = {}, {}
        for key, ms in re.findall(r" (0x[0-9a-f]+) x\d+ ([\d.]+)ms", programs_text):
            programs[key] = programs.get(key, 0.0) + float(ms)
        for name, ms in re.findall(r" ([a-z][a-z-]*) x\d+ ([\d.]+)ms [\d.]+MiB \(per present", classes_text):
            classes[name] = classes.get(name, 0.0) + float(ms)
        return {"total": total, "presents": presents, "programs": programs, "classes": classes}
    if kind == "draw":
        commits, per_commit = number(r": (\d+) commits at", text), number(r"commits at ([\d.]+) us each", text)
        enqueued, room = number(r"; (\d+) enqueued behind", text), number(r"waiting ([\d.]+) us per draw for room", text)
        if None in (commits, per_commit, enqueued, room) or "; drains:" not in text:
            return None
        drains = text.split("; drains:", 1)[1].split(";", 1)[0]
        return {"commits": commits, "per_commit": per_commit, "enqueued": enqueued, "room": room,
                "drain_ms": sum(float(ms) for ms in re.findall(r"\(([\d.]+) ms\)", drains))}
    if kind == "syncwait":
        if "):" not in text:
            return None
        sites = text.split("):", 1)[1].split("; batches ahead", 1)[0]
        return {"wait_ms": sum(float(ms) for ms in re.findall(r" \d+/([\d.]+)ms", sites))}
    return None


def read_log(lines):
    config, phases, current = None, [], None
    previous = {kind: None for kind, _ in KINDS}
    for line in lines:
        text = line.rstrip("\r\n")
        if text.startswith("[ab] in-run A/B:"):
            config = text
            continue
        match = PHASE.match(text)
        if match:
            kind, index, event, present, seconds = match.group(1), int(match.group(2)), match.group(3), int(match.group(4)), float(match.group(5))
            if event == "begins":
                if current is not None:
                    current["end"] = (present, seconds)
                current = {"kind": kind, "index": index, "begin": (present, seconds), "settled": None, "end": None,
                           "reports": {name: [] for name, _ in KINDS}}
                phases.append(current)
            elif current is not None and current["index"] == index:
                current["settled"] = (present, seconds)
            continue
        for name, prefix in KINDS:
            if text.startswith(prefix):
                state = (current["index"], current["settled"] is not None) if current is not None else None
                if state is not None and state[1] and previous[name] == state:
                    report = parse_report(name, text)
                    if report is not None:
                        current["reports"][name].append(report)
                previous[name] = state
                break
    return config, phases


def weighted(rows, value, weight):
    total = sum(row[weight] for row in rows)
    return sum(row[value] * row[weight] for row in rows) / total if total else None


def metrics(phase, programs, classes):
    out = {}
    if phase["settled"] is not None and phase["end"] is not None and phase["end"][1] > phase["settled"][1]:
        presents, seconds = phase["end"][0] - phase["settled"][0], phase["end"][1] - phase["settled"][1]
        out["presents/10s"] = presents / seconds * 10
        if presents > 0:
            out["frame ms"] = seconds / presents * 1000
    rows = phase["reports"]["present"]
    out["GPU busy ms/frame"], out["GPU idle gaps ms/frame"] = weighted(rows, "busy", "frames"), weighted(rows, "gaps", "frames")
    rows = phase["reports"]["gputime"]
    presents = sum(row["presents"] for row in rows)
    if presents:
        out["gputime ms/present"] = sum(row["total"] for row in rows) / presents
        for key in programs:
            out[f"{key} ms/present"] = sum(row["programs"].get(key, 0.0) for row in rows) / presents
        for name in classes:
            out[f"{name} ms/present"] = sum(row["classes"].get(name, 0.0) for row in rows) / presents
    rows = phase["reports"]["draw"]
    out["committer us/commit"], out["room wait us/draw"] = weighted(rows, "per_commit", "commits"), weighted(rows, "room", "enqueued")
    out["drain ms/report"] = sum(row["drain_ms"] for row in rows) / len(rows) if rows else None
    rows = phase["reports"]["syncwait"]
    out["syncwait GPU ms/report"] = sum(row["wait_ms"] for row in rows) / len(rows) if rows else None
    return {name: value for name, value in out.items() if value is not None}


def noise(values, phases):
    differences = []
    for index, phase in enumerate(phases):
        if phase["kind"] != "B" or values[index] is None:
            continue
        neighbours = [values[other] for other in (index - 1, index + 1)
                      if 0 <= other < len(phases) and phases[other]["kind"] == "A" and values[other] is not None]
        if neighbours:
            differences.append(values[index] - sum(neighbours) / len(neighbours))
    if not differences:
        return None
    mean = sum(differences) / len(differences)
    if len(differences) == 1:
        return {"mean": mean, "se": None, "sd": None, "n": 1}
    sd = math.sqrt(sum((value - mean) ** 2 for value in differences) / (len(differences) - 1))
    return {"mean": mean, "se": sd / math.sqrt(len(differences)), "sd": sd, "n": len(differences)}


def split(lines, programs=(), classes=()):
    config, phases = read_log(lines)
    if not phases:
        raise ValueError("no [ab] phase lines (was APS5_AB set?)")
    table = [metrics(phase, programs, classes) for phase in phases]
    names = list(dict.fromkeys(name for row in table for name in row))
    rows = []
    for name in names:
        values = [row.get(name) for row in table]
        a = [value for phase, value in zip(phases, values) if phase["kind"] == "A" and value is not None]
        b = [value for phase, value in zip(phases, values) if phase["kind"] == "B" and value is not None]
        if a and b:
            mean_a, mean_b = sum(a) / len(a), sum(b) / len(b)
            rows.append({"metric": name, "a": mean_a, "b": mean_b, "change": mean_b - mean_a,
                         "change_pct": (mean_b / mean_a - 1) * 100 if mean_a else None, "noise": noise(values, phases)})
    return {"config": config,
            "phases": [{"kind": phase["kind"], "index": phase["index"], "begin_present": phase["begin"][0],
                        "settled_present": phase["settled"][0] if phase["settled"] else None,
                        "end_present": phase["end"][0] if phase["end"] else None,
                        "clean_reports": {name: len(reports) for name, reports in phase["reports"].items()},
                        "metrics": row} for phase, row in zip(phases, table)],
            "metrics": rows}


def text_report(result):
    lines = [result["config"] or "[ab] configuration line missing", "",
             "phase  begin / settled / end   clean reports (present/gputime/draw/syncwait)  presents/10s"]
    for phase in result["phases"]:
        settled = "-" if phase["settled_present"] is None else phase["settled_present"]
        end = "-" if phase["end_present"] is None else phase["end_present"]
        reports = "/".join(str(phase["clean_reports"][name]) for name, _ in KINDS)
        rate = phase["metrics"].get("presents/10s")
        lines.append(f"{phase['kind']} {phase['index']:<3} {phase['begin_present']:>7} / {settled:>7} / {end:>7}   {reports:<45}  "
                     + ("unfinished" if rate is None else f"{rate:.2f}"))
    lines.extend(["", f"{'metric':<30} {'A':>10} {'B':>10} {'B-A':>10} {'B/A-1':>8}   B vs neighbouring A: mean +- SE (SD, n)"])
    for row in result["metrics"]:
        spread = row["noise"]
        if spread is None:
            text = "-"
        elif spread["n"] == 1:
            text = f"{spread['mean']:+.2f} (n 1)"
        else:
            text = f"{spread['mean']:+.2f} +- {spread['se']:.2f} (SD {spread['sd']:.2f}, n {spread['n']})"
        percent = "n/a" if row["change_pct"] is None else f"{row['change_pct']:+.1f}%"
        lines.append(f"{row['metric']:<30} {row['a']:>10.2f} {row['b']:>10.2f} {row['change']:>+10.2f} {percent:>8}   {text}")
    return "\n".join(lines) + "\n"


def program_key(value):
    if not re.fullmatch(r"0x[0-9a-fA-F]+", value):
        raise argparse.ArgumentTypeError("must be a hexadecimal program key such as 0x1000")
    return "0x" + value[2:].lower().lstrip("0").rjust(1, "0")


def main(arguments=None):
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("log", help="the run's standard error log with the [ab] lines")
    parser.add_argument("--program", action="append", type=program_key, default=[], help="also report this [gputime] program key per present (repeatable)")
    parser.add_argument("--class", dest="classes", action="append", default=[], help="also report this [gputime] command class per present (repeatable)")
    parser.add_argument("--json", action="store_true", help="print the phases and metrics as JSON")
    options = parser.parse_args(arguments)
    with Path(options.log).open(encoding="utf-8", errors="replace") as source:
        result = split(source, options.program, options.classes)
    sys.stdout.write(json.dumps(result, indent=1) + "\n" if options.json else text_report(result))
    return 0


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        sys.exit(2)
