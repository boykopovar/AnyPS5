import argparse
import html
import os
import sys
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path


def result(test):
    status = test.get("status")
    if status == "run":
        return "Passed", ""
    if status == "fail":
        failure = test.find("failure")
        return "Failed", failure.get("message", "") if failure is not None else ""
    if status == "disabled":
        return "Not run", "Disabled"
    if status == "notrun":
        skipped = test.find("skipped")
        reason = skipped.get("message", "") if skipped is not None else ""
        return ("Skipped" if reason.startswith("SKIP_") else "Not run"), reason
    raise ValueError(f"Unknown CTest status: {status!r}")


def cell(value):
    return html.escape(value).replace("|", "\\|").replace("\n", " ").replace("\r", " ")


def summarize(path):
    root = ET.parse(path).getroot()
    if root.tag != "testsuite":
        raise ValueError("Expected a CTest JUnit testsuite")
    tests = root.findall("testcase")
    if not tests:
        raise ValueError("CTest report contains zero tests; no execution evidence is available")
    if int(root.get("tests", "-1")) != len(tests):
        raise ValueError("CTest report test count does not match its results")
    outcomes = [(test, *result(test)) for test in tests]
    counts = Counter(status for _, status, _ in outcomes)
    lines = ["### CTest results", "", "| Selected | Passed | Failed | Skipped | Not run |",
             "| --- | --- | --- | --- | --- |",
             f"| {len(tests)} | {counts['Passed']} | {counts['Failed']} | {counts['Skipped']} | {counts['Not run']} |",
             "", "Counts describe the selected test cases in this CTest report, not individual repeat attempts.",
             "Skipped tests ran and requested a skip. Not-run tests include disabled tests and tests blocked by failed fixtures.",
             "The CTest artifact retains names, durations and captured output."]
    incomplete = [(test, status, reason) for test, status, reason in outcomes if status != "Passed"]
    if incomplete:
        lines += ["", "| Test | Result | Reason |", "| --- | --- | --- |"]
        lines += [f"| {cell(test.get('name', ''))} | {status} | {cell(reason)} |"
                  for test, status, reason in incomplete]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Summarize CTest JUnit execution and skip counts")
    parser.add_argument("report", type=Path)
    args = parser.parse_args()
    try:
        summary = summarize(args.report)
        code = 0
    except (OSError, ET.ParseError, ValueError) as error:
        summary = f"### CTest results\n\nNo usable CTest report: {cell(str(error))}.\n"
        code = 1
    print(summary)
    destination = os.environ.get("GITHUB_STEP_SUMMARY")
    if destination:
        with open(destination, "a", encoding="utf-8") as stream:
            stream.write(summary)
    return code


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
