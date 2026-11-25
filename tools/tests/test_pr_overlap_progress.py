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
    return {"number": number, "head": {"sha": f"{number:040x}", "ref": f"branch{number}"}, "user": {"login": "someone"}, "body": ""}


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

    def fake_remote(self, missing, pulls, fetched):
        def git(*args):
            if args[0] == "ls-remote":
                ref = args[-1]
                if ref == "refs/heads/main":
                    return "f" * 40 + "\trefs/heads/main\n"
                return pulls[ref] + "\t" + ref + "\n"
            shas = args[5:]
            if missing & set(shas):
                raise pr_overlap.subprocess.CalledProcessError(128, ["git", *args], stderr="did not send all necessary objects")
            fetched.extend(shas)
            return ""
        return git

    def test_fetch_compares_the_new_head_of_a_pull_request_pushed_during_the_run(self):
        prs = {n: {"ref": f"refs/pr/{n}", "sha": f"{n:040x}"} for n in range(1, 4)}
        moved = "e" * 40
        fetched = []
        git = self.fake_remote({f"{2:040x}"}, {"refs/pull/2/head": moved}, fetched)
        with patch.object(pr_overlap, "git", side_effect=git), patch.object(pr_overlap, "REPO", "owner/project", create=True):
            _, lines = self.capture(pr_overlap.fetch, "main", prs)
        self.assertEqual(prs[2]["sha"], moved)
        self.assertEqual(prs[2]["ref"], moved)
        self.assertEqual([prs[n]["ref"] for n in (1, 3)], [f"{1:040x}", f"{3:040x}"])
        self.assertEqual(set(fetched), {"f" * 40, f"{1:040x}", f"{3:040x}", moved})
        self.assertEqual(lines, [
            f"pr_overlap: pull request #2 moved from {f'{2:040x}'[:10]} to {moved[:10]} during this run; comparing {moved[:10]}",
            "pr_overlap: fetched 3/3 pull request heads",
        ])

    def test_fetch_fails_when_an_unmoved_head_cannot_be_fetched(self):
        prs = {n: {"ref": f"refs/pr/{n}", "sha": f"{n:040x}"} for n in range(1, 4)}
        git = self.fake_remote({f"{2:040x}"}, {"refs/pull/2/head": f"{2:040x}"}, [])
        with patch.object(pr_overlap, "git", side_effect=git), patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                self.assertRaisesRegex(ValueError, f"cannot fetch the head {2:040x} of pull request #2"):
            self.capture(pr_overlap.fetch, "main", prs)

    def test_fetch_still_fails_when_the_base_cannot_be_fetched(self):
        prs = {1: {"ref": "refs/pr/1", "sha": f"{1:040x}"}}
        git = self.fake_remote({"f" * 40}, {}, [])
        with patch.object(pr_overlap, "git", side_effect=git), patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                self.assertRaises(pr_overlap.subprocess.CalledProcessError):
            self.capture(pr_overlap.fetch, "main", prs)


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
