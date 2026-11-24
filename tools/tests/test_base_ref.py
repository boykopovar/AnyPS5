import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import base_ref


class BaseRefTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.repo = Path(directory.name)
        self.git("init", "-q", "-b", "fixture")

    def git(self, *arguments):
        environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1"}
        result = subprocess.run(["git", "-C", str(self.repo), "-c", "user.name=Base Ref Test",
                                 "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *arguments],
                                capture_output=True, text=True, check=True, env=environment)
        return result.stdout.strip()

    def commit(self, subject):
        self.git("commit", "-q", "--allow-empty", "-m", subject)
        return self.git("rev-parse", "HEAD")

    def point(self, ref, commit=None):
        if commit is None:
            self.git("update-ref", "-d", ref)
        else:
            self.git("update-ref", ref, commit)

    def test_upstream_main_is_preferred_over_a_stale_origin_main(self):
        stale = self.commit("first")
        current = self.commit("second")
        self.point("refs/remotes/origin/main", stale)
        self.point("refs/remotes/upstream/main", current)
        self.point("refs/heads/main", current)
        self.assertEqual(base_ref.resolve(self.repo), "refs/remotes/upstream/main")

    def test_origin_main_is_used_without_an_upstream_remote(self):
        commit = self.commit("first")
        self.point("refs/remotes/origin/main", commit)
        self.point("refs/heads/main", commit)
        self.assertEqual(base_ref.resolve(self.repo), "refs/remotes/origin/main")

    def test_local_main_is_the_last_resort(self):
        commit = self.commit("first")
        self.point("refs/heads/main", commit)
        self.assertEqual(base_ref.resolve(self.repo), "refs/heads/main")

    def test_tag_cannot_shadow_remote_branch(self):
        stale = self.commit("stale tag")
        current = self.commit("current branch")
        self.point("refs/tags/origin/main", stale)
        self.point("refs/remotes/origin/main", current)
        base = base_ref.resolve(self.repo)
        self.assertEqual(self.git("rev-parse", base), current)

    def test_an_explicit_base_is_returned_unchecked(self):
        self.commit("first")
        self.assertEqual(base_ref.resolve(self.repo, "release/1.0"), "release/1.0")

    def test_fatal_git_error_is_distinct_from_missing_refs(self):
        outside = self.repo / "outside"
        outside.mkdir()
        (outside / ".git").write_text("invalid gitdir\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Git could not resolve the base branch:.*fatal:"):
            base_ref.resolve(outside)

    def test_no_base_branch_names_every_candidate(self):
        self.commit("first")
        with self.assertRaises(ValueError) as caught:
            base_ref.resolve(self.repo)
        for candidate in base_ref.CANDIDATES:
            self.assertIn(candidate, str(caught.exception))


if __name__ == "__main__":
    unittest.main()
