import json
import subprocess
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
            patch.object(pr_overlap, "base_commit", return_value="refs/pr/base"),
            patch.object(pr_overlap, "fetch_heads"),
            patch.object(pr_overlap, "own", return_value={7: {"ref": "refs/pr/7"}}),
            patch.object(pr_overlap, "push_head", return_value=("topic", None)),
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


class MergeBaseTests(unittest.TestCase):
    def test_no_merge_base_returns_none(self):
        error = subprocess.CalledProcessError(1, ["git", "merge-base"])
        with patch.object(pr_overlap, "git", side_effect=error):
            self.assertIsNone(pr_overlap.merge_base("refs/pr/3"))


class UnrelatedHistoryTests(unittest.TestCase):
    PATCH = (
        "@@ -1 +1 @@\n"
        "-int old_value;\n"
        "+int APS5_VABI sceExample_nid_postfix(int value) { return value; }"
    )
    DIFF = (
        "diff --git a/core/libs/prx/libSceExample/Export.cpp b/core/libs/prx/libSceExample/Export.cpp\n"
        "--- a/core/libs/prx/libSceExample/Export.cpp\n"
        "+++ b/core/libs/prx/libSceExample/Export.cpp\n"
        + PATCH
    )

    def test_pull_file_data_uses_github_files_and_patches(self):
        pages = "\n".join((
            json.dumps([{
                "filename": "core/libs/prx/libSceExample/Export.cpp",
                "status": "modified",
                "patch": self.PATCH,
            }]),
            json.dumps([{
                "filename": "docs/example.md",
                "status": "added",
                "patch": "@@ -0,0 +1 @@\n+example",
            }]),
        ))
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), patch.object(
                pr_overlap, "gh", return_value=pages) as gh:
            files, added, existing, diff = pr_overlap.pull_file_data({"_number": 7})
        self.assertEqual(files, {"core/libs/prx/libSceExample/Export.cpp", "docs/example.md"})
        self.assertEqual(added, {"docs/example.md"})
        self.assertEqual(existing, {"core/libs/prx/libSceExample/Export.cpp"})
        self.assertIn(self.DIFF, diff)
        gh.assert_called_once_with("api", "--paginate", "repos/owner/project/pulls/7/files?per_page=100")

    def test_pull_file_data_expands_rename_like_no_renames_diff(self):
        response = json.dumps([{
            "filename": "core/libs/prx/libNew/Export.cpp",
            "previous_filename": "core/libs/prx/libOld/Export.cpp",
            "status": "renamed",
            "patch": self.PATCH,
        }])
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), patch.object(
                pr_overlap, "gh", return_value=response):
            files, added, existing, diff = pr_overlap.pull_file_data({"_number": 7})
        self.assertEqual(files, {
            "core/libs/prx/libOld/Export.cpp",
            "core/libs/prx/libNew/Export.cpp",
        })
        self.assertEqual(added, {"core/libs/prx/libNew/Export.cpp"})
        self.assertEqual(existing, {"core/libs/prx/libOld/Export.cpp"})
        self.assertIn(
            "diff --git a/core/libs/prx/libOld/Export.cpp b/core/libs/prx/libNew/Export.cpp",
            diff,
        )

    def test_pull_file_data_fails_closed_without_prx_cpp_patch(self):
        response = json.dumps([{
            "filename": "core/libs/prx/libSceExample/Export.cpp",
            "status": "modified",
        }])
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), patch.object(
                pr_overlap, "gh", return_value=response):
            with self.assertRaisesRegex(RuntimeError, "has no patch"):
                pr_overlap.pull_file_data({"_number": 7})

    def test_pull_file_data_fails_closed_when_renamed_prx_source_has_no_patch(self):
        response = json.dumps([{
            "filename": "docs/Export.cpp",
            "previous_filename": "core/libs/prx/libSceExample/Export.cpp",
            "status": "renamed",
        }])
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), patch.object(
                pr_overlap, "gh", return_value=response):
            with self.assertRaisesRegex(RuntimeError, "has no patch"):
                pr_overlap.pull_file_data({"_number": 7})

    def test_probe_uses_github_file_data_without_merge_base(self):
        file_data = (
            {"core/libs/prx/libSceExample/Export.cpp"},
            set(),
            {"core/libs/prx/libSceExample/Export.cpp"},
            self.DIFF,
        )
        with patch.object(pr_overlap, "merge_base", return_value=None), patch.object(
                pr_overlap, "pull_file_data", return_value=file_data):
            files, (found, added), existing = pr_overlap.probe(
                {"ref": "refs/pr/7", "_number": 7})
        self.assertEqual(files, file_data[0])
        self.assertEqual(existing, file_data[2])
        self.assertEqual(found, {"sceExample"})
        self.assertTrue(added)

    def test_scan_uses_github_file_data_when_merge_tree_refuses_history(self):
        file_data = (
            {"core/libs/prx/libSceExample/Export.cpp"},
            set(),
            {"core/libs/prx/libSceExample/Export.cpp"},
            self.DIFF,
        )
        pr = {"ref": "refs/pr/7", "_number": 7}
        with patch.object(
                pr_overlap, "merge",
                side_effect=RuntimeError("refusing to merge unrelated histories")), patch.object(
                pr_overlap, "merge_base", return_value=None), patch.object(
                pr_overlap, "pull_file_data", return_value=file_data):
            pr_overlap.scan(pr)
        self.assertIsNone(pr["tree"])
        self.assertEqual(pr["files"], file_data[0])
        self.assertEqual(pr["exports"], {"sceExample"})
        self.assertTrue(pr["added_exports"])

    def test_body_exports_fails_closed_without_merge_base(self):
        with patch.object(pr_overlap, "library_for_path", return_value="lib"), patch.object(
                pr_overlap, "merge_base", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "pull request 7.*without a merge base"):
                pr_overlap.body_exports(
                    {"ref": "refs/pr/7", "_number": 7},
                    {"core/libs/prx/libSceExample/Export.cpp"},
                    {"lib": {"sceExample": {"requested"}}},
                )


if __name__ == "__main__":
    unittest.main()
