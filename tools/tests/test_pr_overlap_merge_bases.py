import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

MAIN_FILES = [f"main{index}.txt" for index in range(5)]


def run(*args, date="2026-01-01T00:00:00"):
    env = {**os.environ, "GIT_AUTHOR_DATE": date, "GIT_COMMITTER_DATE": date}
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, text=True, check=True, env=env).stdout.strip()


def write(path, text):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def commit(parent, message, files, date="2026-01-01T00:00:00"):
    run("checkout", "-q", "--detach", parent)
    for path, text in files.items():
        write(path, text)
    run("add", "-A")
    run("commit", "-q", "-m", message, date=date)
    return run("rev-parse", "HEAD")


def merge(ours, theirs, message):
    run("checkout", "-q", "--detach", ours)
    run("merge", "-q", "--no-ff", "--no-edit", "-m", message, theirs, date="2026-01-04T00:00:00")
    return run("rev-parse", "HEAD")


class MergeBaseTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        write("shared.txt", "one\ntwo\nthree\n")
        for name in MAIN_FILES:
            write(name, "base\n")
        run("add", "-A")
        run("commit", "-q", "-m", "root")
        root = run("rev-parse", "HEAD")
        main_side = commit(root, "main side", {name: "main\n" for name in MAIN_FILES}, "2026-01-02T00:00:00")
        side = commit(root, "stale copy of part of main", {MAIN_FILES[0]: "main\n"}, "2026-01-03T00:00:00")
        main = merge(main_side, side, "main takes the stale copy")
        head = merge(side, main_side, "pull request takes main")
        self.bases = run("merge-base", "--all", main, head).split()
        self.side = side
        self.main = main
        self.head = head
        run("update-ref", "refs/pr/base", main)

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def pull_request(self, files):
        files = {**files, "core/libs/prx/lib/Pull.cpp": "int APS5_VABI pulled_nid_postfix(int value) { return value; }\n"}
        sha = commit(self.head, "pull request change", files)
        run("update-ref", "refs/pr/1", sha)
        return {"ref": "refs/pr/1", "sha": sha, "author": "author", "depends": set()}

    def test_history_has_two_merge_bases_and_git_picks_the_stale_one(self):
        self.assertEqual(len(self.bases), 2)
        self.assertEqual(run("merge-base", self.main, self.head), self.side)

    def test_conflicting_pull_request_lists_only_its_own_files(self):
        run("update-ref", "refs/pr/base", commit(self.main, "main edits shared", {"shared.txt": "main\ntwo\nthree\n"}))
        pr = self.pull_request({"shared.txt": "pull\ntwo\nthree\n"})

        pr_overlap.scan(pr)

        self.assertIsNone(pr["tree"])
        self.assertEqual(pr["files"], {"shared.txt", "core/libs/prx/lib/Pull.cpp"})
        self.assertEqual(pr["added"], {"core/libs/prx/lib/Pull.cpp"})

    def test_probe_lists_only_the_pull_request_files(self):
        pr = self.pull_request({"shared.txt": "pull\ntwo\nthree\n"})

        files, (exports, _), existing = pr_overlap.probe(pr)

        self.assertEqual(files, {"shared.txt", "core/libs/prx/lib/Pull.cpp"})
        self.assertEqual(existing, {"shared.txt"})
        self.assertEqual(exports, {"pulled"})

    def test_single_merge_base_is_unchanged(self):
        sha = commit(self.main, "plain pull request", {"shared.txt": "plain\ntwo\nthree\n"})
        run("update-ref", "refs/pr/2", sha)

        files, _, _ = pr_overlap.probe({"ref": "refs/pr/2"})

        self.assertEqual(files, {"shared.txt"})


class PrerequisiteHistoryTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.tmp = tempfile.TemporaryDirectory()
        os.chdir(self.tmp.name)
        run("init", "-q")
        run("config", "commit.gpgsign", "false")
        write("config.txt", "fence=old\ninline=old\nend\n")
        write("main.txt", "base\n")
        run("add", "-A")
        run("commit", "-q", "-m", "base")
        self.root = run("rev-parse", "HEAD")
        self.parent = commit(self.root, "shared prerequisite", {"config.txt": "fence=old\nescape=shared\ninline=new\nend\n"})
        self.main = commit(self.root, "main advances independently", {"main.txt": "main\n"})
        run("update-ref", "refs/pr/base", self.main)

    def tearDown(self):
        os.chdir(self.old)
        self.tmp.cleanup()

    def pull_requests(self, conflict=False):
        left = commit(self.parent, "left follow-up", {"config.txt": "fence=left\nfence=expanded\nescape=shared\ninline=new\nend\n"})
        changes = {"config.txt": "fence=right\nescape=shared\ninline=new\nend\n"} if conflict else {"right.txt": "right\n"}
        right = commit(self.parent, "right follow-up", changes)
        prs = {1: {"ref": left, "author": "left", "depends": {3}},
               2: {"ref": right, "author": "right", "depends": {3}},
               3: {"ref": self.parent, "author": "parent", "depends": set()}}
        for pr in prs.values():
            pr_overlap.scan(pr)
            self.assertIsNotNone(pr["tree"])
        return prs

    def test_compatible_followups_keep_the_shared_prerequisite(self):
        prs = self.pull_requests()
        raw = subprocess.run(["git", "merge-tree", "--write-tree", prs[1]["ref"], prs[2]["ref"]], capture_output=True)
        self.assertEqual(raw.returncode, 0)
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))

    def test_comparison_commits_keep_both_actual_common_bases(self):
        prs = self.pull_requests()
        bases = set(run("merge-base", "--all", prs[1]["tree"], prs[2]["tree"]).split())
        self.assertEqual(bases, {self.main, self.parent})
        for number in (1, 2):
            ancestry = subprocess.run(["git", "merge-base", "--is-ancestor", prs[number]["ref"], prs[number]["tree"]])
            self.assertEqual(ancestry.returncode, 0)

    def test_incompatible_followups_still_report_their_conflict(self):
        prs = self.pull_requests(conflict=True)
        found = pr_overlap.overlaps(prs, 1, 2)
        self.assertIsNotNone(found)
        self.assertIn("config.txt", found["conflicts"])

    def test_scanning_the_base_itself_remains_empty(self):
        pr = {"ref": "refs/pr/base"}
        pr_overlap.scan(pr)
        self.assertIsNotNone(pr["tree"])
        self.assertEqual(pr["files"], set())
        self.assertEqual(pr["added_exports"], {})


if __name__ == "__main__":
    unittest.main()
