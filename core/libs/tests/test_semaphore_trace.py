import os
import re
import subprocess
import sys


def run(executable, enabled):
    environment = os.environ.copy()
    environment.pop("APS5_TRACE_SYNC", None)
    if enabled:
        environment["APS5_TRACE_SYNC"] = "1"
    return subprocess.run([executable], capture_output=True, check=False, env=environment, text=True, timeout=15)


def main():
    executable = sys.argv[1]
    quiet = run(executable, False)
    if quiet.returncode != 0 or "[sema]" in quiet.stderr:
        raise SystemExit("semaphore test failed or traced without APS5_TRACE_SYNC")

    traced = run(executable, True)
    if traced.returncode != 0:
        raise SystemExit(traced.stderr or f"semaphore test exited {traced.returncode}")

    lines = traced.stderr.splitlines()
    create = next((line for line in lines if "[sema] create " in line and "'trace'" in line), None)
    if create is None:
        raise SystemExit("semaphore creation trace is missing")
    handle_match = re.search(r"\[sema\] create (\S+)", create)
    if handle_match is None:
        raise SystemExit("semaphore creation trace has no handle")
    handle = handle_match.group(1)
    expected = {
        "signal": f"[sema] signal {handle} 'trace'",
        "wait begin": f"[sema] wait begin {handle} 'trace'",
        "wait end": f"[sema] wait end {handle} 'trace'",
    }
    for event, prefix in expected.items():
        if not any(line.startswith(prefix) for line in lines):
            raise SystemExit(f"semaphore {event} trace for the created handle is missing")
    if not any(line.startswith(expected["wait begin"]) and "timeout=-1" in line for line in lines):
        raise SystemExit("indefinite semaphore wait is not identified in the trace")


if __name__ == "__main__":
    main()
