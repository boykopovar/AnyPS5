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

    def test_a_missing_line_is_reported_when_diff_noprefix_is_set(self):
        self.git("config", "diff.noprefix", "true")
        self.write("f.txt", "a\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "NEW" in problem for problem in problems), problems)

    def test_a_line_added_twice_is_reported(self):
        self.write("f.txt", "a\nNEW\nNEW\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("a line is added that the branch did not add" in problem for problem in problems), problems)

    def test_a_rebase_that_substitutes_a_different_deleted_line_is_reported(self):
        self.git("checkout", "-q", "old")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", "a\nc\n")
        self.git("commit", "-q", "-am", "fix(tools): remove a line")
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.write("f.txt", "a\nb\n")
        self.git("commit", "-q", "-am", "fix(tools): remove a line")
        problems = self.verify()
        self.assertTrue(any("a deleted line is missing" in problem and "b" in problem for problem in problems), problems)
        self.assertTrue(any("a line is deleted that the branch did not delete" in problem and "c" in problem for problem in problems), problems)

    def test_a_line_already_deleted_upstream_needs_no_second_deletion(self):
        self.git("checkout", "-q", "old")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", "a\nc\n")
        self.git("commit", "-q", "-am", "fix(tools): remove a line")
        self.git("checkout", "-q", "main")
        self.write("f.txt", "a\nc\n")
        self.git("commit", "-q", "-am", "docs(tools): remove the same line upstream")
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.git("commit", "-q", "--allow-empty", "-m", "fix(tools): remove a line")
        self.assertEqual(self.verify(), [])

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

    def test_a_non_ascii_line_is_reported_as_utf8(self):
        self.write("f.txt", "a\nNEW\nb\nc\ntail\n→ 界\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        done = subprocess.run([sys.executable, str(Path(rebase_verify.__file__)), "old", "new", "--onto", "main", "--repo", str(self.root)],
                              capture_output=True, env={**os.environ, "PYTHONIOENCODING": "cp1252"})
        self.assertEqual(done.returncode, 1, done.stderr.decode("utf-8", "replace"))
        self.assertIn("error: f.txt: a line is added that the branch did not add (1x): → 界", done.stdout.decode("utf-8"))

    def test_a_clean_rebase_stays_clean_after_main_moves_on(self):
        self.git("checkout", "-q", "main")
        self.write("g.txt", "later main work\n")
        self.git("commit", "-q", "-am", "docs(tools): later main work")
        self.assertEqual(self.verify(), [])

    def test_a_missing_line_is_reported_after_main_moves_on(self):
        self.write("f.txt", "a\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        self.git("checkout", "-q", "main")
        self.write("g.txt", "later main work\n")
        self.git("commit", "-q", "-am", "docs(tools): later main work")
        problems = self.verify()
        self.assertEqual(len(problems), 1, problems)
        self.assertIn("an added line is missing", problems[0])

    def delete_after_rebase(self, new_text):
        self.git("checkout", "-q", "old")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", "a\nc\n")
        self.git("commit", "-q", "-am", "fix(tools): remove a line")
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.write("f.txt", new_text)
        self.git("commit", "-q", "-am", "fix(tools): remove a line")
        self.git("checkout", "-q", "main")
        self.write("f.txt", "a\nc\n")
        self.git("commit", "-q", "-am", "docs(tools): remove the same line later")
        return self.verify()

    def test_a_deletion_main_repeats_after_the_rebase_stays_clean(self):
        self.assertEqual(self.delete_after_rebase("a\nc\n"), [])

    def test_a_different_deleted_line_is_reported_when_main_deletes_the_line_after_the_rebase(self):
        problems = self.delete_after_rebase("a\nb\n")
        self.assertTrue(any("a deleted line is missing" in problem and problem.endswith(": b") for problem in problems), problems)
        self.assertTrue(any("a line is deleted that the branch did not delete" in problem and problem.endswith(": c") for problem in problems), problems)

    def test_a_merge_of_main_into_the_old_branch_is_not_a_commit_to_keep(self):
        self.git("checkout", "-q", "old")
        self.git("merge", "-q", "--no-ff", "--no-edit", "main")
        self.assertEqual(self.verify(), [])


if __name__ == "__main__":
    unittest.main()
