import argparse
import csv
import io
from pathlib import Path
import sys

import ab_split

COLUMNS = ("config", "phases", "metric", "a", "b", "change", "change_pct",
           "noise_mean", "noise_se", "noise_sd", "noise_n")


def render(result):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=COLUMNS)
    writer.writeheader()
    for metric in result["metrics"]:
        row = {key: metric[key] for key in ("metric", "a", "b", "change", "change_pct")}
        row.update(config=result["config"], phases=len(result["phases"]))
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
    args = parser.parse_args(argv)
    try:
        with args.log.open(encoding="utf-8-sig", errors="replace") as lines:
            text = render(ab_split.split(lines, args.program, args.classes))
    except (OSError, ValueError, OverflowError) as error:
        parser.error(str(error))
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
