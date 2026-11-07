import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def run(*args, date=None):
    env = dict(os.environ)
    if date:
        env["GIT_COMMITTER_DATE"] = date
        env["GIT_AUTHOR_DATE"] = date
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, text=True, check=True, env=env).stdout.strip()


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def commit(message, date, **files):
    for name, text in files.items():
        write(name.replace("_", "."), text)
    run("add", "-A")
    run("commit", "-q", "-m", message, date=date)
    return run("rev-parse", "HEAD")


class MergeBaseTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        write("a.txt", "one\ntwo\nthree\n")
        root = commit("root", "2026-01-01T00:00:00Z", a_txt="one\ntwo\nthree\n")
        run("checkout", "-q", "-b", "main-side")
        big = {f"big{index}_txt": "main\n" for index in range(6)}
        main_side = commit("main side", "2026-01-02T00:00:00Z", **big)
        run("checkout", "-q", "--detach", root)
        pr_side = commit("pr side", "2026-01-03T00:00:00Z", pr_txt="pr\n")
        run("checkout", "-q", "--detach", main_side)
        run("merge", "-q", "--no-edit", pr_side, date="2026-01-04T00:00:00Z")
        base = commit("main change", "2026-01-05T00:00:00Z", a_txt="main\ntwo\nthree\n")
        run("update-ref", "refs/pr/base", base)
        run("checkout", "-q", "--detach", pr_side)
        run("merge", "-q", "--no-edit", main_side, date="2026-01-04T00:00:00Z")
        self.head = commit("pull request change", "2026-01-06T00:00:00Z", a_txt="pr\ntwo\nthree\n")
        self.pr = {"ref": self.head, "sha": self.head, "author": "author", "depends": set()}

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def test_the_fixture_has_two_merge_bases_and_git_picks_the_larger_diff(self):
        self.assertEqual(len(run("merge-base", "--all", "refs/pr/base", self.head).split()), 2)
        picked = run("merge-base", "refs/pr/base", self.head)
        self.assertGreater(len(run("diff", "--name-only", picked, self.head).split()), 2)

    def test_a_conflicting_pull_request_is_diffed_from_the_base_with_the_smallest_diff(self):
        pr_overlap.scan(self.pr)
        self.assertEqual(self.pr["files"], {"a.txt", "pr.txt"})

    def test_a_single_merge_base_costs_no_extra_diff(self):
        run("checkout", "-q", "--detach", "refs/pr/base")
        linear = commit("linear", "2026-01-07T00:00:00Z", linear_txt="x\n")
        calls = []
        real = pr_overlap.git

        def record(*args):
            calls.append(args[0])
            return real(*args)

        with patch.object(pr_overlap, "git", side_effect=record):
            base = pr_overlap.merge_base(linear)
        self.assertEqual(base, run("rev-parse", "refs/pr/base"))
        self.assertEqual(calls, ["merge-base"])

    def test_a_branch_probe_is_diffed_from_the_base_with_the_smallest_diff(self):
        files, _, existing = pr_overlap.probe(self.pr)
        self.assertEqual(files, {"a.txt", "pr.txt"})
        self.assertEqual(existing, {"a.txt"})


if __name__ == "__main__":
    unittest.main()
