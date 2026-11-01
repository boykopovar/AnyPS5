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


if __name__ == "__main__":
    unittest.main()
