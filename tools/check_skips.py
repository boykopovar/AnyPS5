import sys
import json
import argparse
import re
import xml.etree.ElementTree as ET
from pathlib import Path

DEVICE_LIMITATION = re.compile(r"skipped,.*(subgroup|the device (has no|reports|lacks|does not|imports (no|shared memory through host pointers\b)|flushes|binds fewer)|the host (has not enough free disk space|cannot allocate a buffer|has no interface that can join|mounts /dev/shm noexec|has no image (segment )?that (ends|starts) inside a 16 KiB page)|no display|no usable Vulkan device|host imports are unavailable|host imports unavailable|host imports? (is|are) compared, not (write-)?watched|host import of the test block refused|unproven on CPU Vulkan \(llvmpipe\)|not enough free disk space|cannot create a raw socket)", re.IGNORECASE)

HOST_LIMITATION = re.compile(r"^skipping: (not enough free disk space for a file|cannot allocate a buffer) of more than 2 GiB", re.IGNORECASE | re.MULTILINE)

NO_DISPLAY = re.compile(r"skipped,.*no display", re.IGNORECASE)

NO_VULKAN_DEVICE = re.compile(r"skipped,.*no usable Vulkan device", re.IGNORECASE)

NO_RAW_SOCKET = re.compile(r"skipped,.*cannot create a raw socket", re.IGNORECASE)

HIGH_REFRESH_LIMITATION = re.compile(r"current display does not provide 120 Hz", re.IGNORECASE)
DEVICE_LOG_SOURCE = re.compile(r"\[[^\]]*:\d+ [^\]]*\] ")
DEVICE_LOG_SUBSYSTEM = re.compile(r"\[(?:float-controls|gpu|pipeline-cache|pipeline-journal|shader-disk-cache|time|write-watch)\] ")
VULKAN_LOADER_LOG = re.compile(r"\[Vulkan Loader\] (?:(?:DEBUG|INFO|WARNING) \| )?(?:LAYER|DRIVER):(?:\s|$)")
HDR_OUTPUT_STATUS = re.compile(
    r"\[videoout\] HDR output (?:available: peak [0-9]+\.[0-9] nits, full frame [0-9]+\.[0-9] nits, black [0-9]+\.[0-9]{4} nits"
    r"|unavailable \(display in HDR: (?:yes|no), surface offers HDR10: (?:yes|no)\))")
CTEST_SKIP_STATUS = re.compile(r"SKIP_RETURN_CODE=\d+|SKIP_REGULAR_EXPRESSION_MATCHED", re.IGNORECASE)

WINDOWS_PREFIX = "agc"

NO_RESERVED_CORE = "this host reserves no committer core"

ENVIRONMENT_LOADER_RETENTION = "environment binding: skipped unload checks because the platform loader retained libkernel; fallback detachment was not verified"


def device_limitations(output, patterns):
    reasons = [match.group(0) for line in output.splitlines() if (match := re.search(r"skipped,.*", line, re.IGNORECASE))]
    return bool(reasons) and all(any(pattern.search(reason) for pattern in patterns) for reason in reasons)


SYMLINK_PRIVILEGE_TESTS = tuple(f"guest_kernel_sys_paths_{case}{suffix}" for case in ("symlinkat", "readlink_limits", "readlinkat_limits") for suffix in ("", "_patched"))


def symlink_privilege_limitation(name, output):
    if name not in SYMLINK_PRIVILEGE_TESTS:
        return False
    reasons = [match.group(0).strip() for line in output.splitlines() if (match := re.search(r"(?:skipped,|SKIP:).*", line, re.IGNORECASE))]
    return bool(reasons) and all(reason == "SKIP: Windows symbolic-link privileges unavailable" for reason in reasons)


def emulator_log_line(line):
    if HDR_OUTPUT_STATUS.fullmatch(line):
        return True
    return bool(DEVICE_LOG_SOURCE.match(line) or DEVICE_LOG_SUBSYSTEM.match(line) or VULKAN_LOADER_LOG.match(line)) and not re.search(r"skipped,", line, re.IGNORECASE)


def high_refresh_limitation(output):
    reasons = []
    for line in output.splitlines():
        reason = line.strip()
        if not reason:
            continue
        if emulator_log_line(reason):
            continue
        if match := re.search(r"skipped,", reason, re.IGNORECASE):
            reason = reason[match.end():].strip()
        if reason:
            reasons.append(reason)
    return bool(reasons) and all(HIGH_REFRESH_LIMITATION.fullmatch(reason) for reason in reasons)


def environment_loader_retention(output):
    reasons = [line.strip() for line in output.splitlines() if re.search(r"\bskipp(?:ed|ing)\b", line, re.IGNORECASE)]
    return reasons == [ENVIRONMENT_LOADER_RETENTION]


def no_reserved_core(name, output):
    if not name.startswith("agc_worker_affinity"):
        return False
    lines = [line.strip() for line in output.splitlines() if line.strip() and not emulator_log_line(line.strip())]
    return lines == [NO_RESERVED_CORE]


def skip_summary(output):
    stated = next((line.strip() for line in output.splitlines() if re.search(r"skipped,|SKIP:", line, re.IGNORECASE)), None)
    if stated is not None:
        return stated
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    first = next((line for line in lines if not emulator_log_line(line)), lines[0] if lines else None)
    return f"the test stated no reason; its output starts with: {first}" if first else "the test stated no reason and wrote no output"


def skips(path, require_tests=False):
    root = ET.parse(path).getroot()
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError(f"{path}: expected a JUnit testsuite or testsuites root, got {root.tag!r}")
    found = False
    seen = set()
    for case in root.iter("testcase"):
        found = True
        name = case.get("name")
        if not name:
            raise ValueError(f"{path}: skipped testcase has no name")
        if name in seen:
            raise ValueError(f"{path}: testcase {name!r} is listed more than once")
        seen.add(name)
        status = case.get("status")
        if status is not None and status not in ("run", "fail", "notrun", "disabled", "None"):
            raise ValueError(f"{path}: invalid testcase status {status!r} for {name!r}")
        skipped = case.find("skipped")
        ctest_skip = skipped is not None and CTEST_SKIP_STATUS.fullmatch(skipped.get("message", "").strip())
        if status == "disabled" or (status == "notrun" and not ctest_skip):
            raise ValueError(f"{path}: testcase {name!r} is {status} and was not run")
        if skipped is None:
            continue
        output = ["skipped, " + reason.strip() for reason in (skipped.get("message", ""), skipped.text or "")
                  if reason.strip() and not CTEST_SKIP_STATUS.fullmatch(reason.strip())]
        output += [case.findtext(channel) or "" for channel in ("system-out", "system-err")]
        yield name, "\n".join(output)
    if require_tests and not found:
        raise ValueError(f"{path}: report contains no testcases")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="fail when a test is skipped that the job's device is not expected to skip")
    parser.add_argument("junit", type=Path, help="the file written by ctest --output-junit")
    parser.add_argument("--job", choices=("linux", "windows"), default="linux")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--json", action="store_true", help="emit skip reasons and policy decisions as JSON")
    output.add_argument("--list", action="store_true", help="print the skipped tests and why the file says they skipped")
    parser.add_argument("--require-tests", action="store_true", help="fail if the report contains no testcases")
    args = parser.parse_args(argv)
    try:
        found = sorted(skips(args.junit, require_tests=args.require_tests))
    except (OSError, ET.ParseError, ValueError) as error:
        parser.error(str(error))
    if args.list:
        for name, output in found:
            reason = next((match.group(0) for line in output.splitlines()
                           if (match := re.search(r"skipped,.*", line, re.IGNORECASE))), "")
            print(f"{name}: {reason}")
        return 0
    if args.job == "windows":
        unexpected = [name for name, output in found if not name.startswith(WINDOWS_PREFIX) and not symlink_privilege_limitation(name, output) and not device_limitations(output, (NO_DISPLAY, NO_VULKAN_DEVICE, NO_RAW_SOCKET)) and not high_refresh_limitation(output)]
        expected = f"any {WINDOWS_PREFIX}* test, which the runner has no Vulkan driver for, or a test that reports no display or Vulkan device, a display without a 120 Hz mode or no permission to create a raw socket, or the symlink creation and readlink limit tests reporting unavailable Windows symbolic-link privileges"
    else:
        unexpected = [name for name, output in found if not device_limitations(output, (DEVICE_LIMITATION,)) and not high_refresh_limitation(output)
                      and not (name == "guest_environment_binding_unload" and environment_loader_retention(output)) and not no_reserved_core(name, output)]
        expected = "any test that reports the device limitation it needs, such as narrow subgroups, a missing extension, no display for a Vulkan window, a display without a 120 Hz mode or no permission to create a raw socket, or guest_environment_binding_unload reporting loader retention"
    if args.json:
        grouped = {}
        for name, output in found:
            grouped.setdefault(name, []).append(output)
        print(json.dumps({"job": args.job, "skips": [
            {"name": name, "output": "\n".join(outputs), "occurrences": len(outputs), "expected": name not in unexpected}
            for name, outputs in sorted(grouped.items())], "unexpected": unexpected}, sort_keys=True))
        return int(bool(unexpected))
    reasons = {name: skip_summary(output) for name, output in found}
    for name in unexpected:
        print(f"error: {name} skipped: {reasons[name]}")
    if unexpected:
        print(f"{len(unexpected)} test(s) skipped for a reason that is not expected: the job lost that coverage and ctest still reports every test as passed")
        return 1
    print(f"{len(found)} test(s) skipped, all expected ({expected})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
