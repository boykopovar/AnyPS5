import copy
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

EXPORT = "int APS5_VABI shared_nid_postfix(int value) { return value; }\n"


def run(*args):
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, text=True, check=True).stdout.strip()


def write(path, text):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def commit_on_base(base, message, files):
    run("checkout", "-q", "--detach", base)
    for path, text in files.items():
        write(path, text)
    run("add", "-A")
    run("commit", "-q", "-m", message)
    return run("rev-parse", "HEAD")


class BranchScanTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        for name in ("a.txt", "b.txt", "c.txt"):
            write(name, "one\ntwo\nthree\n")
        write("core/libs/prx/lib/Existing.cpp", "int Existing() { return 0; }\n")
        run("add", "-A")
        run("commit", "-q", "-m", "base")
        base = run("rev-parse", "HEAD")
        run("update-ref", "refs/pr/base", base)
        branch = commit_on_base(base, "branch", {"a.txt": "branch\ntwo\nthree\n", "core/libs/prx/lib/Branch.cpp": EXPORT})
        numbers = {
            1: commit_on_base(base, "same lines", {"a.txt": "other\ntwo\nthree\n"}),
            2: commit_on_base(base, "unrelated file", {"c.txt": "one\ntwo\nchanged\n"}),
            3: commit_on_base(base, "same export elsewhere", {"core/libs/prx/lib/Other.cpp": EXPORT}),
            4: commit_on_base(base, "unrelated export", {"core/libs/prx/lib/Third.cpp": "int APS5_VABI other_nid_postfix(int value) { return value; }\n"}),
        }
        for number, sha in numbers.items():
            run("update-ref", f"refs/pr/{number}", sha)
        self.prs = {n: {"ref": f"refs/pr/{n}", "sha": sha, "author": f"author{n}", "depends": set()} for n, sha in numbers.items()}
        self.prs[0] = {"ref": branch, "author": None, "depends": set()}

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def test_reports_match_a_scan_of_every_pull_request(self):
        everything = copy.deepcopy(self.prs)
        for pr in everything.values():
            pr_overlap.scan(pr)
        pr_overlap.scan_all(self.prs, 2)

        for number in (1, 2, 3, 4):
            with self.subTest(number=number):
                self.assertEqual(pr_overlap.overlaps(self.prs, 0, number), pr_overlap.overlaps(everything, 0, number))
        self.assertIn("a.txt", pr_overlap.overlaps(self.prs, 0, 1)["conflicts"])
        self.assertEqual(pr_overlap.overlaps(self.prs, 0, 3)["exports"], {"shared"})
        self.assertIsNone(pr_overlap.overlaps(self.prs, 0, 2))
        self.assertIsNone(pr_overlap.overlaps(self.prs, 0, 4))

    def test_only_pull_requests_sharing_a_file_or_an_export_are_merged(self):
        merged = []

        def record(*args):
            merged.append(args[1])
            return real(*args)

        real = pr_overlap.merge
        with patch.object(pr_overlap, "merge", side_effect=record):
            pr_overlap.scan_all(self.prs, 2)

        self.assertCountEqual(merged, [self.prs[0]["ref"], "refs/pr/1", "refs/pr/3"])

    def test_scanning_without_a_branch_still_merges_every_pull_request(self):
        merged = []

        def record(*args):
            merged.append(args[1])
            return real(*args)

        real = pr_overlap.merge
        del self.prs[0]
        with patch.object(pr_overlap, "merge", side_effect=record):
            pr_overlap.scan_all(self.prs, 2)

        self.assertCountEqual(merged, ["refs/pr/1", "refs/pr/2", "refs/pr/3", "refs/pr/4"])

    def test_a_pull_request_on_a_path_the_base_renamed_is_still_compared(self):
        old = run("rev-parse", "refs/pr/base")
        older = commit_on_base(old, "older pull request", {"b.txt": "older\ntwo\nthree\n"})
        run("checkout", "-q", "--detach", old)
        run("mv", "b.txt", "renamed.txt")
        run("commit", "-q", "-m", "rename on main")
        base = run("rev-parse", "HEAD")
        run("update-ref", "refs/pr/base", base)
        branch = commit_on_base(base, "branch", {"renamed.txt": "branch\ntwo\nthree\n"})
        run("update-ref", "refs/pr/5", older)
        prs = {5: {"ref": "refs/pr/5", "sha": older, "author": "author5", "depends": set()},
               0: {"ref": branch, "author": None, "depends": set()}}
        everything = copy.deepcopy(prs)
        for pr in everything.values():
            pr_overlap.scan(pr)
        pr_overlap.scan_all(prs, 2)
        expected = pr_overlap.overlaps(everything, 0, 5)
        self.assertIsNotNone(expected)
        self.assertIn("renamed.txt", expected["conflicts"])
        self.assertEqual(pr_overlap.overlaps(prs, 0, 5), expected)

    def test_a_pull_request_on_a_path_the_base_deleted_is_merged(self):
        old = run("rev-parse", "refs/pr/base")
        older = commit_on_base(old, "older pull request", {"c.txt": "one\ntwo\nolder\n"})
        run("checkout", "-q", "--detach", old)
        run("rm", "-q", "c.txt")
        run("commit", "-q", "-m", "delete on main")
        base = run("rev-parse", "HEAD")
        run("update-ref", "refs/pr/base", base)
        run("update-ref", "refs/pr/6", older)
        prs = {6: {"ref": "refs/pr/6", "sha": older, "author": "author6", "depends": set()},
               0: {"ref": commit_on_base(base, "branch", {"a.txt": "branch\ntwo\nthree\n"}), "author": None, "depends": set()}}
        merged = []
        real = pr_overlap.merge

        def record(*args):
            merged.append(args[1])
            return real(*args)

        with patch.object(pr_overlap, "merge", side_effect=record):
            pr_overlap.scan_all(prs, 2)
        self.assertIn("refs/pr/6", merged)


if __name__ == "__main__":
    unittest.main()
