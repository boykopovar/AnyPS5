import os
import signal
import subprocess
import sys


def disable_core():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


cases = [
    ("runtime.cpp", 37, "ReadEntry", "index < count", False),
    ("%s-component.cpp", 918, "Lookup<int>", "cursor < end && size % 4 == 0", False),
    ("buffered.cpp", 64, "ReadBuffered", "entry != nullptr", True),
]

for file, line, function, condition, buffered in cases:
    kwargs = {"preexec_fn": disable_core} if os.name == "posix" else {}
    result = subprocess.run(
        [sys.argv[1], file, str(line), function, condition] + (["buffered"] if buffered else []),
        capture_output=True, text=True, timeout=10, **kwargs,
    )
    expected_exit = -signal.SIGABRT if os.name == "posix" else 3
    if result.returncode != expected_exit:
        raise AssertionError(f"assertion did not abort: {result.returncode}\n{result.stderr}")
    expected = f"{file}:{line}: {function}: Assertion '{condition}' failed.\n"
    if expected not in result.stderr:
        raise AssertionError(f"missing assertion diagnostic: {expected!r}\n{result.stderr}")

print(f"{len(cases)} assertion diagnostics preserved; all children aborted")
