import os
import signal
import subprocess
import sys


def disable_core():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


cases = [
    ("readable", "[libc] guest assertion failed: count > 0 (Source.cpp:12)\n"),
    ("unmapped", "[libc] guest assertion failed: ? (?)\n"),
    ("null", "[libc] guest assertion failed: ? (?)\n"),
]

for mode, expected in cases:
    kwargs = {"preexec_fn": disable_core} if os.name == "posix" else {}
    result = subprocess.run([sys.argv[1], mode], capture_output=True, text=True, timeout=10, **kwargs)
    expected_exit = -signal.SIGABRT if os.name == "posix" else 3
    if result.returncode != expected_exit:
        raise AssertionError(f"{mode}: assertion did not abort: {result.returncode}\n{result.stderr}")
    if expected not in result.stderr:
        raise AssertionError(f"{mode}: missing assertion diagnostic: {expected!r}\n{result.stderr}")

print(f"{len(cases)} guest assertion messages printed; all children aborted")
