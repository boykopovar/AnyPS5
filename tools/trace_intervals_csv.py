import argparse
import csv
import io
from pathlib import Path
import sys

import trace_digest

COLUMNS = ("interval", "start_s", "end_s", "presents", "p50_ms", "p90_ms", "p99_ms", "max_ms",
           "gpu_busy_ms_per_present", "busiest_worker", "busiest_worker_pct", "top_phase",
           "top_phase_ms_per_frame", "trace_dropped", "identity_changed")


def render(result):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=COLUMNS)
    writer.writeheader()
    changed = set(result["identity_changed_at"])
    for index, interval in enumerate(result["intervals"]):
        worker = interval["busiest_worker"] or {}
        phase = interval["top_phase"] or {}
        row = {key: interval[key] for key in ("start_s", "end_s", "presents", "gpu_busy_ms_per_present", "trace_dropped")}
        row.update({key + "_ms": interval["frame_ms"][key] for key in ("p50", "p90", "p99", "max")})
        row.update(interval=index + 1, busiest_worker=worker.get("thread"), busiest_worker_pct=worker.get("pct"),
                   top_phase=phase.get("name"), top_phase_ms_per_frame=phase.get("ms_per_frame"), identity_changed=index in changed)
        writer.writerow(row)
    return output.getvalue()


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace", newline="")
    parser = argparse.ArgumentParser(description="Export trace interval summaries as CSV")
    parser.add_argument("summary", type=Path)
    args = parser.parse_args(argv)
    try:
        text = render(trace_digest.digest_intervals(trace_digest.load_intervals(args.summary)))
    except (EOFError, OSError, ValueError, KeyError, TypeError, RecursionError) as error:
        parser.error(str(error))
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
