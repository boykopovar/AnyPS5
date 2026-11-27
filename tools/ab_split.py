import argparse
import codecs
import json
import math
import re
import sys
from pathlib import Path

DESCRIPTION = """Per-phase means of an in-run A/B (APS5_AB) log. The driver prints "[ab] phase <arm> <k> begins at present
<n> (t <s> s)" when a phase starts and "[ab] phase <arm> <k> settled at present <n> (t <s> s)" APS5_AB_SETTLE seconds
later; <arm> is A or B in the two-arm form and the arm's name in the multi-arm form. Each periodic report covers the time since the previous report of its kind, so a report counts for phase k
only when the previous report of its kind came after phase k's settled line and the report came before phase k+1
began. Presents per 10 s and frame time come from the [ab] lines (settled line to the next phase's begin), GPU busy
and idle gaps per frame from [present], GPU time per present from [gputime] (APS5_PROFILE_GPU), the committer and
drains from [draw] pipelined, unlocked hook waits from [syncwait] (APS5_PROFILE_DRAW). The noise column compares
each B phase with the mean of its neighbouring A phases, which cancels a linear drift: mean difference, standard
error, standard deviation and count. With more than two arms, or with APS5_AB_ROTATE, it compares each phase of an
arm with the mean of the A phases of its own cycle (the phases of one pass of the plan, as many as the [ab] header
lists arms): rotated orders put an arm before A in one cycle and after it in the next, so a linear drift cancels in
the mean of the differences. Every arm other than A gets its own row of each metric, named "<metric> [<arm>]". A counted GPU timing report that dropped ranges refuses the comparison.
A requested program's phase metric is omitted if a report lists 12 programs
without it: the list may be truncated, so the program's time is unknown.
A counted present report with missing batch completion records also refuses the comparison."""

PHASE = re.compile(r"\[ab\] phase ([A-Za-z0-9_+]+) ([0-9]+) (begins|settled) at present ([0-9]+) \(t ([0-9]+(?:\.[0-9]+)?) s\)(:.*)?")
ROTATION = re.compile(r", the arm order rotates by one at every cycle of (\d+) phases")
ARM_ENTRY = re.compile(r"; ([A-Za-z0-9_+]+): ")
KINDS = (("present", re.compile(r"\[present\] \d+ presents")), ("gputime", re.compile(r"\[gputime\] [\d.]+ ms of GPU time")),
         ("draw", re.compile(r"\[draw\] pipelined \(")), ("syncwait", re.compile(r"\[syncwait\] unlocked hook waits")))


def finite(value):
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError("non-finite A/B statistic")
    if isinstance(value, dict):
        for item in value.values():
            finite(item)
    elif isinstance(value, (list, tuple)):
        for item in value:
            finite(item)
    return value


def finite_sum(values):
    return finite(sum(values))


def number(pattern, text):
    match = re.search(pattern, text)
    return finite(float(match.group(1))) if match else None


def parse_report(kind, text):
    if kind == "present":
        presents, dropped = number(r"^\[present\] (\d+) presents", text), number(r"\((\d+) dropped", text)
        busy, gaps = number(r"GPU busy ([\d.]+) ms", text), number(r"idle gaps ([\d.]+) ms", text)
        if None in (presents, dropped, busy, gaps):
            return None
        missing = re.search(r"\((\d+) batches without a completion record\)", text)
        if missing and int(missing.group(1)):
            raise ValueError("missing batch completion records in a settled A/B phase; GPU busy and idle timing is incomplete")
        return {"frames": presents + dropped, "busy": busy, "gaps": gaps}
    if kind == "gputime":
        total, presents = number(r"^\[gputime\] ([\d.]+) ms of GPU time", text), number(r"; (\d+) presents;", text)
        if total is None or presents is None or "; by class:" not in text:
            return None
        dropped = re.search(r"; (\d+) ranges dropped at the \d+ cap\)", text)
        if dropped and int(dropped.group(1)):
            raise ValueError("GPU timing ranges were dropped in a settled A/B phase; timing statistics are incomplete")
        programs_text, classes_text = text.split("by program:", 1)[-1].split("; by class:", 1)
        programs, classes = {}, {}
        for key, ms in re.findall(r" (?<!draw )(?<!drawset )(0x[0-9a-f]+) x\d+ ([\d.]+)ms", programs_text):
            programs[key] = programs.get(key, 0.0) + finite(float(ms))
        for name, ms in re.findall(r" ([a-z][a-z-]*) x\d+ ([\d.]+)ms [\d.]+MiB \(per present", classes_text):
            classes[name] = classes.get(name, 0.0) + finite(float(ms))
        truncated = len(re.findall(r" (?:draw )?0x[0-9a-f]+ x\d+ [\d.]+ms", programs_text)) >= 12
        return {"total": total, "presents": presents, "programs": programs, "programs_truncated": truncated, "classes": classes}
    if kind == "draw":
        commits, per_commit = number(r": (\d+) commits at", text), number(r"commits at ([\d.]+) us each", text)
        enqueued, room = number(r"; (\d+) enqueued behind", text), number(r"waiting ([\d.]+) us per draw for room", text)
        if None in (commits, per_commit, enqueued, room) or "; drains:" not in text:
            return None
        drains = text.split("; drains:", 1)[1].split(";", 1)[0]
        return {"commits": commits, "per_commit": per_commit, "enqueued": enqueued, "room": room,
                "drain_ms": finite_sum(finite(float(ms)) for ms in re.findall(r"\(([\d.]+) ms\)", drains))}
    if kind == "syncwait":
        if "):" not in text:
            return None
        sites = text.split("):", 1)[1].split("; batches ahead", 1)[0]
        return {"wait_ms": finite_sum(finite(float(ms)) for ms in re.findall(r" \d+/([\d.]+)ms", sites))}
    return None


def read_log(lines):
    config, phases, current = None, [], None
    previous = {kind: None for kind, _ in KINDS}
    unmatched_settlement = None
    for line_number, line in enumerate(lines, 1):
        text = line.rstrip("\r\n")
        if text.startswith("[ab] in-run A/B:"):
            if config is not None or phases:
                raise ValueError("multiple A/B runs in one log; split the captures before comparing phases")
            config = text
            continue
        if text.startswith("[ab] phase "):
            marker = PHASE.fullmatch(text.rstrip())
            if marker is None or (marker[3] == "settled" and marker[6] is not None):
                raise ValueError(f"line {line_number}: invalid A/B phase marker")
            kind, raw_index, event, raw_present, raw_seconds, _ = marker.groups()
            index, present, seconds = int(raw_index), int(raw_present), finite(float(raw_seconds))
            point = (present, seconds)

            if event == "begins":
                if phases and index <= phases[-1]["index"]:
                    raise ValueError("phase index restarts in one log (multiple A/B runs); split the captures before comparing phases")

                if current is not None:
                    if current["settled"] is None and unmatched_settlement is not None:
                        raise ValueError(f"phase {unmatched_settlement[0]} {unmatched_settlement[1]} settled does not match active phase {current['kind']} {current['index']}")
                    previous_point = current["settled"] or current["begin"]
                    if point[0] < previous_point[0] or point[1] < previous_point[1]:
                        raise ValueError("phase began before the previous phase boundary")
                    current["end"] = point

                current = {"kind": kind, "index": index, "begin": point, "settled": None, "end": None,
                           "reports": {name: [] for name, _ in KINDS}}
                unmatched_settlement = None
                phases.append(current)

            elif current is not None:
                if current["index"] != index or current["kind"] != kind:
                    unmatched_settlement = (kind, index)
                else:
                    if current["settled"] is not None:
                        raise ValueError("phase settled more than once")
                    previous_point = current["begin"]
                    if point[0] < previous_point[0] or point[1] < previous_point[1]:
                        raise ValueError("phase settled before it began")
                    current["settled"] = point
                    unmatched_settlement = None

            continue
        for name, report_line in KINDS:
            if report_line.match(text):
                state = (current["index"], current["settled"] is not None) if current is not None else None
                if state is not None and state[1] and previous[name] == state:
                    report = parse_report(name, text)
                    if report is not None:
                        current["reports"][name].append(report)
                previous[name] = state
                break
    return config, phases


def weighted(rows, value, weight):
    total = finite_sum(row[weight] for row in rows)
    return finite_sum(row[value] * row[weight] for row in rows) / total if total else None


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
    presents = finite_sum(row["presents"] for row in rows)
    if presents:
        out["gputime ms/present"] = finite_sum(row["total"] for row in rows) / presents
        for key in programs:
            if all(key in row["programs"] or not row["programs_truncated"] for row in rows):
                out[f"{key} ms/present"] = finite_sum(row["programs"].get(key, 0.0) for row in rows) / presents
        for name in classes:
            out[f"{name} ms/present"] = finite_sum(row["classes"].get(name, 0.0) for row in rows) / presents
    rows = phase["reports"]["draw"]
    out["committer us/commit"], out["room wait us/draw"] = weighted(rows, "per_commit", "commits"), weighted(rows, "room", "enqueued")
    out["drain ms/report"] = finite_sum(row["drain_ms"] for row in rows) / len(rows) if rows else None
    rows = phase["reports"]["syncwait"]
    out["syncwait GPU ms/report"] = finite_sum(row["wait_ms"] for row in rows) / len(rows) if rows else None
    return {name: value for name, value in out.items() if value is not None}


def cycle_length(config):
    if not config:
        return None
    rotation = ROTATION.search(config)
    if rotation:
        return int(rotation.group(1))
    entries = len(ARM_ENTRY.findall(config))
    return entries if entries > 2 else None


def noise(values, phases, arm="B", cycle=None):
    differences = []
    for index, phase in enumerate(phases):
        if phase["kind"] != arm or values[index] is None:
            continue
        if cycle:
            neighbours = [values[other] for other, partner in enumerate(phases)
                          if partner["kind"] == "A" and partner["index"] // cycle == phase["index"] // cycle and values[other] is not None]
        else:
            neighbours = [values[other] for other in (index - 1, index + 1)
                          if 0 <= other < len(phases) and phases[other]["kind"] == "A"
                          and abs(phases[other]["index"] - phase["index"]) == 1 and values[other] is not None]
        if neighbours:
            differences.append(values[index] - finite_sum(neighbours) / len(neighbours))
    if not differences:
        return None
    mean = finite_sum(differences) / len(differences)
    if len(differences) == 1:
        return {"mean": mean, "se": None, "sd": None, "n": 1}
    sd = math.sqrt(finite_sum((value - mean) ** 2 for value in differences) / (len(differences) - 1))
    return {"mean": mean, "se": sd / math.sqrt(len(differences)), "sd": sd, "n": len(differences)}


def split_report(lines, programs, classes):
    programs, classes = tuple(programs), tuple(classes)
    reserved = {"gputime", *programs}
    for name in classes:
        if name in reserved:
            raise ValueError(f"--class {name} would overwrite the {name} ms/present metric")
    config, phases = read_log(lines)
    if not phases:
        raise ValueError("no [ab] phase lines (was APS5_AB set?)")
    table = [metrics(phase, programs, classes) for phase in phases]
    names = list(dict.fromkeys(name for row in table for name in row))
    arms = list(dict.fromkeys(phase["kind"] for phase in phases if phase["kind"] != "A"))
    cycle = cycle_length(config)
    rows = []
    for arm in arms:
        for name in names:
            values = [row.get(name) for row in table]
            a = [value for phase, value in zip(phases, values) if phase["kind"] == "A" and value is not None]
            b = [value for phase, value in zip(phases, values) if phase["kind"] == arm and value is not None]
            if a and b:
                mean_a, mean_b = finite_sum(a) / len(a), finite_sum(b) / len(b)
                row = {"metric": name, "a": mean_a, "b": mean_b, "change": mean_b - mean_a,
                       "change_pct": (mean_b / mean_a - 1) * 100 if mean_a else None, "noise": noise(values, phases, arm, cycle)}
                if len(arms) > 1:
                    row["metric"], row["arm"] = f"{name} [{arm}]", arm
                rows.append(row)
    return {"config": config,
            "phases": [{"kind": phase["kind"], "index": phase["index"], "begin_present": phase["begin"][0],
                        "settled_present": phase["settled"][0] if phase["settled"] else None,
                        "end_present": phase["end"][0] if phase["end"] else None,
                        "clean_reports": {name: len(reports) for name, reports in phase["reports"].items()},
                        "metrics": row} for phase, row in zip(phases, table)],
            "metrics": rows}


def split(lines, programs=(), classes=()):
    try:
        return finite(split_report(lines, programs, classes))
    except OverflowError as error:
        raise ValueError("A/B statistic exceeds the finite numeric range") from error


def text_report(result):
    cycled = cycle_length(result["config"]) is not None
    lines = [result["config"] or "[ab] configuration line missing", "",
             "phase  begin / settled / end   clean reports (present/gputime/draw/syncwait)  presents/10s"]
    for phase in result["phases"]:
        settled = "-" if phase["settled_present"] is None else phase["settled_present"]
        end = "-" if phase["end_present"] is None else phase["end_present"]
        reports = "/".join(str(phase["clean_reports"][name]) for name, _ in KINDS)
        rate = phase["metrics"].get("presents/10s")
        lines.append(f"{phase['kind']} {phase['index']:<3} {phase['begin_present']:>7} / {settled:>7} / {end:>7}   {reports:<45}  "
                     + ("unfinished" if rate is None else f"{rate:.2f}"))
    lines.extend(["", f"{'metric':<30} {'A':>10} {'B':>10} {'B-A':>10} {'B/A-1':>8}   " + ("arm vs A of its cycle" if cycled else "B vs neighbouring A") + ": mean +- SE (SD, n)"])
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
    if not re.fullmatch(r"0[xX][0-9a-fA-F]+", value):
        raise argparse.ArgumentTypeError("must be a hexadecimal program key such as 0x1000")
    return "0x" + value[2:].lower().lstrip("0").rjust(1, "0")


def open_log(path):
    with Path(path).open("rb") as raw:
        bom = raw.read(4)
    if bom in (codecs.BOM_UTF32_LE, codecs.BOM_UTF32_BE):
        raise ValueError(f"{path} is UTF-32; "
                         'capture the run with cmd /d /c "<command> 2> run.log" instead')
    if bom[:2] in (codecs.BOM_UTF16_LE, codecs.BOM_UTF16_BE):
        raise ValueError(f"{path} is UTF-16, as Windows PowerShell 5.1 writes a redirected 2>, which also wraps long lines; "
                         'capture the run with cmd /d /c "<command> 2> run.log" instead')
    return Path(path).open(encoding="utf-8-sig", errors="replace")


def main(arguments=None):
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("log", help="the run's standard error log with the [ab] lines")
    parser.add_argument("--program", action="append", type=program_key, default=[], help="also report this [gputime] program key per present (repeatable)")
    parser.add_argument("--class", dest="classes", action="append", default=[], help="also report this [gputime] command class per present (repeatable)")
    parser.add_argument("--json", action="store_true", help="print the phases and metrics as JSON")
    options = parser.parse_args(arguments)
    with open_log(options.log) as source:
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
