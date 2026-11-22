import io
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

    def test_drops_the_open_pull_request_whose_head_branch_is_the_probed_branch(self):
        prs = {5: {"branch": "contrib/topic", "head_repo": "mine/project"},
               6: {"branch": "contrib/other", "head_repo": "mine/project"}}
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", "mine/project")), {6})

    def test_keeps_every_pull_request_when_two_of_them_share_the_probed_branch_name(self):
        prs = {5: {"branch": "contrib/topic", "head_repo": "mine/project"},
               6: {"branch": "contrib/topic", "head_repo": "mine/project"}}
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", "mine/project")), {5, 6})

    def test_keeps_a_pull_request_record_without_a_head_branch(self):
        prs = {5: {"sha": "a" * 40}}
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", "mine/project")), {5})

    def test_keeps_the_only_same_named_branch_if_it_is_on_another_fork(self):
        prs = {5: {"sha": "a" * 40, "branch": "contrib/topic", "head_repo": "other/project"}}
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", "mine/project")), {5})

    def test_drops_only_the_matching_repository_and_branch(self):
        prs = {
            5: {"branch": "contrib/topic", "head_repo": "Mine/Project"},
            6: {"branch": "contrib/topic", "head_repo": "other/project"},
        }
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", "mine/project")), {6})

    def test_keeps_same_named_pull_request_without_a_known_push_repository(self):
        prs = {5: {"branch": "contrib/topic", "head_repo": "other/project"}}
        self.assertEqual(set(pr_overlap.without_own_pull_request(prs, "contrib/topic", None)), {5})

    def test_the_branch_check_does_not_compare_the_branchs_own_pull_request(self):
        seen = {}
        with patch.object(sys, "argv", ["pr_overlap.py", "--repo", "owner/project", "--branch", "contrib/topic"]), \
                patch.object(pr_overlap, "open_prs", return_value={
                    7: {"branch": "published/topic", "head_repo": "mine/project"},
                    8: {"branch": "published/topic", "head_repo": "other/project"},
                }), \
                patch.object(pr_overlap, "push_head", return_value=("published/topic", "mine/project")), \
                patch.object(pr_overlap, "base_commit", return_value="refs/pr/base"), \
                patch.object(pr_overlap, "fetch_heads"), \
                patch.object(pr_overlap, "own", side_effect=lambda prs, sha: prs), \
                patch.object(pr_overlap, "git", return_value="a" * 40), \
                patch.object(pr_overlap, "scan_all", side_effect=lambda prs, jobs: seen.update(prs)), \
                patch.object(pr_overlap, "base_exports", return_value={}), \
                patch.object(pr_overlap, "main_export_findings", return_value={}), \
                patch.object(pr_overlap, "overlaps", return_value=None), \
                patch.object(pr_overlap, "report", return_value=""), \
                patch.object(pr_overlap, "progress"), \
                patch("builtins.print"):
            self.assertEqual(pr_overlap.main(), 0)
        self.assertEqual(set(seen), {0, 8})

    def test_explicit_head_repo_ignores_only_same_fork_and_branch_after_rebase(self):
        seen = {}
        prs = {7: {"branch": "contrib/topic", "head_repo": "mine/project"},
               8: {"branch": "contrib/topic", "head_repo": "other/project"},
               9: {"branch": "contrib/other", "head_repo": "mine/project"}}
        with patch.object(pr_overlap, "open_prs", return_value=prs), \
                patch.object(pr_overlap, "resolve_branch", return_value="a" * 40), \
                patch.object(pr_overlap, "push_head", side_effect=AssertionError("explicit identity must not be inferred")), \
                patch.object(pr_overlap, "base_commit", return_value="refs/pr/base"), \
                patch.object(pr_overlap, "fetch_heads"), \
                patch.object(pr_overlap, "own", side_effect=lambda rows, sha: rows), \
                patch.object(pr_overlap, "scan_all", side_effect=lambda rows, jobs: seen.update(rows)), \
                patch.object(pr_overlap, "base_exports", return_value={}), \
                patch.object(pr_overlap, "main_export_findings", return_value={}), \
                patch.object(pr_overlap, "overlaps", return_value=None), \
                patch.object(pr_overlap, "report", return_value=""), \
                patch.object(pr_overlap, "progress"), \
                patch("builtins.print"):
            result = pr_overlap.main(["--repo", "owner/project", "--branch", "contrib/topic", "--head-repo", "Mine/Project"])
        self.assertEqual(result, 0)
        self.assertEqual(set(seen), {0, 8, 9})

    def test_explicit_head_repo_requires_branch_and_owner_repo(self):
        for args in (["--names", "a", "--head-repo", "mine/project"],
                     ["--out", "unused", "--head-repo", "mine/project"],
                     ["--branch", "topic", "--head-repo", ""],
                     ["--branch", "topic", "--head-repo", "not-a-repo"],
                     ["--branch", "topic", "--head-repo", "mine/project/extra"]):
            with self.subTest(args=args), patch.object(pr_overlap, "open_prs") as network, \
                    patch.object(sys, "stderr", new_callable=io.StringIO):
                with self.assertRaises(SystemExit) as error:
                    pr_overlap.main(args)
                self.assertEqual(error.exception.code, 2)
                network.assert_not_called()

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

    def test_uses_the_configured_push_repository_and_destination_branch(self):
        run("remote", "add", "fork", "https://github.com/reader/project.git")
        run("remote", "set-url", "--push", "fork", "git@github.com:writer/project.git")
        run("config", "branch.main.pushRemote", "fork")
        run("config", "remote.fork.push", "refs/heads/main:refs/heads/published/topic")
        self.assertEqual(pr_overlap.push_head("main"), ("published/topic", "writer/project"))

    def test_leaves_an_unconfigured_branch_without_a_push_identity(self):
        self.assertEqual(pr_overlap.push_head("main"), ("main", None))

    def test_rejects_malformed_push_identity_rows(self):
        for row in ("refs/heads/main\tfork", "refs/heads/main\tfork\trefs/heads/topic\textra"):
            with self.subTest(row=row), patch.object(pr_overlap, "git", return_value=row) as git:
                self.assertEqual(pr_overlap.push_head("main"), ("main", None))
                self.assertEqual(git.call_count, 1)

    def test_refuses_a_name_that_matches_no_commit(self):
        with self.assertRaises(ValueError) as raised:
            pr_overlap.resolve_branch("contrib/absent")
        self.assertIn("contrib/absent", str(raised.exception))


if __name__ == "__main__":
    unittest.main()
