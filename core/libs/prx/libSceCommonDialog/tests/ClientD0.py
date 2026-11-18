import signal
import subprocess
import sys


result = subprocess.run([sys.argv[1], "--client-d0"], capture_output=True, timeout=10)
if result.returncode == 0:
    raise AssertionError("Client D0 returned")
if sys.platform != "win32" and result.returncode != -signal.SIGILL:
    raise AssertionError(f"Client D0 exited with {result.returncode}, expected SIGILL")
