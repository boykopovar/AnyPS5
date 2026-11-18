import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


def run(*args):
    return subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args],
                          capture_output=True, text=True, check=True).stdout.strip()


class OwnBranchTests(unittest.TestCase):
    def test_drops_pull_requests_already_contained_in_the_branch(self):
        old = os.getcwd()
        with tempfile.TemporaryDirectory() as tmp:
            os.chdir(tmp)
            try:
                run("init", "-q")
                run("commit", "-q", "--allow-empty", "-m", "a")
                a = run("rev-parse", "HEAD")
                run("commit", "-q", "--allow-empty", "-m", "b")
                b = run("rev-parse", "HEAD")
                run("checkout", "-q", a)
                run("commit", "-q", "--allow-empty", "-m", "c")
                c = run("rev-parse", "HEAD")
                prs = {7: {"sha": a}, 8: {"sha": b}, 9: {"sha": c}}
                self.assertEqual(set(pr_overlap.own(prs, b)), {9})
            finally:
                os.chdir(old)

    def test_scans_all_pull_requests_with_a_bounded_worker_pool(self):
        prs = {number: {"sha": str(number)} for number in range(1, 9)}
        scanned = []

        def scan(pr):
            scanned.append(pr["sha"])

        with patch.object(pr_overlap, "scan", side_effect=scan):
            pr_overlap.scan_all(prs, 3)

        self.assertCountEqual(scanned, [str(number) for number in prs])


class BranchResolutionTests(unittest.TestCase):
    def setUp(self):
        self.old = os.getcwd()
        self.directory = tempfile.TemporaryDirectory()
        os.chdir(self.directory.name)
        run("init", "-q", "-b", "main")
        run("commit", "-q", "--allow-empty", "-m", "a")

    def tearDown(self):
        os.chdir(self.old)
        self.directory.cleanup()

    def test_a_mistyped_branch_is_rejected_before_any_pull_request_head_is_fetched(self):
        def unreachable(*args, **kwargs):
            raise AssertionError("a mistyped --branch must not fetch pull request heads")

        with patch.object(pr_overlap, "open_prs", return_value={}), \
                patch.object(pr_overlap, "base_commit", return_value="a" * 40), \
                patch.object(pr_overlap, "fetch_heads", side_effect=unreachable), \
                patch.object(pr_overlap, "upstream", return_value="boykopovar/AnyPS5"):
            with self.assertRaises(SystemExit) as raised:
                pr_overlap.main(["--branch", "no-such-branch-in-this-repository"])

        self.assertIn("no-such-branch-in-this-repository", str(raised.exception))

    def test_resolves_the_head_commit_of_a_local_branch(self):
        run("branch", "contrib/topic")
        self.assertEqual(pr_overlap.resolve_branch("contrib/topic"), run("rev-parse", "HEAD"))

    def test_refuses_a_name_that_matches_no_commit(self):
        with self.assertRaises(ValueError) as raised:
            pr_overlap.resolve_branch("contrib/absent")
        self.assertIn("contrib/absent", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
