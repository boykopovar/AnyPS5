import contextlib
import io
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import rebase_verify


class RebaseVerifyTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.git("init", "-q", "-b", "main")
        self.write("f.txt", "a\nb\nc\n")
        self.git("add", "f.txt")
        self.git("commit", "-q", "-m", "fix(tools): base")
        self.base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-b", "old")
        self.write("f.txt", "a\nNEW\nb\nc\n")
        self.git("commit", "-q", "-am", "feat(tools): add the new line")
        self.write("f.txt", "a\nNEW\nb\nc\ntail\n")
        self.git("commit", "-q", "-am", "feat(tools): add a tail line")
        self.old = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "main")
        self.write("g.txt", "main work\n")
        self.git("add", "g.txt")
        self.git("commit", "-q", "-m", "docs(tools): main work")
        self.git("checkout", "-q", "-b", "new", "main")
        self.git("cherry-pick", f"{self.base}..{self.old}")

    def git(self, *arguments):
        environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1"}
        return subprocess.run(["git", "-C", str(self.root), "-c", "user.name=Rebase Test", "-c", "user.email=test@example.com",
                               "-c", "commit.gpgsign=false", *arguments], capture_output=True, text=True, check=True,
                              env=environment).stdout.strip()

    def write(self, name, text):
        (self.root / name).write_text(text, encoding="utf-8")

    def verify(self):
        return rebase_verify.verify(self.root, "old", "new", "main")[3]

    def test_a_clean_rebase_keeps_every_line_author_and_commit(self):
        self.assertEqual(self.verify(), [])

    def test_a_line_the_branch_added_that_is_missing_is_reported(self):
        self.write("f.txt", "a\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "NEW" in problem for problem in problems), problems)

    def test_a_line_added_twice_is_reported(self):
        self.write("f.txt", "a\nNEW\nNEW\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("a line is added that the branch did not add" in problem for problem in problems), problems)

    def test_an_author_that_changed_is_reported(self):
        self.git("commit", "-q", "--amend", "--no-edit", "--author=Someone Else <other@example.com>", "-a")
        self.assertIn("authors or subjects differ between the old and the new commits", self.verify())

    def test_a_dropped_commit_is_reported(self):
        self.git("reset", "-q", "--hard", "main")
        self.git("cherry-pick", f"{self.old}~2..{self.old}~1")
        problems = self.verify()
        self.assertIn("the branch has 1 commits on main, and had 2 on its old base", problems)

    def test_main_exits_one_on_a_difference_and_zero_on_a_clean_rebase(self):
        arguments = ["old", "new", "--onto", "main", "--repo", str(self.root)]
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(rebase_verify.main(arguments), 0)
            self.write("f.txt", "a\nb\nc\ntail\n")
            self.git("commit", "-q", "-a", "--amend", "--no-edit")
            self.assertEqual(rebase_verify.main(arguments), 1)


if __name__ == "__main__":
    unittest.main()
