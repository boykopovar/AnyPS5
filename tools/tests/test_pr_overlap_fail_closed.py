import sys
import unittest
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


class FailClosedTests(unittest.TestCase):
    def run_branch_check(self, found):
        argv = ["pr_overlap.py", "--repo", "owner/project", "--branch", "topic"]
        patches = (
            patch.object(sys, "argv", argv),
            patch.object(pr_overlap, "open_prs", return_value={7: {"ref": "refs/pr/7"}}),
            patch.object(pr_overlap, "fetch"),
            patch.object(pr_overlap, "own", return_value={7: {"ref": "refs/pr/7"}}),
            patch.object(pr_overlap, "git", return_value="a" * 40),
            patch.object(pr_overlap, "scan_all"),
            patch.object(pr_overlap, "base_exports", return_value={}),
            patch.object(pr_overlap, "main_export_findings", return_value={}),
            patch.object(pr_overlap, "overlaps", return_value=found),
            patch.object(pr_overlap, "report", return_value="review result\n"),
            patch.object(pr_overlap, "progress"),
            patch("builtins.print"),
        )
        with ExitStack() as stack:
            for item in patches:
                stack.enter_context(item)
            return pr_overlap.main()

    def test_uncompared_shared_files_fail_the_local_branch_check(self):
        result = self.run_branch_check({"added": set(), "uncompared": {"VectorAlgorithms.cpp"}})
        self.assertEqual(result, 2)

    def test_confirmed_comparisons_keep_the_local_branch_check_green(self):
        result = self.run_branch_check({"added": set(), "conflicts": {}, "exports": set()})
        self.assertEqual(result, 0)

    def test_reports_every_uncompared_path(self):
        self.assertEqual(
            pr_overlap.unverified_overlaps({
                7: {"uncompared": {"b.cpp", "a.cpp"}},
                8: {"conflicts": {"c.cpp": [(1, 2)]}},
            }),
            {7: {"a.cpp", "b.cpp"}},
        )


if __name__ == "__main__":
    unittest.main()
