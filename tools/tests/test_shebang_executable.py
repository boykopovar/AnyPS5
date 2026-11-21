import subprocess
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]


class ShebangExecutableTests(unittest.TestCase):
    def test_tracked_tools_with_shebang_are_executable(self):
        listing = subprocess.run(
            ["git", "ls-files", "-s", "--", "tools/*.py"],
            cwd=REPO_ROOT, capture_output=True, text=True, check=True).stdout
        not_executable = []
        for line in listing.splitlines():
            mode, _, _, path = line.split(maxsplit=3)
            if (REPO_ROOT / path).read_bytes()[:2] == b"#!" and mode != "100755":
                not_executable.append(f"{path} ({mode})")
        self.assertEqual(not_executable, [])


if __name__ == "__main__":
    unittest.main()
