import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "compile_slot.sh"
BASH = shutil.which("bash")


def run(path, slot_dir, command):
    env = {"PATH": path, "ANYPS5_COMPILE_SLOT_DIR": str(slot_dir), "ANYPS5_COMPILE_SLOTS": "1"}
    try:
        return subprocess.run([BASH, str(SCRIPT), *command], env=env, capture_output=True, text=True, timeout=10)
    except subprocess.TimeoutExpired:
        raise AssertionError("compile_slot.sh did not exit within 10 seconds")


@unittest.skipUnless(BASH and sys.platform.startswith("linux"), "needs bash on Linux")
class CompileSlotTests(unittest.TestCase):
    def test_exits_when_flock_is_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            (bin_dir / "sleep").symlink_to(shutil.which("sleep"))
            result = run(str(bin_dir), Path(tmp), ["true"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("flock is required", result.stderr)

    @unittest.skipUnless(shutil.which("flock"), "needs flock")
    def test_exits_when_the_slot_cannot_be_created(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = run(os.environ["PATH"], Path(tmp) / "missing", ["true"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("cannot create", result.stderr)

    @unittest.skipUnless(shutil.which("flock"), "needs flock")
    def test_runs_the_command_when_a_slot_is_free(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = run(os.environ["PATH"], Path(tmp), [sys.executable, "-c", "print('compiled')"])
        self.assertEqual((result.returncode, result.stdout.strip()), (0, "compiled"))


if __name__ == "__main__":
    unittest.main()
