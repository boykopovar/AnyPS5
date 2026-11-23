import argparse
import csv
import io
from pathlib import Path
import sys

import ab_split
from performance_csv import safe_cell

COLUMNS = ("config", "phases", "metric", "a", "b", "change", "change_pct",
           "noise_mean", "noise_se", "noise_sd", "noise_n")


def render(result, spreadsheet_safe=False):
    cell = safe_cell if spreadsheet_safe else lambda value: value
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=COLUMNS,
                            quoting=csv.QUOTE_NONNUMERIC if spreadsheet_safe else csv.QUOTE_MINIMAL)
    writer.writeheader()
    for metric in result["metrics"]:
        row = {key: metric[key] for key in ("a", "b", "change", "change_pct")}
        row.update(metric=cell(metric["metric"]), config=cell(result["config"]), phases=len(result["phases"]))
        noise = metric["noise"] or {}
        row.update({"noise_" + key: noise.get(key) for key in ("mean", "se", "sd", "n")})
        writer.writerow(row)
    return output.getvalue()


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace", newline="")
    parser = argparse.ArgumentParser(description="Export the existing in-run A/B comparison as CSV")
    parser.add_argument("log", type=Path)
    parser.add_argument("--program", action="append", type=ab_split.program_key, default=[])
    parser.add_argument("--class", dest="classes", action="append", default=[])
    parser.add_argument("--spreadsheet-safe", action="store_true",
                        help="opt-in tab prefix changes text cells; spreadsheet behavior varies and save/reopen not guaranteed")
    args = parser.parse_args(argv)
    try:
        with ab_split.open_log(args.log) as lines:
            text = render(ab_split.split(lines, args.program, args.classes), args.spreadsheet_safe)
    except (OSError, ValueError, OverflowError) as error:
        parser.error(str(error))
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
