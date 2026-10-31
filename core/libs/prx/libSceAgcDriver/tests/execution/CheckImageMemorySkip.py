import subprocess
import sys


result = subprocess.run(sys.argv[1:], capture_output=True, text=True)
sys.stdout.write(result.stdout)
sys.stderr.write(result.stderr)
if result.returncode != 77:
    sys.exit(f"image memory: expected exit 77 for unavailable coverage, got {result.returncode}")
if "image memory: sharing coverage unavailable after teardown" in result.stdout:
    sys.exit(0)
if "skipped, no usable Vulkan device:" in result.stdout:
    sys.exit(77)
sys.exit("image memory: coverage skip did not complete the teardown checks")
