import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / "compile_slot.sh"
BASH = shutil.which("bash")


def run(path, slot_dir, command, slots="1", timeout=10):
    env = {"PATH": path, "ANYPS5_COMPILE_SLOT_DIR": str(slot_dir), "ANYPS5_COMPILE_SLOTS": slots}
    try:
        return subprocess.run([BASH, str(SCRIPT), *command], env=env, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise AssertionError(f"compile_slot.sh did not exit within {timeout} seconds")


@unittest.skipUnless(BASH and sys.platform.startswith("linux"), "needs bash on Linux")
class CompileSlotTests(unittest.TestCase):
    def test_invalid_slot_counts_fail_without_waiting_for_a_lock(self):
        with tempfile.TemporaryDirectory() as tmp:
            for count in ("0", "-1", "garbage", "18446744073709551616"):
                with self.subTest(count=count):
                    result = run(os.environ["PATH"], Path(tmp), ["true"], slots=count, timeout=1)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertIn("positive decimal integer", result.stderr)
            self.assertEqual(list(Path(tmp).iterdir()), [])

    def test_missing_compiler_command_fails_before_opening_a_slot(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = run(os.environ["PATH"], Path(tmp), [], timeout=1)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn("compiler command is required", result.stderr)
            self.assertEqual(list(Path(tmp).iterdir()), [])

    def test_exits_when_flock_is_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            bin_dir = Path(tmp) / "bin"
            bin_dir.mkdir()
            (bin_dir / "sleep").symlink_to(shutil.which("sleep"))
            result = run(str(bin_dir), Path(tmp), ["true"])
        self.assertEqual(result.returncode, 2)
        self.assertIn("flock is required", result.stderr)

    def test_lock_error_stops_the_compiler_wrapper(self):
        with tempfile.TemporaryDirectory() as tmp:
            flock = Path(tmp) / "flock"
            flock.write_text("#!/bin/sh\nexit 71\n")
            flock.chmod(0o755)
            result = run(os.pathsep.join((tmp, os.environ["PATH"])), Path(tmp), ["true"])
        self.assertEqual(result.returncode, 71)
        self.assertIn("flock failed", result.stderr)

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
