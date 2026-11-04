
import os
import subprocess
import sys
import tempfile
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


class StaleOriginBaseTests(unittest.TestCase):
    def git(self, repo, *arguments):
        return subprocess.run(["git", "-C", str(repo), "-c", "user.name=Convention Test", "-c", "user.email=test@example.com",
                               "-c", "commit.gpgsign=false", *arguments], capture_output=True, text=True, check=True).stdout

    def check(self, repo, *arguments):
        environment = {key: value for key, value in os.environ.items() if key != "GITHUB_EVENT_PATH"}
        return subprocess.run([sys.executable, str(CHECKER), *arguments], cwd=repo, capture_output=True, text=True,
                              env=environment)

    def test_the_default_base_is_upstream_main_not_a_stale_origin_main(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            self.git(repo, "init", "-q")
            self.git(repo, "commit", "-q", "--allow-empty", "-m", "fix(tools): first")
            self.git(repo, "update-ref", "refs/remotes/origin/main", "HEAD")
            self.git(repo, "commit", "-q", "--allow-empty", "-m", "a subject the rules reject")
            self.git(repo, "update-ref", "refs/remotes/upstream/main", "HEAD")
            result = self.check(repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            stale = self.check(repo, "--base", "origin/main")
            self.assertEqual(stale.returncode, 1)
            self.assertIn("Commit subject is not Conventional Commits", stale.stdout + stale.stderr)


if __name__ == "__main__":
    unittest.main()
