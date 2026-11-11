import argparse
import csv
import io
import re
import sys
from pathlib import Path

import performance_compare

DANGEROUS_TRIGGER = re.compile(r"^\s*([\t\r\n]|[=+\-@\uff1d\uff0b\uff0d\uff20])")


def safe_cell(value):
    if isinstance(value, str) and DANGEROUS_TRIGGER.match(value):
        return "\t" + value
    return value


def configuration_cell(value):
    if value is None:
        return "<missing>"
    if isinstance(value, str) and (value == "<missing>" or value.startswith("\\")):
        return "\\" + value
    return value


def render_csv(report, spreadsheet_safe=False):
    cell = safe_cell if spreadsheet_safe else lambda value: value
    output = io.StringIO(newline="")
    writer = csv.writer(output, quoting=csv.QUOTE_NONNUMERIC if spreadsheet_safe else csv.QUOTE_MINIMAL)
    writer.writerow(("kind", "name", "unit", "baseline", "candidate", "change", "change_pct"))
    for field, unit in (("commit", ""), ("trace", ""), ("presents", "count")):
        writer.writerow(("capture", cell(field), cell(unit), cell(report["baseline"][field]),
                         cell(report["candidate"][field]), "", ""))
    writer.writerow(("capture", "window.ms", "ms", cell(report["baseline"]["window"]["ms"]),
                     cell(report["candidate"]["window"]["ms"]), "", ""))
    for kind, key, unit in (("metric", "metrics_ms", "ms"), ("phase", "top_phases_ms_per_frame", "ms/frame"),
                            ("worker", "worker_busy_pct", "%")):
        for name, values in report[key].items():
            writer.writerow((cell(kind), cell(name), cell(unit), *(cell(values[field]) for field in ("baseline", "candidate", "change", "change_pct"))))
    for name, values in report["configuration_differences"].items():
        writer.writerow(("configuration", cell(name), "", cell(configuration_cell(values["baseline"])),
                         cell(configuration_cell(values["candidate"])), "", ""))
    for note in report["notes"]:
        writer.writerow(("note", cell(note), "", "", "", "", ""))
    return output.getvalue()


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace", newline="")
    parser = argparse.ArgumentParser(description="Export a performance comparison as CSV. Empty metric cells mean unreported or unavailable, not zero.")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--baseline-trace", help="exact trace field to select in the baseline JSONL")
    parser.add_argument("--candidate-trace", help="exact trace field to select in the candidate JSONL")
    parser.add_argument("--require-matching-context", action="store_true")
    parser.add_argument("--spreadsheet-safe", action="store_true",
                        help="opt-in tab prefix changes text cells; spreadsheet behavior varies and save/reopen not guaranteed")
    args = parser.parse_args(argv)
    try:
        baseline = performance_compare.load_summary(args.baseline, args.baseline_trace)
        candidate = performance_compare.load_summary(args.candidate, args.candidate_trace)
        report = performance_compare.compare(baseline, candidate)
        if args.require_matching_context and report["configuration_differences"]:
            raise ValueError("recorded configuration differs: " + ", ".join(report["configuration_differences"]))
        output = render_csv(report, spreadsheet_safe=args.spreadsheet_safe)
    except (OSError, ValueError, OverflowError, RecursionError) as error:
        parser.exit(2, f"{parser.prog}: {error}\n")
    sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
