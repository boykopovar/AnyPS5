import contextlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def pull(number):
    return {"number": number, "head": {"sha": f"{number:040x}"}, "user": {"login": "someone"}, "body": ""}


class ProgressTests(unittest.TestCase):
    def capture(self, function, *args):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            result = function(*args)
        return result, stderr.getvalue().splitlines()

    def test_listing_reports_the_number_of_open_pull_requests(self):
        pages = json.dumps([pull(n) for n in range(1, 101)]) + "\n" + json.dumps([pull(n) for n in range(101, 151)])
        with patch.object(pr_overlap, "gh", return_value=pages), patch.object(pr_overlap, "REPO", "owner/project", create=True):
            prs, lines = self.capture(pr_overlap.open_prs, "main")
        self.assertEqual(len(prs), 150)
        self.assertEqual(lines, ["pr_overlap: 150 open pull requests against main"])

    def test_fetch_reports_the_heads_fetched_after_each_batch(self):
        prs = {n: {"ref": f"refs/pr/{n}", "sha": f"{n:040x}"} for n in range(1, 251)}
        calls = []
        with patch.object(pr_overlap, "git", side_effect=lambda *args: calls.append(args) or (
                "f" * 40 + "\trefs/heads/main\n" if args[0] == "ls-remote" else "")), \
                patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch, "main", prs)
        self.assertEqual(len(calls), 4)
        self.assertEqual(lines, [
            "pr_overlap: fetched 99/250 pull request heads",
            "pr_overlap: fetched 199/250 pull request heads",
            "pr_overlap: fetched 250/250 pull request heads",
        ])

    def test_fetch_without_pull_requests_still_reports_once(self):
        with patch.object(pr_overlap, "git", side_effect=lambda *args: "f" * 40 + "\trefs/heads/main\n"
                if args[0] == "ls-remote" else ""), patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch, "main", {})
        self.assertEqual(lines, ["pr_overlap: fetched 0/0 pull request heads"])

    def test_fetch_with_existing_check_fetches_only_missing_commits(self):
        prs = {n: {"ref": f"refs/pr/{n}", "sha": f"{n:040x}"} for n in range(1, 151)}
        present = {f"{n:040x}" for n in range(1, 151) if n % 3}
        calls = []
        with patch.object(pr_overlap, "missing_commits", side_effect=lambda commits: [c for c in commits if c not in present]), \
                patch.object(pr_overlap, "git", side_effect=lambda *args: calls.append(args) or ""), \
                patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch_heads, "f" * 40, prs, True)
        fetched = [commit for call in calls for commit in call[5:]]
        self.assertEqual(fetched, ["f" * 40] + [f"{n:040x}" for n in range(1, 151) if n % 3 == 0])
        self.assertEqual(lines, ["pr_overlap: fetched 51/51 missing commits"])
        self.assertTrue(all(pr["ref"] == pr["sha"] for pr in prs.values()))

    def test_fetch_with_existing_check_skips_fetch_when_nothing_is_missing(self):
        prs = {1: {"ref": "refs/pr/1", "sha": "1" * 40}}
        calls = []
        with patch.object(pr_overlap, "missing_commits", return_value=[]), \
                patch.object(pr_overlap, "git", side_effect=lambda *args: calls.append(args) or ""), \
                patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch_heads, "f" * 40, prs, True)
        self.assertEqual(calls, [])
        self.assertEqual(lines, [])

    def test_missing_commits_reports_only_commits_absent_from_the_repository(self):
        with tempfile.TemporaryDirectory() as directory:
            environment = dict(os.environ, GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
            subprocess.run(["git", "init", "-q", directory], check=True, env=environment)
            subprocess.run(["git", "-C", directory, "commit", "-q", "--allow-empty", "-m", "x"], check=True, env=environment)
            head = subprocess.run(["git", "-C", directory, "rev-parse", "HEAD"], check=True, capture_output=True, text=True).stdout.strip()
            tree = subprocess.run(["git", "-C", directory, "rev-parse", "HEAD^{tree}"], check=True, capture_output=True, text=True).stdout.strip()
            absent = "0123456789abcdef" * 2 + "01234567"
            with patch.object(pr_overlap, "_repository_root", return_value=Path(directory)):
                self.assertEqual(pr_overlap.missing_commits([head, absent, tree]), [absent, tree])

    def test_missing_commits_fetches_everything_when_git_fails(self):
        with patch.object(pr_overlap.subprocess, "run", side_effect=OSError("no git")), \
                patch.object(pr_overlap, "_repository_root", return_value=Path(".")):
            self.assertEqual(pr_overlap.missing_commits(["a" * 40, "b" * 40]), ["a" * 40, "b" * 40])


    def fake_scan(self, pr):
        pr.update(files=set(pr.get("files_on_scan", ())), added=set(), exports=set(), tree="tree")

    def test_branch_run_reports_probing_and_merging(self):
        prs = {0: {"ref": "branch", "files_on_scan": {"shared.txt"}}}
        prs.update({n: {"ref": f"refs/pr/{n}"} for n in range(1, 251)})

        def fake_probe(pr):
            number = int(pr["ref"].rsplit("/", 1)[1])
            return ({"shared.txt"} if number % 25 == 0 else {"other.txt"}), (set(), {}), set()

        with patch.object(pr_overlap, "scan", side_effect=self.fake_scan), patch.object(pr_overlap, "probe", side_effect=fake_probe), \
                patch.object(pr_overlap, "git", return_value="shared.txt\0other.txt\0"):
            _, lines = self.capture(pr_overlap.scan_all, prs, 2)
        self.assertEqual(lines, [
            "pr_overlap: probed 100/250 pull requests, 4 share files or exports with the branch",
            "pr_overlap: probed 200/250 pull requests, 8 share files or exports with the branch",
            "pr_overlap: probed 250/250 pull requests, 10 share files or exports with the branch",
            "pr_overlap: merged 10/10 pull requests into the base",
        ])

    def test_full_run_reports_every_hundred_merges(self):
        prs = {n: {"ref": f"refs/pr/{n}"} for n in range(1, 151)}
        with patch.object(pr_overlap, "scan", side_effect=self.fake_scan):
            _, lines = self.capture(pr_overlap.scan_all, prs, 2)
        self.assertEqual(lines, [
            "pr_overlap: merged 100/150 pull requests into the base",
            "pr_overlap: merged 150/150 pull requests into the base",
        ])

if __name__ == "__main__":
    unittest.main()
