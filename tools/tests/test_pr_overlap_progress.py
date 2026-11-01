import contextlib
import io
import json
import sys
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
        with patch.object(pr_overlap, "git", side_effect=lambda *args: calls.append(args) or ""), \
                patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch, "main", prs)
        self.assertEqual(len(calls), 3)
        self.assertEqual(lines, [
            "pr_overlap: fetched 99/250 pull request heads",
            "pr_overlap: fetched 199/250 pull request heads",
            "pr_overlap: fetched 250/250 pull request heads",
        ])

    def test_fetch_without_pull_requests_still_reports_once(self):
        with patch.object(pr_overlap, "git", return_value=""), patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch, "main", {})
        self.assertEqual(lines, ["pr_overlap: fetched 0/0 pull request heads"])


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
