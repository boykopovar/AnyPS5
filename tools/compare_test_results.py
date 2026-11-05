import re
import xml.etree.ElementTree as ET


OUTCOMES = ("passed", "failed", "skipped")
CTEST_STATUSES = {None, "run", "fail", "notrun", "disabled"}
CTEST_SKIP = re.compile(r"SKIP_RETURN_CODE=-?[0-9]+|SKIP_REGULAR_EXPRESSION_MATCHED", re.IGNORECASE)


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
