#!/usr/bin/env python3
"""Build and run AnyPS5's tests that pass in a build for a lifted guest.

`lifted_tests.py --build <directory>` builds the tests that tests/lifted_tests.txt names in an AnyPS5 build configured
with Ninja, ANYPS5_GUEST_LIFTED and BUILD_TESTING, and runs them with ctest."""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS = ROOT / "tests" / "lifted_tests.txt"


def output(command):
    return subprocess.run(command, check=True, capture_output=True, text=True).stdout


def numbers(build):
    tests = json.loads(output(["ctest", "--test-dir", build, "--show-only=json-v1"]))["tests"]
    return {test["name"]: number for number, test in enumerate(tests, 1)}


def commands(build):
    found = {}
    for testfile in Path(build).rglob("CTestTestfile.cmake"):
        for line in testfile.read_text().splitlines():
            if match := re.fullmatch(r'add_test\(\[=\[(.+?)\]=\] (.*)\)', line):
                found[match.group(1)] = re.findall(r'"((?:[^"\\]|\\.)*)"', match.group(2))
    return found


def targets_for(build, names):
    outputs = {line.split(":", 1)[0] for line in output(["ninja", "-C", build, "-t", "targets", "all"]).splitlines()}
    tests = commands(build)
    return sorted({path for name in names for token in re.findall(r"[^\s'\"]+", " ".join(tests[name]))
                   if (path := os.path.relpath(token, build)) in outputs})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", required=True, help="AnyPS5's build directory")
    parser.add_argument("--jobs", type=int, default=os.cpu_count())
    args = parser.parse_args()
    names = TESTS.read_text().split()
    tests = numbers(args.build)
    unknown = [name for name in names if name not in tests]
    if unknown:
        raise SystemExit(f"{TESTS} names tests that {args.build} does not register: {' '.join(unknown)}")
    subprocess.run(["ninja", "-C", args.build, "-j", str(args.jobs), *targets_for(args.build, names)], check=True)
    selection = "0,0,0," + ",".join(str(tests[name]) for name in names)
    return subprocess.run(["ctest", "--test-dir", args.build, "-I", selection, "--parallel", str(args.jobs), "--timeout", "30",
                           "--output-on-failure", "--no-tests=error"]).returncode


if __name__ == "__main__":
    sys.exit(main())
