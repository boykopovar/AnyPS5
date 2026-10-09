import argparse
import re
import subprocess
from pathlib import Path

TEST_OUTPUT = r"tests[\\/]"

KNOWN_UNRUN = {
    "agc_driver_mesh_tests",
    "agc_driver_recorder_tests",
    "video_out_flip_tests",
}


def built(build):
    listed = subprocess.run(["ninja", "-C", str(build), "-t", "targets", "all"],
                            capture_output=True, text=True, check=True).stdout
    names = set()
    for line in listed.splitlines():
        path, _, rule = line.partition(": ")
        if "EXECUTABLE" not in rule:
            continue
        found = re.match(rf"^{TEST_OUTPUT}([A-Za-z0-9_.]+)$", path)
        if not found:
            continue
        name = found.group(1).rstrip(".")
        if name.endswith(".exe"):
            name = name[:-4]
        if name:
            names.add(name)
    return names


def registered(build):
    names = set()
    for file in Path(build).rglob("CTestTestfile.cmake"):
        for line in file.read_text(errors="replace").splitlines():
            found = re.match(r"\s*add_test\s*\((.*)\)\s*$", line)
            if not found:
                continue
            for token in re.findall(r'"([^"]*)"|(\S+)', found.group(1)):
                argument = token[0] or token[1]
                base = re.split(r"[\\/]", argument)[-1]
                if base.endswith(".exe"):
                    base = base[:-4]
                if base:
                    names.add(base)
    return names


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="fail when a test executable is built that no ctest test runs")
    parser.add_argument("build", type=Path, nargs="?", default=Path("build"), help="configured build directory")
    parser.add_argument("--list", action="store_true", help="print every test executable and whether ctest runs it")
    args = parser.parse_args()
    every = sorted(built(args.build))
    run = registered(args.build)
    if args.list:
        for name in every:
            print(f"{'run' if name in run else 'NOT RUN'}  {name}")
        raise SystemExit
    unrun = sorted(set(every) - run)
    if not every:
        print(f"no test executables found under {args.build}; is it configured with -DBUILD_TESTING=ON?")
        raise SystemExit(1)
    for name in sorted(set(unrun) & KNOWN_UNRUN):
        print(f"note: {name} is built and not run, as recorded")
    stale = sorted(KNOWN_UNRUN & run)
    for name in stale:
        print(f"error: {name} is run by ctest now; remove it from KNOWN_UNRUN")
    fresh = sorted(set(unrun) - KNOWN_UNRUN)
    for name in fresh:
        print(f"error: {name} is built but no ctest test runs it")
    if fresh:
        print(f"{len(fresh)} test executable(s) would never run: ctest reports a full pass over what it knows, so a test nobody registered is invisible in CI")
    if stale or fresh:
        raise SystemExit(1)
    print(f"{len(every)} test executable(s), {len(unrun)} built and not run as recorded, none new")
