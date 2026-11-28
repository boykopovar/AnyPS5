import re
import subprocess
import sys


def run(executable, mode, status, patterns, reports):
    result = subprocess.run([executable, mode], capture_output=True, text=True, timeout=10)
    output = result.stdout + result.stderr
    if result.returncode != status:
        raise AssertionError(f"{mode}: exit {result.returncode}, expected {status}\n{output}")
    for pattern in patterns:
        if re.search(pattern, output) is None:
            raise AssertionError(f"{mode}: missing {pattern!r}\n{output}")
    if output.count("runtime error:") != reports:
        raise AssertionError(f"{mode}: expected {reports} diagnostics\n{output}")
    if output.count("death callback") != int(status == 1):
        raise AssertionError(f"{mode}: unexpected death callback count\n{output}")


def main():
    executable = sys.argv[1]
    run(executable, "ubsan_legacy_type", 0, [
        r"legacy.cpp:101:3: runtime error: load of null pointer of type 'int'",
        r"legacy.cpp:102:7: runtime error: store to misaligned address 0x[0-9a-f]+ for type 'int', which requires 256 byte alignment",
        r"legacy.cpp:103:9: runtime error: reference binding to address 0x[0-9a-f]+ with insufficient space for an object of type 'int'",
        r"legacy.cpp:104:11: runtime error: member access within address 0x[0-9a-f]+ with insufficient space for an object of type 'int'",
        r"legacy type returned",
    ], 4)
    run(executable, "ubsan_legacy_return", 0, [
        r"legacy.cpp:201:5: runtime error: null pointer returned from function declared to never return null",
        r"legacy.h:8:2: note: returns_nonnull attribute specified here",
        r"legacy.cpp:202:6: runtime error: null pointer returned from function declared to never return null",
        r"legacy return returned",
    ], 2)
    for kind, diagnostic in [("type", "load of null pointer of type 'int'"), ("return", "null pointer returned from function declared to never return null")]:
        run(executable, f"ubsan_legacy_{kind}_abort", 1, [diagnostic, "SUMMARY: UndefinedBehaviorSanitizer:"], 1)
        run(executable, f"ubsan_legacy_{kind}_repeat_abort", 1, [diagnostic, "SUMMARY: UndefinedBehaviorSanitizer:"], 2)
    print("legacy UBSan diagnostics, deduplication and fatal exits passed")


if __name__ == "__main__":
    main()
