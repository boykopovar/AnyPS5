"""Validate the reports that `run_report_tests --dump <folder>` writes against docs/run-report.schema.json.

Usage: validate_run_report_dump.py <folder | run_report_tests executable>. An executable is run with --dump first. Needs the pinned jsonschema (tools/requirements-run-report.txt).
Exits non-zero when a report is invalid or when the folder holds no report at all.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = json.loads((ROOT / "docs/run-report.schema.json").read_text(encoding="utf-8"))
MAX_BYTES = 65536


def main():
    folder = Path(sys.argv[1])
    if folder.is_file():
        scratch = tempfile.mkdtemp(prefix="run-report-dump-")
        subprocess.run([str(folder), "--dump", scratch], check=True)
        folder = Path(scratch)
    reports = sorted(folder.glob("*.json"))
    if not reports:
        print(f"error: no report in {folder}", file=sys.stderr)
        return 1
    validator = jsonschema.Draft202012Validator(SCHEMA)
    failures = 0
    for report in reports:
        raw = report.read_bytes()
        errors = [f"size {len(raw)} exceeds {MAX_BYTES}"] if len(raw) > MAX_BYTES else []
        errors += [f"{'/'.join(map(str, error.absolute_path))}: {error.message}" for error in validator.iter_errors(json.loads(raw))]
        print(f"{'FAIL' if errors else 'PASS'} {report.name}")
        for error in errors:
            print(f"  {error}")
        failures += bool(errors)
    print(f"{len(reports) - failures} valid, {failures} invalid")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
