import argparse
import csv
import io
from pathlib import Path
import sys

from performance_csv import safe_cell
import trace_digest

COLUMNS = ("interval", "start_s", "end_s", "presents", "p50_ms", "p90_ms", "p99_ms", "max_ms",
           "gpu_busy_ms_per_present", "busiest_worker", "busiest_worker_pct", "top_phase",
           "top_phase_ms_per_frame", "trace_dropped", "identity_changed")


def render(result, spreadsheet_safe=False):
    cell = safe_cell if spreadsheet_safe else lambda value: value
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=COLUMNS,
                            quoting=csv.QUOTE_NONNUMERIC if spreadsheet_safe else csv.QUOTE_MINIMAL)
    writer.writeheader()
    changed = set(result["identity_changed_at"])
    for index, interval in enumerate(result["intervals"]):
        worker = interval["busiest_worker"] or {}
        phase = interval["top_phase"] or {}
        row = {key: interval[key] for key in ("start_s", "end_s", "presents", "gpu_busy_ms_per_present")}
        row.update({key + "_ms": interval["frame_ms"][key] for key in ("p50", "p90", "p99", "max")})
        row.update(interval=index + 1, busiest_worker=cell(worker.get("thread")), busiest_worker_pct=worker.get("pct"),
                   top_phase=cell(phase.get("name")), top_phase_ms_per_frame=phase.get("ms_per_frame"),
                   trace_dropped=str(interval["trace_dropped"]), identity_changed=str(index in changed))
        writer.writerow(row)
    return output.getvalue()


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace", newline="")
    parser = argparse.ArgumentParser(description="Export trace interval summaries as CSV")
    parser.add_argument("summary", type=Path)
    parser.add_argument("--require-complete", action="store_true",
                        help="require a nonempty capture with contiguous times and present ranges, unique intervals, "
                             "no dropped events and unchanged identity")
    parser.add_argument("--spreadsheet-safe", action="store_true",
                        help="opt-in tab prefix changes text cells; spreadsheet behavior varies and save/reopen not guaranteed")
    args = parser.parse_args(argv)
    try:
        records = trace_digest.load_intervals(args.summary)
        result = trace_digest.digest_intervals(records)
        if args.require_complete:
            intervals = result["intervals"]
            if not intervals or result["identity_changed_at"] or any(row["trace_dropped"] for row in intervals):
                raise ValueError("capture is empty, has dropped events or changing identity")
            previous_start, previous_end = intervals[0]["start_s"], intervals[0]["end_s"]
            for index, row in enumerate(intervals[1:], start=2):
                start, end = row["start_s"], row["end_s"]
                if start != previous_end:
                    raise ValueError(f"interval {index} starts at {start} s; previous interval ends at {previous_end} s")
                if (start, end) == (previous_start, previous_end):
                    raise ValueError(f"interval {index} duplicates the previous interval at {start} s")
                previous_start, previous_end = start, end
            expected_present = None
            for index, record in enumerate(records, start=1):
                window = record.get("window")
                first = window.get("first_present") if isinstance(window, dict) else None
                if type(first) is not int or first < 0:
                    raise ValueError(f"interval {index} window.first_present must be a non-negative integer")
                if expected_present is not None and first != expected_present:
                    raise ValueError(f"interval {index} starts at present {first}; expected present {expected_present}")
                expected_present = first + record["presents"]
        text = render(result, args.spreadsheet_safe)
    except (EOFError, OSError, ValueError, KeyError, TypeError, RecursionError) as error:
        parser.error(str(error))
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
