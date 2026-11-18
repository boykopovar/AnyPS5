import collections
import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import rebase_verify


def run_git(root, *arguments):
    environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1"}
    return subprocess.run(["git", "-C", str(root), "-c", "user.name=Rebase Test", "-c", "user.email=test@example.com",
                           "-c", "commit.gpgsign=false", *arguments], capture_output=True, text=True, check=True,
                          env=environment).stdout.strip()


class RebaseVerifyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        cls.template = Path(directory.name)
        git = lambda *arguments: run_git(cls.template, *arguments)
        write = lambda name, text: (cls.template / name).write_text(text, encoding="utf-8")
        git("init", "-q", "-b", "main")
        git("config", "core.filemode", "false")
        write("f.txt", "a\nb\nc\n")
        git("add", "f.txt")
        git("commit", "-q", "-m", "fix(tools): base")
        cls.base = git("rev-parse", "HEAD")
        git("checkout", "-q", "-b", "old")
        write("f.txt", "a\nNEW\nb\nc\n")
        git("commit", "-q", "-am", "feat(tools): add the new line")
        write("f.txt", "a\nNEW\nb\nc\ntail\n")
        git("commit", "-q", "-am", "feat(tools): add a tail line")
        cls.old = git("rev-parse", "HEAD")
        git("checkout", "-q", "main")
        write("g.txt", "main work\n")
        git("add", "g.txt")
        git("commit", "-q", "-m", "docs(tools): main work")
        git("checkout", "-q", "-b", "new", "main")
        git("cherry-pick", f"{cls.base}..{cls.old}")

    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name) / "repository"
        shutil.copytree(self.template, self.root, symlinks=True)

    def git(self, *arguments):
        return run_git(self.root, *arguments)

    def write(self, name, text):
        (self.root / name).write_text(text, encoding="utf-8")

    def verify(self):
        return rebase_verify.verify(self.root, "old", "new", "main")[3]

    def amend_mode(self, path, executable):
        self.git("update-index", "--chmod=+x" if executable else "--chmod=-x", "--", path)
        self.git("commit", "-q", "--amend", "--no-edit")

    def test_a_clean_rebase_keeps_every_line_author_and_commit(self):
        self.assertEqual(self.verify(), [])

    def test_relative_diff_cannot_hide_lost_lines_from_a_subdirectory(self):
        nested = self.root / "nested"
        nested.mkdir()
        self.git("config", "diff.relative", "true")
        for text, message in (("a\nb\nc\ntail\n", "an added line is missing (1x): NEW"),
                              ("a\nNEW\nc\ntail\n", "a line is deleted that the branch did not delete (1x): b")):
            with self.subTest(text=text):
                self.write("f.txt", text)
                self.git("commit", "-q", "-a", "--amend", "--no-edit")
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    status = rebase_verify.main(["old", "new", "--onto", "main", "--repo", str(nested)])
                self.assertEqual(status, 1)
                self.assertIn("error: f.txt: " + message, output.getvalue())

    def test_changed_lines_keep_repository_paths_from_a_subdirectory(self):
        nested = self.root / "nested"
        nested.mkdir()
        self.write("nested/inside.txt", "inside\n")
        self.git("add", "nested/inside.txt")
        self.git("commit", "-q", "--amend", "--no-edit")
        expected = rebase_verify.changed_lines(self.root, "main", "new")
        self.assertEqual(set(expected[0]), {"f.txt", "nested/inside.txt"})
        for relative in ("false", "true"):
            with self.subTest(relative=relative):
                self.git("config", "diff.relative", relative)
                self.assertEqual(rebase_verify.changed_lines(nested, "main", "new"), expected)

    def test_relative_diff_keeps_a_clean_rebase_clean_from_a_subdirectory(self):
        nested = self.root / "nested"
        nested.mkdir()
        self.git("config", "diff.relative", "true")
        self.assertEqual(rebase_verify.verify(nested, "old", "new", "main")[3], [])

    def test_an_executable_mode_change_must_survive_rebase(self):
        self.git("checkout", "-q", "old")
        self.amend_mode("f.txt", True)
        self.git("checkout", "-q", "new")
        self.assertEqual(self.verify(), ["f.txt: file mode 100644, expected 100755"])
        self.amend_mode("f.txt", True)
        self.assertEqual(self.verify(), [])

    def test_an_unexpected_executable_mode_change_is_reported(self):
        self.amend_mode("f.txt", True)
        self.assertEqual(self.verify(), ["f.txt: file mode 100755, expected 100644"])

    def test_an_executable_mode_already_applied_upstream_is_kept(self):
        self.git("checkout", "-q", "old")
        self.amend_mode("f.txt", True)
        self.git("checkout", "-q", "main")
        self.amend_mode("f.txt", True)
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.git("cherry-pick", f"{self.base}..old")
        self.assertEqual(self.verify(), [])

    def test_an_upstream_executable_mode_must_not_be_reverted(self):
        self.git("checkout", "-q", "main")
        self.amend_mode("f.txt", True)
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.git("cherry-pick", f"{self.base}..old")
        self.assertEqual(self.verify(), [])
        self.amend_mode("f.txt", False)
        self.assertEqual(self.verify(), ["f.txt: file mode 100644, expected 100755"])

    def test_a_new_executable_file_keeps_its_mode(self):
        path = "new script.sh"
        for branch, executable in (("old", True), ("new", False)):
            self.git("checkout", "-q", branch)
            self.write(path, "exit 0\n")
            self.git("add", "--", path)
            self.amend_mode(path, executable)
        self.assertEqual(self.verify(), ["new script.sh: file mode 100644, expected 100755"])

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


class RebaseDiffTextTests(unittest.TestCase):
    def changes(self, text, sign="+"):
        from unittest.mock import patch
        diff = "diff --git a/f.cpp b/f.cpp\n--- a/f.cpp\n+++ b/f.cpp\n@@ -1 +1 @@\n" + sign + text + "\n"
        with patch.object(rebase_verify, "git", return_value=diff):
            return rebase_verify.changed_lines(Path("."), "base", "head")

    def test_unicode_separators_cannot_hide_changed_added_text(self):
        for separator in ("\u0085", "\u2028", "\u2029"):
            with self.subTest(separator=repr(separator)):
                before = 'const char* s = "prefix' + separator + 'before";'
                after = 'const char* s = "prefix' + separator + 'after";'
                old, _ = self.changes(before)
                new, _ = self.changes(after)
                self.assertEqual(len(rebase_verify.differences(old, new)), 2)
                self.assertEqual(old["f.cpp"], {before: 1})

    def test_unicode_separators_cannot_hide_changed_deleted_text(self):
        for separator in ("\u0085", "\u2028", "\u2029"):
            with self.subTest(separator=repr(separator)):
                before, after = "prefix" + separator + "before", "prefix" + separator + "after"
                _, old = self.changes(before, "-")
                _, new = self.changes(after, "-")
                self.assertEqual(len(rebase_verify.differences(old, new, deleted=True)), 2)
                self.assertEqual(old["f.cpp"], {before: 1})

    def test_unchanged_unicode_and_ordinary_line_controls(self):
        for text in ("ordinary", "prefix\u2028tail"):
            old, _ = self.changes(text)
            new, _ = self.changes(text)
            self.assertEqual(rebase_verify.differences(old, new), [])
        old, _ = self.changes("before")
        new, _ = self.changes("after")
        self.assertEqual(len(rebase_verify.differences(old, new)), 2)


class CountHelperTests(unittest.TestCase):
    def test_default_onto_returns_an_explicit_name_without_asking_git(self):
        self.assertEqual(rebase_verify.default_onto(Path("."), "origin/main"), "origin/main")

    def test_commits_and_people_count_the_commits_between_two_refs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_git(root, "init", "-q")
            run_git(root, "-c", "user.name=Base", "-c", "user.email=base@example.invalid",
                    "commit", "-q", "--allow-empty", "-m", "base")
            base = run_git(root, "rev-parse", "HEAD").strip()
            for name, email in (("Alice", "alice@example.invalid"), ("Bob", "bob@example.invalid"),
                                ("Alice", "alice@example.invalid")):
                run_git(root, "-c", f"user.name={name}", "-c", f"user.email={email}",
                        "commit", "-q", "--allow-empty", "-m", f"change by {name}")
            tip = run_git(root, "rev-parse", "HEAD").strip()
            self.assertEqual(rebase_verify.commits(root, base, tip), 3)
            self.assertEqual(rebase_verify.people(root, base, tip), collections.Counter({
                "Alice <alice@example.invalid>\x1fchange by Alice": 2,
                "Bob <bob@example.invalid>\x1fchange by Bob": 1}))
class ChangedModeTests(unittest.TestCase):
    def commit(self, root, message):
        run_git(root, "-c", "user.name=t", "-c", "user.email=t@example.invalid", "commit", "-q", "-m", message)
        return run_git(root, "rev-parse", "HEAD")

    def test_changed_modes_reports_a_file_whose_executable_bit_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_git(root, "init", "-q")
            (root / "f.txt").write_text("x\n", encoding="utf-8")
            run_git(root, "add", "f.txt")
            base = self.commit(root, "base")
            run_git(root, "update-index", "--chmod=+x", "--", "f.txt")
            tip = self.commit(root, "make executable")
            self.assertEqual(rebase_verify.changed_modes(root, base, tip), {b"f.txt": ("100644", "100755")})

    def test_changed_modes_is_empty_for_a_content_only_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_git(root, "init", "-q")
            (root / "f.txt").write_text("x\n", encoding="utf-8")
            run_git(root, "add", "f.txt")
            base = self.commit(root, "base")
            (root / "f.txt").write_text("y\n", encoding="utf-8")
            run_git(root, "add", "f.txt")
            tip = self.commit(root, "edit")
            self.assertEqual(rebase_verify.changed_modes(root, base, tip), {})


if __name__ == "__main__":
    unittest.main()
