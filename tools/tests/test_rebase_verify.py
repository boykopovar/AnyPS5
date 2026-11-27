import collections
import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import rebase_verify

REBASE_VERIFY_TEST_SHARDS = 3


def iter_test_cases(suite):
    for test in suite:
        if isinstance(test, unittest.TestSuite):
            yield from iter_test_cases(test)
        else:
            yield test


def discovered_test_cases():
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    return sorted(iter_test_cases(suite), key=lambda test: test.id())


def rebase_verify_test_shard(shard, shard_count=REBASE_VERIFY_TEST_SHARDS):
    if shard_count <= 0 or shard < 0 or shard >= shard_count:
        raise ValueError("shard index must be within the shard count")
    tests = discovered_test_cases()
    return unittest.TestSuite(test for index, test in enumerate(tests) if index % shard_count == shard)


class TestShardTests(unittest.TestCase):
    def test_shards_discover_each_case_once(self):
        discovered = [test.id() for test in discovered_test_cases()]
        sharded = [test.id() for shard in range(REBASE_VERIFY_TEST_SHARDS)
                   for test in iter_test_cases(rebase_verify_test_shard(shard))]
        self.assertEqual(sorted(sharded), sorted(discovered))
        self.assertEqual(len(sharded), len(set(sharded)))
        self.assertTrue(all(len(list(iter_test_cases(rebase_verify_test_shard(shard)))) > 0
                            for shard in range(REBASE_VERIFY_TEST_SHARDS)))


def run_git(root, *arguments, input_bytes=None, raw=False):
    environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1"}
    output = subprocess.run(["git", "-C", str(root), "-c", "user.name=Rebase Test", "-c", "user.email=test@example.com",
                             "-c", "commit.gpgsign=false", *arguments], input=input_bytes, capture_output=True,
                            text=input_bytes is None and not raw, check=True, env=environment).stdout
    if raw:
        return output
    return (output if input_bytes is None else output.decode("ascii")).strip()


class RebaseVerifyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        directory = tempfile.TemporaryDirectory()
        cls.addClassCleanup(directory.cleanup)
        cls.template = Path(directory.name)
        git = lambda *arguments: run_git(cls.template, *arguments)
        write = lambda name, text: (cls.template / name).write_bytes(text.encode("utf-8"))
        git("init", "-q", "-b", "main")
        git("config", "core.autocrlf", "false")
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

    def git(self, *arguments, input_bytes=None, raw=False):
        return run_git(self.root, *arguments, input_bytes=input_bytes, raw=raw)

    def write(self, name, text):
        (self.root / name).write_bytes(text.encode("utf-8"))

    def commit_tree_file(self, parent, name, content, message):
        name = os.fsencode(name)
        blob = self.git("hash-object", "-w", "--stdin", input_bytes=content).encode("ascii")
        entries = self.git("ls-tree", "-z", parent, raw=True).split(b"\0")
        retained = [entry for entry in entries if entry and entry.split(b"\t", 1)[1] != name]
        retained.append(b"100644 blob " + blob + b"\t" + name)
        tree = self.git("mktree", "-z", input_bytes=b"\0".join(retained) + b"\0")
        return self.git("commit-tree", tree, "-p", parent, "-m", message)

    def verify(self):
        return rebase_verify.verify(self.root, "old", "new", "main")[3]

    def amend_mode(self, path, executable):
        self.git("update-index", "--chmod=+x" if executable else "--chmod=-x", "--", path)
        self.git("commit", "-q", "--amend", "--no-edit")

    def test_a_clean_rebase_keeps_every_line_author_and_commit(self):
        self.assertEqual(self.verify(), [])

    def test_revision_range_like_filename_does_not_break_validation(self):
        (self.root / f"{self.base}..old").touch()
        (self.root / f"{self.base}..{self.old}").touch()
        self.assertEqual(self.verify(), [])

    def test_non_utf8_text_edits_are_distinguished(self):
        self.git("checkout", "-q", "-B", "old", self.base)
        self.commit_bytes(b"changed: \xfe\n", "fix(tools): record identifier")
        self.git("checkout", "-q", "-B", "new", "main")
        self.commit_bytes(b"changed: \xff\n", "fix(tools): record identifier")
        self.assertEqual(len(self.verify()), 2)

    def test_non_utf8_paths_remain_distinct(self):
        for branch, ref, suffix in (("old", self.base, b"\xfe"), ("new", "main", b"\xff")):
            commit = self.commit_tree_file(ref, b"file " + suffix + b".txt", b"same content\n",
                                           "fix(tools): add a file")
            self.git("update-ref", f"refs/heads/{branch}", commit)
        self.assertEqual(len(self.verify()), 4)

    def test_rebase_checks_added_and_deleted_lines_at_quoted_paths(self):
        path = 'quoted-"-\\-\t-é.txt'
        name = path.encode("utf-8")
        main = self.commit_tree_file("main", name, b"old line\n", "feat(tools): add a quoted path")
        old = self.commit_tree_file(main, name, b"new line\n", "fix(tools): change content at a quoted path")
        self.git("update-ref", "refs/heads/main", main)
        self.git("update-ref", "refs/heads/old", old)
        self.git("update-ref", "refs/heads/new", old)
        self.assertEqual(self.verify(), [])

        new = self.commit_tree_file(main, name, b"old line\ndifferent line\n",
                                    "fix(tools): change content at a quoted path")
        self.git("update-ref", "refs/heads/new", new)
        self.assertEqual(self.verify(), [
            f"{path}: an added line is missing (1x): new line",
            f"{path}: a line is added that the branch did not add (1x): different line",
            f"{path}: a deleted line is missing (1x): old line",
        ])

    def test_rebase_reports_a_crlf_line_changed_to_lf(self):
        path = "windows.txt"
        self.git("checkout", "-q", "old")
        self.write(path, "first\r\nsecond\r\n")
        self.git("add", "--", path)
        self.git("commit", "-q", "-m", "feat(tools): add a Windows fixture")
        self.git("checkout", "-q", "new")
        self.git("cherry-pick", "old")
        self.assertEqual(self.verify(), [])

        self.write(path, "first\r\nsecond\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = rebase_verify.main(["old", "new", "--onto", "main", "--repo", str(self.root)])
        self.assertEqual(status, 1, output.getvalue())
        self.assertIn(f"error: {path}: an added line is missing (1x): second\\r\n", output.getvalue())
        self.assertIn(f"error: {path}: a line is added that the branch did not add (1x): second\n", output.getvalue())

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

    def test_rebase_rejects_moving_an_added_line_with_the_same_base_file(self):
        self.assertEqual(self.verify(), [])
        self.write("f.txt", "a\nb\nNEW\nc\ntail\n")
        self.git("commit", "-qa", "--amend", "--no-edit")
        self.assertEqual(self.verify(), [
            "f.txt: rebased content differs although the base content is unchanged",
        ])

    def test_rebase_rejects_reordered_lines_in_a_new_file(self):
        path = "new file.txt"
        self.git("checkout", "-q", "old")
        self.write(path, "first\nsecond\n")
        self.git("add", "--", path)
        self.git("commit", "-qm", "feat(tools): add an ordered file")
        self.git("checkout", "-q", "new")
        self.git("cherry-pick", "old")
        self.assertEqual(self.verify(), [])

        self.write(path, "second\nfirst\n")
        self.git("commit", "-qa", "--amend", "--no-edit")
        self.assertEqual(self.verify(), [
            f"{path}: rebased content differs although the base content is unchanged",
        ])

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

    def test_paths_with_spaces_have_no_trailing_tab(self):
        path = "spaced file.txt"
        self.git("checkout", "-q", "old")
        self.write(path, "old line\n")
        self.git("add", "--", path)
        self.git("commit", "-q", "--amend", "--no-edit")
        self.git("checkout", "-q", "new")
        self.write(path, "other line\n")
        self.git("add", "--", path)
        self.git("commit", "-q", "--amend", "--no-edit")
        added, _ = rebase_verify.changed_lines(self.root, self.base, "old")
        self.assertIn(path, added)
        self.assertNotIn(path + "\t", added)
        _, deleted = rebase_verify.changed_lines(self.root, "old", self.base)
        self.assertIn(path, deleted)
        self.assertNotIn(path + "\t", deleted)
        self.assertEqual(self.verify(), [
            f"{path}: an added line is missing (1x): old line",
            f"{path}: a line is added that the branch did not add (1x): other line",
        ])

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

    def test_submodule_revision_is_checked_regardless_of_diff_config(self):
        path = "dependencies/example"
        expected = self.base
        unexpected = self.git("rev-parse", "main")
        for branch in ("old", "new"):
            self.git("checkout", "-q", branch)
            self.git("update-index", "--add", "--cacheinfo", f"160000,{expected},{path}")
            self.git("commit", "-q", "--amend", "--no-edit")
        self.assertEqual(self.verify(), [])

        self.git("update-index", "--cacheinfo", f"160000,{unexpected},{path}")
        self.git("commit", "-q", "--amend", "--no-edit")
        for style, ignore in (("short", "none"), ("log", "none"), ("diff", "none"), ("short", "all")):
            with self.subTest(style=style, ignore=ignore):
                self.git("config", "diff.submodule", style)
                self.git("config", "diff.ignoreSubmodules", ignore)
                self.assertEqual(self.verify(), [
                    f"{path}: an added line is missing (1x): Subproject commit {expected}",
                    f"{path}: a line is added that the branch did not add (1x): Subproject commit {unexpected}",
                ])

    def test_a_line_added_twice_is_reported(self):
        self.write("f.txt", "a\nNEW\nNEW\nb\nc\ntail\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("a line is added that the branch did not add" in problem for problem in problems), problems)

    def test_a_line_already_added_upstream_needs_no_second_addition(self):
        self.git("checkout", "-q", "old")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", "a\nshared\nbranch-only\nb\nc\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared lines")
        self.git("checkout", "-q", "main")
        self.write("f.txt", "a\nshared\nb\nc\n")
        self.git("commit", "-q", "-am", "docs(tools): add the shared line upstream")
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.write("f.txt", "a\nshared\nbranch-only\nb\nc\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared lines")
        self.assertEqual(self.verify(), [])

    def test_an_extra_copy_of_a_line_already_added_upstream_is_reported(self):
        self.git("checkout", "-q", "old")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", "a\nshared\nbranch-only\nb\nc\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared lines")
        self.git("checkout", "-q", "main")
        self.write("f.txt", "a\nshared\nb\nc\n")
        self.git("commit", "-q", "-am", "docs(tools): add the shared line upstream")
        self.git("checkout", "-q", "new")
        self.git("reset", "-q", "--hard", "main")
        self.write("f.txt", "a\nshared\nshared\nbranch-only\nb\nc\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared lines")
        problems = self.verify()
        self.assertTrue(any("a line is added that the branch did not add" in problem and "shared" in problem
                            for problem in problems), problems)

    def test_large_duplicate_hunk_is_matched_without_recursion(self):
        base = "header\nsection B\nfooter\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add duplicate scale fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        repeated = "repeated entry\n" * 100_001
        self.write("f.txt", "header\n" + repeated + "section B\nbranch-only\nfooter\n")
        self.git("commit", "-q", "-am", "feat(tools): add repeated and unique lines")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "header\n" + repeated + "section B\nfooter\n")
        self.git("commit", "-q", "-am", "docs(tools): add repeated lines upstream")
        self.git("checkout", "-q", "-B", "new", "old")
        self.git("rebase", "-q", "main")
        started = time.perf_counter()
        self.assertEqual(self.verify(), [])
        self.assertLess(time.perf_counter() - started, 10.0)

    def prepare_reordered_block_rebase(self, preserve_branch_block):
        base = "start\nend\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add ordered block fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "start\nlock();\nuse();\nend\n")
        self.git("commit", "-q", "-am", "feat(tools): add ordered lock block")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        upstream = "start\nuse();\nlock();\nend\n"
        self.write("f.txt", upstream)
        self.git("commit", "-q", "-am", "docs(tools): add reversed block upstream")
        resolved = (upstream if not preserve_branch_block
                    else "start\nuse();\nlock();\nlock();\nuse();\nend\n")
        new_commit = self.commit_tree_file("main", "f.txt", resolved.encode("utf-8"),
                                           "feat(tools): add ordered lock block")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_reordered_upstream_block_does_not_satisfy_branch_additions(self):
        self.prepare_reordered_block_rebase(False)
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "lock();" in problem
                            for problem in problems), problems)
        self.assertTrue(any("an added line is missing" in problem and "use();" in problem
                            for problem in problems), problems)

    def test_reordered_upstream_block_does_not_duplicate_a_preserved_branch_block(self):
        self.prepare_reordered_block_rebase(True)
        self.assertEqual(self.verify(), [])

    def prepare_partially_credited_block_rebase(self, preserve_branch_order):
        base = "start\nend\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add partial-credit order fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "start\nlock();\nuse();\nend\n")
        self.git("commit", "-q", "-am", "feat(tools): add ordered lock block")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "start\nuse();\nend\n")
        self.git("commit", "-q", "-am", "docs(tools): add the shared use line upstream")
        resolved = ("start\nlock();\nuse();\nend\n" if preserve_branch_order
                    else "start\nuse();\nlock();\nend\n")
        new_commit = self.commit_tree_file("main", "f.txt", resolved.encode("utf-8"),
                                           "feat(tools): add ordered lock block")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_partially_credited_upstream_line_cannot_reorder_branch_additions(self):
        self.prepare_partially_credited_block_rebase(False)
        problems = self.verify()
        self.assertTrue(any("out of order after upstream credit" in problem for problem in problems), problems)

    def test_partially_credited_upstream_line_keeps_branch_addition_order(self):
        self.prepare_partially_credited_block_rebase(True)
        self.assertEqual(self.verify(), [])

    def prepare_if_block_rebase(self, preserve_branch_block):
        base = "start\nend\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add if-block fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        branch_block = "if (feature) {\n  lock();\n  use();\n}\n"
        self.write("f.txt", "start\n" + branch_block + "end\n")
        self.git("commit", "-q", "-am", "feat(tools): add branch if block")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        upstream_block = "if (feature) {\n  use();\n  lock();\n}\n"
        upstream = "start\n" + upstream_block + "end\n"
        self.write("f.txt", upstream)
        self.git("commit", "-q", "-am", "docs(tools): add a different if block upstream")
        resolved = (upstream if not preserve_branch_block
                    else "start\n" + upstream_block + branch_block + "end\n")
        new_commit = self.commit_tree_file("main", "f.txt", resolved.encode("utf-8"),
                                           "feat(tools): add branch if block")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_reordered_upstream_if_block_does_not_replace_the_branch_block(self):
        self.prepare_if_block_rebase(False)
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "lock();" in problem
                            for problem in problems), problems)
        self.assertTrue(any("an added line is missing" in problem and "use();" in problem
                            for problem in problems), problems)

    def test_branch_if_block_is_preserved_beside_a_reordered_upstream_block(self):
        self.prepare_if_block_rebase(True)
        self.assertEqual(self.verify(), [])

    def prepare_contained_block_rebase(self):
        base = "start\nend\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add contained block fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "start\nlock();\nuse();\nend\n")
        self.git("commit", "-q", "-am", "feat(tools): add ordered lock block")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        upstream = "start\nprefix();\nlock();\nuse();\nsuffix();\nend\n"
        self.write("f.txt", upstream)
        self.git("commit", "-q", "-am", "docs(tools): add the ordered block upstream")
        new_commit = self.commit_tree_file("main", "f.txt", upstream.encode("utf-8"),
                                           "feat(tools): add ordered lock block")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_a_contiguous_upstream_block_satisfies_the_branch_block(self):
        self.prepare_contained_block_rebase()
        self.assertEqual(self.verify(), [])

    def prepare_shorter_overlap_rebase(self):
        base = "a\npivot\nb\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add repeated overlap fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "a\nX\npivot\nX\nX\nb\n")
        self.git("commit", "-q", "-am", "feat(tools): add repeated boundary lines")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "a\nX\nX\nb\n")
        self.git("commit", "-q", "-am", "docs(tools): replace pivot with repeated lines")
        new_commit = self.commit_tree_file("main", "f.txt", "a\nX\nX\nX\nb\n".encode("utf-8"),
                                           "feat(tools): add repeated boundary lines")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_shorter_available_overlap_is_used_when_longest_was_consumed(self):
        self.prepare_shorter_overlap_rebase()
        self.assertEqual(self.verify(), [])

    def test_large_boundary_overlap_availability_is_not_quadratic(self):
        count = 50_000
        base = "a\npivot\nb\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add large boundary overlap fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "a\nX\npivot\n" + "X\n" * count + "b\n")
        self.git("commit", "-q", "-am", "feat(tools): add repeated boundary lines")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "a\n" + "X\n" * count + "b\n")
        self.git("commit", "-q", "-am", "docs(tools): replace pivot with repeated lines")
        new_commit = self.commit_tree_file("main", "f.txt", ("a\n" + "X\n" * (count + 1) + "b\n").encode("utf-8"),
                                           "feat(tools): add repeated boundary lines")
        self.git("update-ref", "refs/heads/new", new_commit)
        started = time.perf_counter()
        self.assertEqual(self.verify(), [])
        self.assertLess(time.perf_counter() - started, 10.0)

    def prepare_duplicate_upstream_line_rebase(self):
        base = "a\nb\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add duplicate credit fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "a\nA\nX\nA\nX\nb\n")
        self.git("commit", "-q", "-am", "feat(tools): add duplicate boundary lines")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "a\nX\nb\n")
        self.git("commit", "-q", "-am", "docs(tools): add one shared X upstream")
        new_commit = self.commit_tree_file("main", "f.txt", "a\nA\nX\nA\nX\nb\n".encode("utf-8"),
                                           "feat(tools): add duplicate boundary lines")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_duplicate_upstream_lines_are_credited_at_ordered_rebased_positions(self):
        self.prepare_duplicate_upstream_line_rebase()
        self.assertEqual(self.verify(), [])

    def prepare_separated_shared_line_rebase(self):
        base = "section A\na1\na2\na3\nsection B\nb1\nb2\nb3\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add separated sections")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "section A\nshared\nbranch-only\na1\na2\na3\nsection B\nb1\nb2\nb3\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared branch behavior")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        self.write("f.txt", "section A\na1\na2\na3\nsection B\nshared\nb1\nb2\nb3\n")
        self.git("commit", "-q", "-am", "docs(tools): add distant shared text upstream")
        self.git("checkout", "-q", "-B", "new", "old")
        self.git("rebase", "-q", "main")

    def test_a_distant_upstream_copy_does_not_replace_a_branch_addition(self):
        self.prepare_separated_shared_line_rebase()
        self.assertEqual(self.verify(), [])

    def test_dropping_a_branch_addition_with_a_distant_upstream_copy_is_reported(self):
        self.prepare_separated_shared_line_rebase()
        self.write("f.txt", "section A\nbranch-only\na1\na2\na3\nsection B\nshared\nb1\nb2\nb3\n")
        self.git("commit", "-q", "-a", "--amend", "--no-edit")
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "shared" in problem for problem in problems), problems)

    def prepare_moved_shared_line_rebase(self, keep_upstream_copy):
        base = "section A\nshared\nsection B\nextra\nsection C\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add moved-line fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        self.write("f.txt", "section A\nsection B\nsection C\nshared\nbranch-only\n")
        self.git("commit", "-q", "-am", "feat(tools): move shared line and remove extra")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        upstream = "section A\nshared\nsection B\nextra\nsection C\nshared\n"
        self.write("f.txt", upstream)
        self.git("commit", "-q", "-am", "docs(tools): add the moved line upstream")
        resolved = ("section A\nsection B\nsection C\nshared\nbranch-only\n" if keep_upstream_copy
                    else "section A\nshared\nsection B\nsection C\nbranch-only\n")
        new_commit = self.commit_tree_file("main", "f.txt", resolved.encode("utf-8"),
                                           "feat(tools): move shared line and remove extra")
        self.git("update-ref", "refs/heads/new", new_commit)

    def verify_with_inter_hunk_context(self):
        global_config = self.root / "empty-global-config"
        global_config.write_text("")
        environment = {
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": str(global_config),
            "GIT_CONFIG_COUNT": "1",
            "GIT_CONFIG_KEY_0": "diff.interHunkContext",
            "GIT_CONFIG_VALUE_0": "1",
        }
        previous = {key: os.environ.get(key) for key in environment}
        try:
            os.environ.update(environment)
            return self.verify()
        finally:
            for key, value in previous.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value

    def test_moved_line_keeps_the_upstream_occurrence_that_satisfies_it(self):
        self.prepare_moved_shared_line_rebase(True)
        self.assertEqual(self.verify_with_inter_hunk_context(), [])

    def test_moved_line_cannot_keep_the_original_and_delete_its_upstream_copy(self):
        self.prepare_moved_shared_line_rebase(False)
        problems = self.verify_with_inter_hunk_context()
        self.assertTrue(any("upstream-added line credited for a branch addition was removed at line 6" in problem
                            for problem in problems), problems)


    def prepare_adjacent_shared_line_rebase(self, side, replacement, shared_at_boundary=True, branch_only_at_boundary=False,
                                            boundary_lines=("shared",)):
        base = "line 1\nline 2\nline 3\nline 4\nline 5\nsection B\nb1\nb2\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add adjacent-rebase fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        branch_additions = list(boundary_lines)
        if branch_only_at_boundary:
            branch_additions = (["branch-only"] + list(boundary_lines) if side == "following"
                                else list(boundary_lines) + ["branch-only"])
        topic = ["line 1", "line 2", "line 3", *branch_additions, "line 4", "line 5", "section B"]
        if not branch_only_at_boundary:
            topic.append("branch-only")
        topic.extend(["b1", "b2"])
        self.write("f.txt", "\n".join(topic) + "\n")
        self.git("commit", "-q", "-am", "feat(tools): add shared branch behavior")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        if side == "following":
            replacement_lines = (list(boundary_lines) + ["upstream replacement"] if shared_at_boundary
                                 else ["upstream replacement"] + list(boundary_lines)) if replacement else list(boundary_lines)
            upstream = ["line 1", "line 2", "line 3", *replacement_lines, "line 5", "section B", "b1", "b2"]
        else:
            replacement_lines = (["upstream replacement"] + list(boundary_lines) if shared_at_boundary
                                 else list(boundary_lines) + ["upstream replacement"]) if replacement else list(boundary_lines)
            upstream = ["line 1", "line 2", *replacement_lines, "line 4", "line 5", "section B", "b1", "b2"]
        upstream_text = "\n".join(upstream) + "\n"
        self.write("f.txt", upstream_text)
        self.git("commit", "-q", "-am", "docs(tools): change the adjacent line upstream")
        self.git("checkout", "-q", "-B", "new", "old")
        environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1", "GIT_EDITOR": "true"}
        result = subprocess.run(["git", "-C", str(self.root), "-c", "user.name=Rebase Test", "-c", "user.email=test@example.com",
                                 "-c", "commit.gpgsign=false", "rebase", "main"], capture_output=True, text=True, env=environment)
        self.assertNotEqual(result.returncode, 0, (side, replacement, result.stdout, result.stderr))
        if branch_only_at_boundary:
            if side == "following":
                resolved = upstream_text.replace(boundary_lines[0] + "\n", "branch-only\n" + boundary_lines[0] + "\n", 1)
            else:
                resolved = upstream_text.replace(boundary_lines[-1] + "\n",
                                                  boundary_lines[-1] + "\nbranch-only\n", 1)
        else:
            resolved = upstream_text.replace("section B\n", "section B\nbranch-only\n", 1)
        self.write("f.txt", resolved)
        self.git("add", "f.txt")
        self.git("-c", "core.editor=true", "rebase", "--continue")

    def test_upstream_addition_at_boundary_survives_following_replacement(self):
        self.prepare_adjacent_shared_line_rebase("following", True)
        self.assertEqual(self.verify(), [])

    def test_upstream_addition_at_boundary_survives_following_deletion(self):
        self.prepare_adjacent_shared_line_rebase("following", False)
        self.assertEqual(self.verify(), [])

    def test_upstream_addition_at_boundary_survives_preceding_replacement(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True)
        self.assertEqual(self.verify(), [])

    def test_upstream_addition_at_boundary_survives_preceding_deletion(self):
        self.prepare_adjacent_shared_line_rebase("preceding", False)
        self.assertEqual(self.verify(), [])

    def test_branch_only_before_following_boundary_match_is_preserved(self):
        self.prepare_adjacent_shared_line_rebase("following", True, branch_only_at_boundary=True)
        self.assertEqual(self.verify(), [])

    def test_branch_only_after_preceding_boundary_match_is_preserved(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True, branch_only_at_boundary=True)
        self.assertEqual(self.verify(), [])

    def test_two_following_boundary_lines_keep_their_order(self):
        self.prepare_adjacent_shared_line_rebase("following", True, boundary_lines=("first", "second"))
        self.assertEqual(self.verify(), [])

    def test_two_preceding_boundary_lines_keep_their_order(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True, boundary_lines=("first", "second"))
        self.assertEqual(self.verify(), [])

    def test_two_following_boundary_lines_in_wrong_order_do_not_match(self):
        self.prepare_adjacent_shared_line_rebase("following", True, shared_at_boundary=False,
                                                 boundary_lines=("first", "second"))
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "first" in problem for problem in problems), problems)
        self.assertTrue(any("an added line is missing" in problem and "second" in problem for problem in problems), problems)

    def test_two_preceding_boundary_lines_in_wrong_order_do_not_match(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True, shared_at_boundary=False,
                                                 boundary_lines=("first", "second"))
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "first" in problem for problem in problems), problems)
        self.assertTrue(any("an added line is missing" in problem and "second" in problem for problem in problems), problems)

    def test_branch_only_before_two_following_boundary_lines_is_preserved(self):
        self.prepare_adjacent_shared_line_rebase("following", True, branch_only_at_boundary=True,
                                                 boundary_lines=("first", "second"))
        self.assertEqual(self.verify(), [])

    def test_branch_only_after_two_preceding_boundary_lines_is_preserved(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True, branch_only_at_boundary=True,
                                                 boundary_lines=("first", "second"))
        self.assertEqual(self.verify(), [])

    def prepare_double_credit_boundary_rebase(self, keep_branch_copy):
        base = "line 1\nline 2\nline 3\nold pivot\nline 5\nsection B\nb1\nb2\n"
        self.git("checkout", "-q", "main")
        self.git("reset", "-q", "--hard", self.base)
        self.write("f.txt", base)
        self.git("commit", "-q", "-am", "docs(tools): add double-credit fixture")
        fixture_base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-B", "old", fixture_base)
        topic = ("line 1\nline 2\nline 3\nmatching line\nold pivot\nmatching line\n"
                 "line 5\nsection B\nbranch-only\nb1\nb2\n")
        self.write("f.txt", topic)
        self.git("commit", "-q", "-am", "feat(tools): add matching boundary lines")
        self.git("checkout", "-q", "-B", "main", fixture_base)
        upstream = "line 1\nline 2\nline 3\nmatching line\nline 5\nsection B\nb1\nb2\n"
        self.write("f.txt", upstream)
        self.git("commit", "-q", "-am", "docs(tools): replace pivot upstream")
        resolved = ("line 1\nline 2\nline 3\nmatching line\nmatching line\nline 5\n"
                    "section B\nbranch-only\nb1\nb2\n" if keep_branch_copy
                    else "line 1\nline 2\nline 3\nmatching line\nline 5\nsection B\nbranch-only\nb1\nb2\n")
        new_commit = self.commit_tree_file("main", "f.txt", resolved.encode("utf-8"),
                                           "feat(tools): add matching boundary lines")
        self.git("update-ref", "refs/heads/new", new_commit)

    def test_one_upstream_occurrence_cannot_satisfy_two_branch_additions_when_one_is_lost(self):
        self.prepare_double_credit_boundary_rebase(keep_branch_copy=False)
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "matching line" in problem for problem in problems), problems)
        self.assertFalse(any("a line is added that the branch did not add" in problem and "matching line" in problem for problem in problems), problems)

    def test_one_upstream_occurrence_can_satisfy_only_one_of_two_branch_additions(self):
        self.prepare_double_credit_boundary_rebase(keep_branch_copy=True)
        self.assertEqual(self.verify(), [])

    def test_following_replacement_only_satisfies_its_boundary_prefix(self):
        self.prepare_adjacent_shared_line_rebase("following", True, shared_at_boundary=False)
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "shared" in problem for problem in problems), problems)

    def test_preceding_replacement_only_satisfies_its_boundary_suffix(self):
        self.prepare_adjacent_shared_line_rebase("preceding", True, shared_at_boundary=False)
        problems = self.verify()
        self.assertTrue(any("an added line is missing" in problem and "shared" in problem for problem in problems), problems)

    def amend_bytes(self, branch, data):
        self.git("checkout", "-q", branch)
        (self.root / "n.txt").write_bytes(data)
        self.git("-c", "core.autocrlf=false", "add", "n.txt")
        self.git("commit", "-q", "--amend", "--no-edit")

    def test_a_dropped_final_newline_is_reported(self):
        self.amend_bytes("old", b"first\nlast\n")
        self.amend_bytes("new", b"first\nlast")
        self.assertEqual(self.verify(), [
            "n.txt: an added line is missing (1x): last",
            "n.txt: a line is added that the branch did not add (1x): last (no newline at end of file)",
        ])
        self.amend_bytes("new", b"first\nlast\n")
        self.assertEqual(self.verify(), [])

    def commit_bytes(self, data, message):
        (self.root / "n.txt").write_bytes(data)
        self.git("-c", "core.autocrlf=false", "add", "n.txt")
        self.git("commit", "-q", "-m", message)

    def check_deletion_across_upstream_newline_change(self, base, upstream):
        self.git("checkout", "-q", "main")
        self.commit_bytes(base, "docs(tools): add n")
        self.git("checkout", "-q", "-B", "old", "main")
        self.commit_bytes(b"keep\n", "fix(tools): drop the last line")
        self.git("checkout", "-q", "main")
        self.commit_bytes(upstream, "docs(tools): change the end of n")
        self.git("checkout", "-q", "-B", "new", "main")
        self.commit_bytes(b"keep\n", "fix(tools): drop the last line")
        self.assertEqual(self.verify(), [])
        self.git("reset", "-q", "--hard", "main")
        self.git("commit", "-q", "--allow-empty", "-m", "fix(tools): drop the last line")
        self.assertEqual(self.verify(), ["n.txt: a deleted line is missing (1x): last"])

    def test_a_deletion_is_checked_after_main_adds_the_final_newline(self):
        self.check_deletion_across_upstream_newline_change(b"keep\nlast", b"keep\nlast\n")

    def test_a_deletion_is_checked_after_main_drops_the_final_newline(self):
        self.check_deletion_across_upstream_newline_change(b"keep\nlast\n", b"keep\nlast")

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
        with patch.object(rebase_verify, "git", return_value=diff.encode("utf-8")):
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

    def test_only_an_added_line_without_a_final_newline_differs_from_the_terminated_line(self):
        marker = "\n" + rebase_verify.NO_NEWLINE
        old, _ = self.changes("tail")
        new, _ = self.changes("tail" + marker)
        self.assertEqual(new["f.cpp"], {"tail" + marker: 1})
        self.assertEqual(rebase_verify.differences(old, new), [
            "f.cpp: an added line is missing (1x): tail",
            "f.cpp: a line is added that the branch did not add (1x): tail (no newline at end of file)",
        ])
        _, deleted = self.changes("tail" + marker, "-")
        self.assertEqual(deleted["f.cpp"], {"tail": 1})
        spoofed, _ = self.changes("tail\n\\literal")
        self.assertEqual(spoofed["f.cpp"], {"tail": 1})


class CountHelperTests(unittest.TestCase):
    def test_people_keeps_unicode_line_separators_inside_each_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run_git(root, "init", "-q")
            run_git(root, "commit", "-q", "--allow-empty", "-m", "base")
            run_git(root, "commit", "-q", "--allow-empty", "-m", "first\u2028second")
            people = rebase_verify.people(root, "HEAD^", "HEAD")
            self.assertEqual(sum(people.values()), 1)
            self.assertIn("first\u2028second", next(iter(people)))

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


class ModeDifferencesTests(unittest.TestCase):
    def differences(self, table):
        with patch.object(rebase_verify, "changed_modes", side_effect=lambda root, base, ref: table.get((base, ref), {})):
            return rebase_verify.mode_differences("root", table["old_base"], "old", table["new_base"], "new")

    def test_preserved_mode_changes_have_no_problems(self):
        change = {b"a": ("100644", "100755")}
        table = {"old_base": "b", "new_base": "b", ("b", "old"): change, ("b", "new"): change}
        self.assertEqual(self.differences(table), [])

    def test_lost_mode_change_is_reported(self):
        table = {"old_base": "b", "new_base": "b", ("b", "old"): {b"a": ("100644", "100755")}}
        self.assertEqual(self.differences(table), ["a: file mode 100644, expected 100755"])

    def test_mode_already_applied_by_the_new_base_is_accepted(self):
        change = {b"a": ("100644", "100755")}
        table = {"old_base": "b0", "new_base": "b1", ("b0", "old"): change, ("b0", "b1"): change}
        self.assertEqual(self.differences(table), [])

    def test_mode_change_only_in_the_new_branch_is_reported(self):
        table = {"old_base": "b", "new_base": "b", ("b", "new"): {b"c": ("100755", "100644")}}
        self.assertEqual(self.differences(table), ["c: file mode 100644, expected 100755"])

    def test_non_utf8_paths_are_escaped(self):
        table = {"old_base": "b", "new_base": "b", ("b", "old"): {b"\xff": ("100644", "100755")}}
        self.assertEqual(self.differences(table), ["\\xff: file mode 100644, expected 100755"])


class BinaryRebase(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.autocrlf", "false")
        self.write(b"\0base")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "old")
        self.write(b"\0expected")
        self.git("add", "asset.bin")
        self.git("commit", "--allow-empty", "-qm", "fix(tools): update asset")
        self.old = self.git("rev-parse", "HEAD")

    def git(self, *args):
        result = subprocess.run(["git", "-C", str(self.root), *args], capture_output=True, text=True, check=True)
        return result.stdout.strip()

    def write(self, data):
        (self.root / "asset.bin").write_bytes(data)

    def new_commit(self, data):
        self.git("checkout", "-q", "-b", "new", self.base)
        self.write(data)
        self.git("add", "asset.bin")
        self.git("commit", "--allow-empty", "-qm", "fix(tools): update asset")
        return self.git("rev-parse", "HEAD")

    def test_unchanged_binary_change_is_accepted(self):
        new = self.new_commit(b"\0expected")
        self.assertEqual(rebase_verify.verify(self.root, self.old, new, self.base)[3], [])

    def test_changed_binary_content_must_be_reported(self):
        new = self.new_commit(b"\0corrupt")
        self.assertTrue(rebase_verify.verify(self.root, self.old, new, self.base)[3])

    def test_reverted_binary_content_must_be_reported(self):
        new = self.new_commit(b"\0base")
        self.assertTrue(rebase_verify.verify(self.root, self.old, new, self.base)[3])

    def test_upstream_binary_change_with_same_result_is_valid(self):
        self.git("checkout", "-q", "-b", "upstream", self.base)
        self.write(b"\0upstream")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "upstream changed binary")
        self.write(b"\0expected")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "fix(tools): update asset")
        self.assertEqual(rebase_verify.verify(self.root, self.old, "HEAD", "upstream~1")[3], [])

    def test_upstream_binary_conflict_requires_manual_review(self):
        self.git("checkout", "-q", "-b", "upstream", self.base)
        self.write(b"\0upstream")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "upstream changed binary")
        self.git("checkout", "-qb", "new")
        self.write(b"\0resolved")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "fix(tools): update asset")
        problems = rebase_verify.verify(self.root, self.old, "HEAD", "upstream")[3]
        self.assertTrue(any("binary" in problem.lower() and "review" in problem.lower() for problem in problems), problems)

    def test_upstream_already_has_expected_binary(self):
        self.git("checkout", "-q", "-b", "upstream", self.base)
        self.write(b"\0expected")
        self.git("add", "asset.bin")
        self.git("commit", "-qm", "upstream changed binary")
        self.git("checkout", "-qb", "new")
        self.git("commit", "--allow-empty", "-qm", "fix(tools): update asset")
        self.assertEqual(rebase_verify.verify(self.root, self.old, "HEAD", "upstream")[3], [])

    def test_deleted_binary_is_checked(self):
        self.git("checkout", "-q", "old")
        self.git("rm", "-q", "asset.bin")
        self.git("commit", "-q", "--amend", "--no-edit")
        self.old = self.git("rev-parse", "HEAD")
        new = self.new_commit(b"\0base")
        self.assertTrue(any("binary" in problem.lower() for problem in rebase_verify.verify(self.root, self.old, new, self.base)[3]))

    def test_new_unexpected_binary_change_requires_review(self):
        self.git("checkout", "-q", "old")
        self.write(b"\0base")
        (self.root / "note.txt").write_text("content\n", encoding="utf-8")
        self.git("add", "asset.bin", "note.txt")
        self.git("commit", "-q", "--amend", "--no-edit")
        self.old = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-b", "new", self.base)
        self.write(b"\0unexpected")
        (self.root / "note.txt").write_text("content\n", encoding="utf-8")
        self.git("add", "asset.bin", "note.txt")
        self.git("commit", "-qm", "fix(tools): update asset")
        problems = rebase_verify.verify(self.root, self.old, "HEAD", self.base)[3]
        self.assertTrue(any("binary" in problem.lower() and "review" in problem.lower() for problem in problems), problems)

    def test_unicode_and_space_path(self):
        self.git("checkout", "-q", "-b", "base2", self.base)
        name = "asset é space.bin"
        (self.root / name).write_bytes(b"\0base")
        self.git("add", name)
        self.git("commit", "-qm", "add binary asset")
        base = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "old2")
        (self.root / name).write_bytes(b"\0expected")
        self.git("add", name)
        self.git("commit", "-qm", "fix(tools): update asset")
        old = self.git("rev-parse", "HEAD")
        self.git("checkout", "-qb", "new2", base)
        (self.root / name).write_bytes(b"\0other")
        self.git("add", name)
        self.git("commit", "-qm", "fix(tools): update asset")
        problems = rebase_verify.verify(self.root, old, "HEAD", base)[3]
        self.assertTrue(any(name in problem and "binary" in problem.lower() for problem in problems), problems)

    def test_added_binary_is_checked(self):
        self.git("checkout", "-q", "old")
        (self.root / "added.bin").write_bytes(b"\0expected-new")
        self.git("add", "added.bin")
        self.git("commit", "-q", "--amend", "--no-edit")
        self.old = self.git("rev-parse", "HEAD")
        new = self.new_commit(b"\0expected")
        problems = rebase_verify.verify(self.root, self.old, new, self.base)[3]
        self.assertTrue(any("added.bin" in problem and "review" in problem.lower() for problem in problems), problems)

    def test_added_binary_preserved_is_accepted(self):
        self.git("checkout", "-q", "old")
        (self.root / "added.bin").write_bytes(b"\0expected-new")
        self.git("add", "added.bin")
        self.git("commit", "-q", "--amend", "--no-edit")
        self.old = self.git("rev-parse", "HEAD")
        self.git("checkout", "-q", "-b", "new", self.base)
        self.write(b"\0expected")
        (self.root / "added.bin").write_bytes(b"\0expected-new")
        self.git("add", "asset.bin", "added.bin")
        self.git("commit", "-qm", "fix(tools): update asset")
        self.assertEqual(rebase_verify.verify(self.root, self.old, "HEAD", self.base)[3], [])

    def test_cli_describes_binary_difference_as_needing_review(self):
        output = io.StringIO()
        with patch.object(rebase_verify, "default_onto", return_value="main"), \
                patch.object(rebase_verify, "verify", return_value=("a" * 40, 1, 1, ["asset.bin: binary content differs; manual review required"])), \
                contextlib.redirect_stdout(output):
            self.assertEqual(rebase_verify.main(["old", "new"]), 1)
        self.assertIn("require review", output.getvalue())
        self.assertNotIn("other than conflict resolution", output.getvalue())


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--shard":
        if len(sys.argv) != 3 or "/" not in sys.argv[2]:
            print("usage: test_rebase_verify.py --shard INDEX/COUNT", file=sys.stderr)
            raise SystemExit(2)
        try:
            shard_index, shard_count = (int(part) for part in sys.argv[2].split("/", 1))
            suite = rebase_verify_test_shard(shard_index, shard_count)
        except ValueError as error:
            print(f"invalid shard: {error}", file=sys.stderr)
            raise SystemExit(2)
        result = unittest.TextTestRunner().run(suite)
        raise SystemExit(0 if result.wasSuccessful() else 1)
    unittest.main()
