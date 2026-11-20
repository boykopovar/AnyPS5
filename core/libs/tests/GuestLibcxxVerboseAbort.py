import os
import signal
import subprocess
import sys


def disable_core():
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


for mode in ("plain", "buffered"):
    kwargs = {"preexec_fn": disable_core} if os.name == "posix" else {}
    result = subprocess.run([sys.argv[1], mode], capture_output=True, text=True, timeout=10, **kwargs)
    expected_exit = -signal.SIGABRT if os.name == "posix" else 3
    if result.returncode != expected_exit:
        raise AssertionError(f"{mode}: verbose abort did not terminate: {result.returncode}\n{result.stderr}")
    if "libc++ diagnostic: 37 sentinel\n" not in result.stderr:
        raise AssertionError(f"{mode}: formatted diagnostic missing\n{result.stderr}")

print("libc++ verbose abort formatted both diagnostics and terminated")
