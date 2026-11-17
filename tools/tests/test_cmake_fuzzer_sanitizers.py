import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def configure(build_dir, *options):
    command = ["cmake", "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
               "-DANYPS5_RELINKER_ONLY=ON", "-DANYPS5_BUILD_FUZZERS=ON", *options]
    return subprocess.run(command, capture_output=True, encoding="utf-8", errors="replace", timeout=300)


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "requires cmake and ninja")
class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def test_fuzzers_reject_tsan_before_compiler_checks(self):
        result = configure(self.root / "tsan", "-DANYPS5_ENABLE_TSAN=ON", "-DBUILD_TESTING=ON")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("do not combine it with ANYPS5_ENABLE_TSAN", result.stderr)


if __name__ == "__main__":
    unittest.main()
