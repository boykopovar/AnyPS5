import argparse
import html
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


OUTCOMES = ("passed", "failed", "skipped")
CTEST_STATUSES = {None, "run", "fail", "notrun", "disabled"}
CTEST_SKIP = re.compile(r"SKIP_RETURN_CODE=-?[0-9]+|SKIP_REGULAR_EXPRESSION_MATCHED", re.IGNORECASE)
CHANGES = {
    "new_failure": "New failure",
    "existing_failure": "Existing failure",
    "fixed": "Fixed",
    "newly_skipped": "Newly skipped",
    "resumed": "Resumed",
    "added": "Added",
    "missing": "Missing",
    "unchanged": "Unchanged",
}


def outcome(case):
    status = case.get("status")
    if status not in CTEST_STATUSES:
        raise ValueError(f"testcase {case.get('name')!r}: unknown CTest status {status!r}")
    if case.find("failure") is not None or case.find("error") is not None or status == "fail":
        return "failed"
    skipped = case.find("skipped")
    reason = (skipped.get("message") or "").strip() if skipped is not None else ""
    if status == "disabled" or (skipped is not None and (status != "notrun" or CTEST_SKIP.fullmatch(reason))):
        return "skipped"
    return "failed" if status == "notrun" else "passed"


def load_results(path):
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError(f"{path}: expected a JUnit testsuite or testsuites root, got {root.tag!r}")
    found = {}
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name:
            raise ValueError(f"{path}: testcase has no name")
        if name in found:
            raise ValueError(f"{path}: testcase {name!r} is listed more than once")
        try:
            found[name] = outcome(case)
        except ValueError as error:
            raise ValueError(f"{path}: {error}") from error
    if not found:
        raise ValueError(f"{path}: the report lists no tests")
    for suite in root.iter():
        if suite.tag not in ("testsuite", "testsuites") or "tests" not in suite.attrib:
            continue
        count = suite.get("tests")
        if not re.fullmatch(r"[0-9]+", count) or int(count) != sum(1 for _ in suite.iter("testcase")):
            raise ValueError(f"{path}: declared test count {count!r} does not match the report")
    return found


def change(before, after):
    if after is None:
        return "missing"
    if after == "failed":
        return "existing_failure" if before == "failed" else "new_failure"
    if before is None:
        return "added"
    if before == after:
        return "unchanged"
    if after == "skipped":
        return "newly_skipped"
    return "fixed" if before == "failed" else "resumed"


def counts(results):
    return {"total": len(results), **{name: sum(value == name for value in results.values()) for name in OUTCOMES}}


def compare(baseline, candidate):
    tests = [{"name": name, "baseline": baseline.get(name), "candidate": candidate.get(name),
              "change": change(baseline.get(name), candidate.get(name))}
             for name in sorted(baseline.keys() | candidate.keys())]
    changes = {name: sum(test["change"] == name for test in tests) for name in CHANGES}
    return {"format": "ctest-comparison/1", "baseline": counts(baseline), "candidate": counts(candidate),
            "has_regressions": any(changes[name] for name in ("new_failure", "newly_skipped", "missing")),
            "changes": changes, "tests": tests}


def cell(value):
    value = html.escape(value).replace("|", "&#124;")
    for character in "\\`*_[]!~":
        value = value.replace(character, f"&#{ord(character)};")
    for character, entity in (("\r", "&#13;"), ("\n", "&#10;"), ("\t", "&#9;")):
        value = value.replace(character, entity)
    return "<code>" + value + "</code>"


def markdown(report):
    lines = ["# CTest comparison", "", "| Run | Total | Passed | Failed | Skipped |",
             "| --- | ---: | ---: | ---: | ---: |"]
    for label in ("baseline", "candidate"):
        count = report[label]
        lines.append(f"| {label} | {count['total']} | {count['passed']} | {count['failed']} | {count['skipped']} |")
    lines += ["", "| Change | Count |", "| --- | ---: |"]
    lines += [f"| {label} | {report['changes'][name]} |" for name, label in CHANGES.items()]
    lines += ["", "Regression check: " + ("failed." if report["has_regressions"] else "passed.")]
    shown = [test for test in report["tests"] if test["change"] != "unchanged"]
    if shown:
        lines += ["", "| Test | Baseline | Candidate | Change |", "| --- | --- | --- | --- |"]
        lines += [f"| {cell(test['name'])} | {test['baseline'] or 'absent'} | {test['candidate'] or 'absent'} | "
                  f"{CHANGES[test['change']]} |" for test in shown]
    return "\n".join(lines) + "\n"


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare CTest JUnit outcomes by test name. "
                                     "New failures, newly skipped tests and missing tests are regressions.")
    parser.add_argument("baseline", type=Path, help="baseline report from ctest --output-junit")
    parser.add_argument("candidate", type=Path, help="candidate report from the same CTest selection")
    parser.add_argument("--json", action="store_true", help="write all test outcomes as JSON instead of a Markdown summary")
    args = parser.parse_args(argv)
    try:
        report = compare(load_results(args.baseline), load_results(args.candidate))
        output = json.dumps(report, ensure_ascii=False, indent=2) + "\n" if args.json else markdown(report)
    except (OSError, ET.ParseError, ValueError) as error:
        print(f"compare_test_results: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(output)
    return 1 if report["has_regressions"] else 0


if __name__ == "__main__":
    sys.exit(main())
