import os
import subprocess
import sys


def run(executable, mode_value, arguments=()):
    environment = os.environ.copy()
    environment.pop("APS5_TRACE_SYNC", None)
    if mode_value is not None:
        environment["APS5_TRACE_SYNC"] = mode_value
    result = subprocess.run([executable, *arguments], capture_output=True, check=False, env=environment, text=True, timeout=15)
    return result


def dump_lines(output):
    return [line for line in output.splitlines() if line.startswith("[sema]") and " seq=" in line]


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    executable = sys.argv[1]

    quiet = run(executable, None)
    require(quiet.returncode == 0, f"direct run exited {quiet.returncode} without tracing")
    require("[sema]" not in quiet.stderr, "ring dump emitted without APS5_TRACE_SYNC")

    legacy = run(executable, "1")
    require(legacy.returncode == 0, f"direct run exited {legacy.returncode} in legacy mode")
    require("sync ring dump" not in legacy.stderr, "legacy mode produced a ring dump")

    traced = run(executable, "ring")
    require(traced.returncode == 0, f"direct run exited {traced.returncode} in ring mode")
    lines = dump_lines(traced.stderr)
    ops = []
    for line in lines:
        for operation in ("create", "signal", "wait begin", "wait end", "poll", "cancel", "delete"):
            if line.startswith(f"[sema] {operation} "):
                ops.append(operation)
                break
    expected = ["create", "wait begin", "wait end", "signal", "poll", "signal", "cancel", "delete"]
    require(ops == expected, f"ring dump operations {ops} differ from {expected}")
    create = next(line for line in lines if line.startswith("[sema] create "))
    require("'ring-trace'" in create and "init=1" in create and "max=2" in create, f"create record malformed: {create}")
    first_signal = next(line for line in lines if line.startswith("[sema] signal "))
    require("count=1" in first_signal and "have=0->1" in first_signal, f"signal record malformed: {first_signal}")
    for line in lines:
        require(" tid=" in line and " t=" in line and " seq=" in line, f"record missing metadata: {line}")
    live = [line for line in traced.stderr.splitlines() if "[sema]" in line and " seq=" not in line and "sync ring dump" not in line]
    require(live == [], f"ring mode wrote live trace lines: {live[:2]}")
    header = next((line for line in traced.stderr.splitlines() if "sync ring dump" in line), "")
    require("8 records" in header, f"dump header malformed: {header!r}")
    sequences = [int(line.rsplit(" seq=", 1)[1]) for line in lines]
    require(sequences == sorted(sequences), "dump is not in sequence order")

    filtered = run(executable, "ring:ring-trace")
    require(filtered.returncode == 0, f"direct run exited {filtered.returncode} in filtered mode")
    filtered_ops = []
    for line in dump_lines(filtered.stderr):
        for operation in ("create", "signal", "wait begin", "wait end", "poll", "cancel", "delete"):
            if line.startswith(f"[sema] {operation} "):
                filtered_ops.append(operation)
                break
    require(filtered_ops == ["wait end", "signal", "signal"], f"filtered dump operations {filtered_ops} differ from the signal/wait-end selection")

    other = run(executable, "ring:other")
    require(other.returncode == 0, f"direct run exited {other.returncode} in filtered mode")
    require(dump_lines(other.stderr) == [], "filtered ring recorded a semaphore of another name")

    crashed = run(executable, "ring", ("crash",))
    require(crashed.returncode != 0, "crash run unexpectedly exited 0")
    require("sync ring dump" in crashed.stderr, "crash path did not write the ring dump")
    require(len(dump_lines(crashed.stderr)) == 8, "crash path dump is incomplete")


if __name__ == "__main__":
    main()
