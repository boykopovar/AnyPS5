import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

DEVICE_LIMITATION = re.compile(r"skipped,.*(subgroup|the device has no|the device reports)", re.IGNORECASE)

WINDOWS_PREFIX = "agc"


def skips(path):
    for case in ET.parse(path).getroot().iter("testcase"):
        if case.find("skipped") is None:
            continue
        name = case.get("name")
        if not name:
            continue
        output = case.find("system-out")
        yield name, (output.text or "") if output is not None else ""


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="fail when a test is skipped that the job's device is not expected to skip")
    parser.add_argument("junit", type=Path, help="the file written by ctest --output-junit")
    parser.add_argument("--job", choices=("linux", "windows"), default="linux")
    parser.add_argument("--list", action="store_true", help="print the skipped tests and why the file says they skipped")
    args = parser.parse_args()
    found = sorted(skips(args.junit))
    if args.list:
        for name, output in found:
            reason = next((line.strip() for line in output.splitlines() if line.startswith("skipped")), "")
            print(f"{name}: {reason}")
        raise SystemExit
    if args.job == "windows":
        unexpected = [name for name, _ in found if not name.startswith(WINDOWS_PREFIX)]
        expected = f"any {WINDOWS_PREFIX}* test, which the runner has no Vulkan driver for"
    else:
        unexpected = [name for name, output in found if not DEVICE_LIMITATION.search(output)]
        expected = "any test that reports the device limitation it needs, such as narrow subgroups or a missing extension"
    for name in unexpected:
        print(f"error: {name} skipped")
    if unexpected:
        print(f"{len(unexpected)} test(s) skipped for a reason that is not expected: the job lost that coverage and ctest still reports every test as passed")
        raise SystemExit(1)
    print(f"{len(found)} test(s) skipped, all expected ({expected})")
