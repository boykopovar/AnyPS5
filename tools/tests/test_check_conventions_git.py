
import subprocess
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
CHECKER = REPO_ROOT / "tools" / "check_conventions.py"
INVALID_REVISION = "this-branch-does-not-exist"


class GitErrorHandlingTests(unittest.TestCase):
    def check_invalid_revision(self, option):
        valid = "--head" if option == "--base" else "--base"
        arguments = [valid, "HEAD", option, INVALID_REVISION]
        result = subprocess.run(
            [
                sys.executable,
                str(CHECKER),
                *arguments,
            ],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("Git command failed:", result.stderr)
        self.assertIn(INVALID_REVISION, result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_invalid_base_branch(self):
        self.check_invalid_revision("--base")

    def test_invalid_head_branch(self):
        self.check_invalid_revision("--head")


if __name__ == "__main__":
    unittest.main()
