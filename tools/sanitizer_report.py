import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

FAILED_STATUSES = ("fail", "notrun")
SKIPPED_STATUSES = ("disabled",)
CTEST_SKIP = re.compile(r"SKIP_RETURN_CODE=\d+|SKIP_REGULAR_EXPRESSION_MATCHED", re.IGNORECASE)
SANITIZERS = ("address", "undefined", "thread")
REPORT_LINES = (
    re.compile(r"SUMMARY: \w+Sanitizer: .*"),
    re.compile(r"(?:ERROR|WARNING): \w+Sanitizer: .*"),
    re.compile(r"(?:\S+ )?runtime error: .*"),
)
REPORT_LENGTH = 200


def results(path):
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError(f"{path}: expected a JUnit testsuite or testsuites root, got {root.tag!r}")
    found = {}
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name:
            raise ValueError(f"{path}: testcase has no name")
        if name in found:
            raise ValueError(f"{path}: testcase {name} is listed more than once")
        skipped, status = case.find("skipped"), case.get("status")
        reason = (skipped.get("message") or "").strip() if skipped is not None else ""
        if status in SKIPPED_STATUSES or (skipped is not None and (CTEST_SKIP.fullmatch(reason) or status not in FAILED_STATUSES)):
            outcome = "skipped"
        elif status in FAILED_STATUSES or case.find("failure") is not None or case.find("error") is not None:
            outcome = "failed"
        else:
            outcome = "passed"
        found[name] = outcome
    if not found:
        raise ValueError(f"{path}: the report lists no tests")
    return found


def first_report(output):
    for pattern in REPORT_LINES:
        if match := pattern.search(output):
            line = " ".join(match.group(0).split())
            return line if len(line) <= REPORT_LENGTH else line[:REPORT_LENGTH - 3] + "..."
    return ""


def reports(path):
    found = {}
    for case in ET.parse(path).getroot().iter("testcase"):
        output = "\n".join(case.findtext(channel) or "" for channel in ("system-out", "system-err"))
        if report := first_report(output):
            found[case.get("name")] = report
    return found


def load_known(path, sanitizers):
    known, seen = {}, set()
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        location = f"{path}:{number}"
        fields = line.split(None, 2)
        if len(fields) < 3:
            raise ValueError(f"{location}: expected a test name, its sanitizers and a reason")
        test, names, reason = fields[0], fields[1].split(","), fields[2].strip()
        if any(name not in SANITIZERS for name in names):
            raise ValueError(f"{location}: sanitizers must be a comma-separated list of {', '.join(SANITIZERS)}")
        for name in names:
            if (test, name) in seen:
                raise ValueError(f"{location}: {test} is already listed for {name}")
            seen.add((test, name))
        if set(names) & set(sanitizers):
            known[test] = reason
    return known


def cell(text):
    return " ".join(text.split()).replace("|", "\\|")


def summarize(title, found, known=None, details=None):
    known, details = known or {}, details or {}
    counts = {outcome: sum(1 for value in found.values() if value == outcome) for outcome in ("passed", "skipped", "failed")}
    failed = sorted(name for name, outcome in found.items() if outcome == "failed" and name not in known)
    expected = sorted(name for name, outcome in found.items() if outcome == "failed" and name in known)
    tally = f"{counts['failed']} failed" + (f" ({len(expected)} known)" if expected else "")
    lines = [f"### {title}", "",
             f"{len(found)} tests: {counts['passed']} passed, {counts['skipped']} skipped, {tally}.", ""]
    if failed and any(name in details for name in failed):
        lines += ["| Failed test | First sanitizer report |", "| - | - |"]
        lines += [f"| `{name}` | {cell(details.get(name, ''))} |" for name in failed] + [""]
    elif failed:
        lines += ["| Failed test |", "| - |"] + [f"| `{name}` |" for name in failed] + [""]
    if expected:
        lines += ["| Known failure | Reason |", "| - | - |"] + [f"| `{name}` | {cell(known[name])} |" for name in expected] + [""]
    recovered = sorted(name for name in known if found.get(name) == "passed")
    if recovered:
        lines += ["| Listed as known but passed | Reason |", "| - | - |"] + [f"| `{name}` | {cell(known[name])} |" for name in recovered] + [""]
    absent = sorted(name for name in known if name not in found)
    if absent:
        lines += ["| Listed as known but not in this run |", "| - |"] + [f"| `{name}` |" for name in absent] + [""]
    return "\n".join(lines) + "\n", bool(failed)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="summarize a sanitizer test run and fail when a test failed")
    parser.add_argument("junit", type=Path, help="the file written by ctest --output-junit")
    parser.add_argument("--title", default="Sanitizer run", help="heading of the summary")
    parser.add_argument("--summary", type=Path, help="also append the summary to this file, such as $GITHUB_STEP_SUMMARY")
    parser.add_argument("--known", type=Path,
                        help="known failures, one per line: test name, comma-separated sanitizers and the reason")
    parser.add_argument("--sanitizer", action="append", choices=SANITIZERS, default=[],
                        help="sanitizer the build used; repeat for several. Selects the --known entries that apply")
    args = parser.parse_args(argv)
    if args.known and not args.sanitizer:
        parser.error("--known needs at least one --sanitizer")
    try:
        if args.summary:
            for source in (args.junit, args.known):
                if source is not None and (source.resolve() == args.summary.resolve() or
                                           (source.exists() and args.summary.exists() and source.samefile(args.summary))):
                    raise ValueError(f"summary output must differ from input: {source}")
        known = load_known(args.known, args.sanitizer) if args.known else {}
        text, failing = summarize(args.title, results(args.junit), known, reports(args.junit))
        if args.summary:
            with args.summary.open("a", encoding="utf-8") as summary:
                summary.write(text)
    except (OSError, ET.ParseError, ValueError, RuntimeError) as error:
        print(f"sanitizer_report: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(text)
    return 1 if failing else 0


if __name__ == "__main__":
    sys.exit(main())
