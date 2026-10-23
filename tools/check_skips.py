import sys
import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

DEVICE_LIMITATION = re.compile(r"skipped,.*(subgroup|the device (has no|reports|lacks|does not|imports no|flushes|binds fewer)|no display|no usable Vulkan device|host imports are unavailable)", re.IGNORECASE)

NO_DISPLAY = re.compile(r"skipped,.*no display", re.IGNORECASE)

NO_VULKAN_DEVICE = re.compile(r"skipped,.*no usable Vulkan device", re.IGNORECASE)

HIGH_REFRESH_LIMITATION = re.compile(r"current display does not provide 120 Hz", re.IGNORECASE)

WINDOWS_PREFIX = "agc"


def high_refresh_limitation(output):
    reasons = []
    for line in output.splitlines():
        reason = line.strip()
        if reason.lower().startswith("skipped,"):
            reason = reason[len("skipped,"):].strip()
        if reason:
            reasons.append(reason)
    return bool(reasons) and all(HIGH_REFRESH_LIMITATION.fullmatch(reason) for reason in reasons)


def skips(path):
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError(f"{path}: expected a JUnit testsuite or testsuites root, got {root.tag!r}")
    for case in root.iter("testcase"):
        if case.get("status") == "disabled":
            raise ValueError(f"{path}: testcase {case.get('name')!r} is disabled and was not run")
        skipped = case.find("skipped")
        if skipped is None:
            continue
        name = case.get("name")
        if not name:
            raise ValueError(f"{path}: skipped testcase has no name")
        output = ["skipped, " + reason.strip() for reason in (skipped.get("message", ""), skipped.text or "") if reason.strip()]
        output += [case.findtext(channel) or "" for channel in ("system-out", "system-err")]
        yield name, "\n".join(output)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description="fail when a test is skipped that the job's device is not expected to skip")
    parser.add_argument("junit", type=Path, help="the file written by ctest --output-junit")
    parser.add_argument("--job", choices=("linux", "windows"), default="linux")
    parser.add_argument("--list", action="store_true", help="print the skipped tests and why the file says they skipped")
    args = parser.parse_args(argv)
    try:
        found = sorted(skips(args.junit))
    except (OSError, ET.ParseError, ValueError) as error:
        parser.error(str(error))
    if args.list:
        for name, output in found:
            reason = next((line.strip() for line in output.splitlines() if line.strip().lower().startswith("skipped")), "")
            print(f"{name}: {reason}")
        return 0
    if args.job == "windows":
        unexpected = [name for name, output in found if not name.startswith(WINDOWS_PREFIX) and not NO_DISPLAY.search(output) and not NO_VULKAN_DEVICE.search(output) and not high_refresh_limitation(output)]
        expected = f"any {WINDOWS_PREFIX}* test, which the runner has no Vulkan driver for, or a test that reports no display or Vulkan device or a display without a 120 Hz mode"
    else:
        unexpected = [name for name, output in found if not DEVICE_LIMITATION.search(output) and not high_refresh_limitation(output)]
        expected = "any test that reports the device limitation it needs, such as narrow subgroups, a missing extension, no display for a Vulkan window or a display without a 120 Hz mode"
    for name in unexpected:
        print(f"error: {name} skipped")
    if unexpected:
        print(f"{len(unexpected)} test(s) skipped for a reason that is not expected: the job lost that coverage and ctest still reports every test as passed")
        return 1
    print(f"{len(found)} test(s) skipped, all expected ({expected})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
