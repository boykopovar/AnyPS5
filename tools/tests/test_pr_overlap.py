import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.parse import unquote, urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap


class OpenPullRequestsTests(unittest.TestCase):
    def test_collects_pull_requests_beyond_the_previous_limit(self):
        pull_requests = [
            {"number": number, "user": {"login": f"author{number}"},
             "head": {"sha": f"sha{number}"}, "body": None}
            for number in range(1, 202)
        ]
        pull_requests[-1]["body"] = "Depends on: #12\n- #34\n"

        def response(*args):
            if args[0] == "api":
                return json.dumps(pull_requests)
            limit = int(args[args.index("-L") + 1])
            return json.dumps([
                {"number": pr["number"], "author": pr["user"], "headRefOid": pr["head"]["sha"],
                 "body": pr["body"]} for pr in pull_requests[:limit]
            ])

        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", side_effect=response):
            found = pr_overlap.open_prs("main")
        self.assertEqual(set(found), set(range(1, 202)))
        self.assertEqual(found[201], {
            "ref": "refs/pr/201", "sha": "sha201", "author": "author201", "depends": {12, 34},
        })
        self.assertEqual(found[1]["depends"], set())

    def test_base_branch_is_encoded_in_the_paginated_request(self):
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", return_value="[]") as gh:
            self.assertEqual(pr_overlap.open_prs("topic/one&two"), {})
        gh.assert_called_once_with(
            "api", "--include",
            "repos/owner/project/pulls?state=open&base=topic%2Fone%26two&per_page=100&page=1",
        )

    def test_single_page_preserves_author_head_and_dependencies(self):
        pull_requests = [{"number": 7, "user": {"login": "contributor"}, "head": {"sha": "head-sha"},
                          "body": "Depends on: none\nUnrelated issue #42"}]
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", return_value=json.dumps(pull_requests)):
            found = pr_overlap.open_prs("main")
        self.assertEqual(found, {
            7: {"ref": "refs/pr/7", "sha": "head-sha", "author": "contributor", "depends": set()},
        })

    def test_collects_concatenated_json_pages_from_paginated_gh_output(self):
        pull_requests = [
            {"number": number, "user": {"login": f"author{number}"},
             "head": {"sha": f"sha{number}"}, "body": None}
            for number in range(1, 4)
        ]
        pages = [pull_requests[:2], pull_requests[2:]]
        output = "\n".join(json.dumps(page) for page in pages)

        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", return_value=output):
            found = pr_overlap.open_prs("main")

        self.assertEqual(set(found), {1, 2, 3})
        self.assertEqual(found[3]["sha"], "sha3")

    def test_rejects_empty_paginated_output(self):
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", return_value="  \n"):
            with self.assertRaisesRegex(ValueError, "returned no JSON pages"):
                pr_overlap.open_prs("main")


class TopInsertTests(unittest.TestCase):
    def test_top_of_file_insertions_clash_on_line_one(self):
        pr = {"tree": "t", "files": {"f.cpp"}, "added": set()}
        with patch.object(pr_overlap, "git", return_value="@@ -0,0 +1 @@\n+x\n"):
            self.assertEqual(pr_overlap.hunks(pr, "f.cpp"), [(1, 1)])
            self.assertEqual(pr_overlap.clashes(pr, pr, "f.cpp"), [(1, 1)])

    def test_top_of_file_span_links_to_line_one(self):
        self.assertEqual(pr_overlap.spans({"f.cpp": [(1, 1)]}, "https://x/blob/sha"),
                         "`f.cpp` [L1](https://x/blob/sha/f.cpp#L1)")

    def test_ordinary_hunk_is_unchanged(self):
        pr = {"tree": "t"}
        with patch.object(pr_overlap, "git", return_value="@@ -3,2 +3,2 @@\n-a\n-b\n+c\n+d\n"):
            self.assertEqual(pr_overlap.hunks(pr, "f.cpp"), [(3, 4)])


class FetchTests(unittest.TestCase):
    def test_fetches_each_head_commit_the_api_reported(self):
        prs = {
            7: {"ref": "refs/pr/7", "sha": "a" * 40, "author": "one", "depends": set()},
            9: {"ref": "refs/pr/9", "sha": "b" * 40, "author": "two", "depends": set()},
        }
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "git", return_value="") as git:
            pr_overlap.fetch("main", prs)
        git.assert_called_once_with(
            "fetch", "--no-tags", "-q", "https://github.com/owner/project.git",
            "+refs/heads/main:refs/pr/base", f"+{'a' * 40}:refs/pr/7", f"+{'b' * 40}:refs/pr/9",
        )

    def test_batches_large_sets_of_pull_request_heads(self):
        prs = {
            number: {
                "ref": f"refs/pr/{number}", "sha": f"{number:040x}",
                "author": "contributor", "depends": set(),
            }
            for number in range(250)
        }
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "git", return_value="") as git:
            pr_overlap.fetch("main", prs)
        self.assertEqual(git.call_count, 3)
        self.assertEqual([len(call.args) - 4 for call in git.call_args_list], [100, 100, 51])
        self.assertEqual(git.call_args_list[0].args[4], "+refs/heads/main:refs/pr/base")
        self.assertEqual(git.call_args_list[-1].args[-1], "+00000000000000000000000000000000000000f9:refs/pr/249")


class ConflictTests(unittest.TestCase):
    def test_uses_the_shared_base_parent_without_newer_merge_tree_option(self):
        first = {"tree": "first-tree", "files": {"shared.cpp"}, "added": set()}
        second = {"tree": "second-tree", "files": {"shared.cpp"}, "added": set()}
        with patch.object(pr_overlap, "merge", return_value=("merged-tree", set())) as merge:
            self.assertEqual(pr_overlap.conflicts(first, second), {})
        merge.assert_called_once_with("first-tree", "second-tree")

    def real_pull_requests(self, main_changes, older_changes, current_changes):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name).resolve()
        self.assertTrue(root.is_relative_to(Path(tempfile.gettempdir()).resolve()))
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(root)

        def git(*args):
            return subprocess.run(
                ["git", "-c", "user.name=Overlap Test", "-c", "user.email=test@example.com", *args],
                capture_output=True, text=True, encoding="utf-8", check=True).stdout.strip()

        git("init", "-q")
        git("config", "commit.gpgsign", "false")
        for name in ("shared.cpp", "other.cpp", "unrelated.cpp"):
            (root / name).write_text("int value = 0;\n", encoding="utf-8")
        git("add", ".")
        git("commit", "-qm", "test: create base")
        base = git("rev-parse", "HEAD")

        def branch(name, parent, changes):
            git("checkout", "-qb", name, parent)
            for path, value in changes.items():
                (root / path).write_text(f"int value = {value};\n", encoding="utf-8")
            git("add", ".")
            git("commit", "-qm", "test: change values", "--allow-empty")
            return git("rev-parse", "HEAD")

        older = branch("older", base, older_changes)
        main = branch("updated-base", base, main_changes)
        current = branch("current", main, current_changes)
        git("update-ref", "refs/pr/base", main)
        prs = {1: {"ref": current, "author": "current-author", "depends": set()},
               2: {"ref": older, "author": "older-author", "depends": set()}}
        for pr in prs.values():
            pr_overlap.scan(pr)
        return prs

    def test_reports_shared_files_when_older_pr_conflicts_with_main(self):
        prs = self.real_pull_requests({"shared.cpp": 2}, {"shared.cpp": 1}, {"shared.cpp": 3})
        self.assertIsNone(prs[2]["tree"])
        self.assertIsNotNone(prs[1]["tree"])
        found = pr_overlap.overlaps(prs, 1, 2)
        self.assertIsNotNone(found)
        self.assertEqual(found["uncompared"], {"shared.cpp"})
        report = pr_overlap.report({2: found}, "https://example.invalid/blob/base")
        self.assertIn("could not be completed", report)
        self.assertIn("`shared.cpp`", report)
        self.assertNotIn("Merging one will break the other", report)

    def test_conflict_in_another_file_does_not_hide_shared_changes(self):
        prs = self.real_pull_requests({"other.cpp": 2}, {"other.cpp": 1, "shared.cpp": 1}, {"shared.cpp": 3})
        self.assertIsNone(prs[2]["tree"])
        found = pr_overlap.overlaps(prs, 1, 2)
        self.assertIsNotNone(found)
        self.assertEqual(found["uncompared"], {"shared.cpp"})

    def test_unmergeable_pr_with_no_shared_files_is_not_reported(self):
        prs = self.real_pull_requests({"other.cpp": 2}, {"other.cpp": 1}, {"unrelated.cpp": 3})
        self.assertIsNone(prs[2]["tree"])
        self.assertIsNone(pr_overlap.overlaps(prs, 1, 2))

    def test_mergeable_branches_still_report_confirmed_line_conflicts(self):
        prs = self.real_pull_requests({}, {"shared.cpp": 1}, {"shared.cpp": 2})
        self.assertTrue(all(pr["tree"] for pr in prs.values()))
        found = pr_overlap.overlaps(prs, 1, 2)
        self.assertEqual(found["conflicts"], {"shared.cpp": [(1, 1)]})
        self.assertNotIn("uncompared", found)

    def test_mixed_report_keeps_confirmed_and_uncompared_sections(self):
        rows = {2: {"added": set(), "conflicts": {"other.cpp": [(1, 1)]}},
                3: {"added": set(), "uncompared": {"shared.cpp"}}}
        report = pr_overlap.report(rows, "https://example.invalid/blob/base")
        self.assertIn("[L1]", report)
        self.assertIn("could not be completed", report)
        self.assertIn("`shared.cpp`", report)
        self.assertTrue(report.startswith(pr_overlap.MARKER))

    def test_report_links_pull_requests_without_cross_references(self):
        rows = {2: {"added": set(), "conflicts": {"other.cpp": [(1, 1)]}},
                3: {"added": set(), "uncompared": {"shared.cpp"}}}
        report = pr_overlap.report(rows, "https://github.com/owner/project/blob/base")
        self.assertIsNone(re.search(r"#\d|https://github\.com/owner/project/(pull|issues)/", report))
        self.assertIn("[PR 2](https://www.github.com/owner/project/pull/2)", report)
        self.assertIn("[PR 3](https://www.github.com/owner/project/pull/3)", report)


class UnicodePathTests(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "POSIX filenames containing control characters")
    def test_control_characters_keep_exact_paths_in_scans_and_conflicts(self):
        previous = os.getcwd()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Overlap Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args], cwd=root,
                    capture_output=True, text=True, encoding="utf-8", check=True).stdout.strip()
            filenames = {"core/libs/prx/libExample/return\r.cpp", "core/libs/prx/libExample/tab\t.cpp", "core/libs/prx/libExample/line\n.cpp",
                         "core/libs/prx/libExample/quote\".cpp"}
            for filename in filenames:
                source = root / filename
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_text("int value = 0;\n", encoding="utf-8")
            git("init", "-q")
            git("add", ".")
            git("commit", "-qm", "test: create base")
            base = git("rev-parse", "HEAD")
            git("update-ref", "refs/pr/base", base)
            for branch, value in (("left", 1), ("right", 2)):
                git("checkout", "-qb", branch, base)
                for filename in filenames:
                    (root / filename).write_text(f"int value = {value};\n", encoding="utf-8")
                git("add", ".")
                git("commit", "-qm", "fix: change value")
            try:
                os.chdir(root)
                left, right = {"ref": "left"}, {"ref": "right"}
                pr_overlap.scan(left)
                pr_overlap.scan(right)
                self.assertEqual(left["files"], filenames)
                self.assertEqual(right["files"], filenames)
                self.assertEqual(pr_overlap.conflicts(left, right), {name: [(1, 1)] for name in filenames})
            finally:
                os.chdir(previous)


    def test_scans_and_reports_conflicts_with_git_path_quoting_enabled(self):
        previous = os.getcwd()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Overlap Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args], cwd=root,
                    capture_output=True, text=True, encoding="utf-8", check=True).stdout.strip()

            filename = "core/libs/prx/libSceExample/caf\u00e9 #1 (%20).cpp"
            source = root / filename
            source.parent.mkdir(parents=True)
            source.write_text("int value = 0;\n", encoding="utf-8")
            git("init", "-q")
            git("config", "core.quotePath", "true")
            git("add", ".")
            git("commit", "-qm", "test: create base")
            base = git("rev-parse", "HEAD")
            git("update-ref", "refs/pr/base", base)
            for branch, value in (("left", 1), ("right", 2)):
                git("checkout", "-qb", branch, base)
                source.write_text(f"int value = {value};\n", encoding="utf-8")
                git("add", ".")
                git("commit", "-qm", "fix: change value")
            try:
                os.chdir(root)
                left, right = {"ref": "left"}, {"ref": "right"}
                pr_overlap.scan(left)
                pr_overlap.scan(right)
                self.assertEqual(left["files"], {filename})
                self.assertEqual(right["files"], {filename})
                self.assertEqual(pr_overlap.conflicts(left, right), {filename: [(1, 1)]})
                blob = f"https://github.com/owner/project/blob/{base}"
                message = pr_overlap.report({7: {"conflicts": pr_overlap.conflicts(left, right), "added": set()}}, blob)
                link = message.split("[L1](", 1)[1].split(")", 1)[0]
                parsed = urlsplit(link)
                self.assertEqual(unquote(parsed.path), f"/owner/project/blob/{base}/{filename}")
                self.assertEqual(parsed.fragment, "L1")
                self.assertEqual(parsed.query, "")
            finally:
                os.chdir(previous)


class SubdirectoryTests(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "fake gh is a shell script")
    def test_branch_report_from_a_subdirectory_matches_the_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve() / "repo"
            fake = Path(directory) / "bin"
            fake.mkdir()

            def git(*args):
                return subprocess.run(
                    ["git", "-c", "user.name=Overlap Test", "-c", "user.email=test@example.com",
                     "-c", "commit.gpgsign=false", *args], cwd=root,
                    capture_output=True, text=True, encoding="utf-8", check=True).stdout.strip()

            filename = "core/libs/prx/libSceExample/Export.cpp"
            source = root / filename
            source.parent.mkdir(parents=True)
            source.write_text("int value = 0;\n", encoding="utf-8")
            (root / "tools").mkdir()
            (root / "tools" / "tool.py").write_text("print()\n", encoding="utf-8")
            git("init", "-q", "-b", "main")
            git("add", ".")
            git("commit", "-qm", "test: create base")
            heads = {}
            for branch, value in (("theirs", 1), ("mine", 2)):
                git("checkout", "-qb", branch, "main")
                source.write_text(f"int value = {value};\nint APS5_VABI sceExample() {{ return {value}; }}\n", encoding="utf-8")
                git("add", ".")
                git("commit", "-qm", "fix: change value")
                heads[branch] = git("rev-parse", "HEAD")
            pulls = [{"number": 7, "user": {"login": "someone"}, "head": {"sha": heads["theirs"]}, "body": "Depends on: none"}]
            (fake / "pulls.json").write_text(json.dumps(pulls), encoding="utf-8")
            (fake / "gh").write_text(f"#!/bin/sh\ncat '{fake / 'pulls.json'}'\n", encoding="utf-8")
            (fake / "gh").chmod(0o755)
            environment = {**os.environ, "PATH": f"{fake}{os.pathsep}{os.environ['PATH']}", "GIT_CONFIG_COUNT": "1",
                           "GIT_CONFIG_KEY_0": f"url.{root.as_uri()}.insteadOf", "GIT_CONFIG_VALUE_0": "https://github.com/owner/project.git"}
            environment.pop("GITHUB_REPOSITORY", None)
            script = Path(pr_overlap.__file__).resolve()
            outputs = {}
            for subdirectory in (".", "tools", "core/libs/prx"):
                result = subprocess.run([sys.executable, str(script), "--repo", "owner/project", "--branch", "mine"],
                                        cwd=root / subdirectory, env=environment, capture_output=True, text=True,
                                        encoding="utf-8", timeout=60)
                self.assertEqual(result.returncode, 0, result.stderr)
                outputs[subdirectory] = result.stdout
            self.assertIn(f"`{filename}` [L1](", outputs["."])
            self.assertIn("| `sceExample` |", outputs["."])
            self.assertEqual(outputs["tools"], outputs["."])
            self.assertEqual(outputs["core/libs/prx"], outputs["."])
            result = subprocess.run([sys.executable, str(script), "--repo", "owner/project", "--out", "reports"],
                                    cwd=root / "tools", env=environment, capture_output=True, text=True,
                                    encoding="utf-8", timeout=60)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((root / "tools" / "reports" / "7.md").is_file())


class MarkdownCellTests(unittest.TestCase):
    def test_regular_filename_is_unchanged(self):
        self.assertEqual(
            pr_overlap.cell({"normal.cpp"}),
            "`normal.cpp`",
        )

    def test_single_backtick_in_filename(self):
        self.assertEqual(
            pr_overlap.cell({"example`file.cpp"}),
            "`` example`file.cpp ``",
        )

    def test_multiple_backticks_in_filename(self):
        self.assertEqual(
            pr_overlap.cell({"example``file.cpp"}),
            "``` example``file.cpp ```",
        )

    def test_multiple_filenames_are_sorted(self):
        self.assertEqual(
            pr_overlap.cell({"normal.cpp", "example`file.cpp"}),
            "`` example`file.cpp ``<br>`normal.cpp`",
        )


    def test_conflict_spans_with_backticks(self):
        result = pr_overlap.spans(
            {"example`file.cpp": [(5, 5)]},
            "https://github.com/boykopovar/AnyPS5/blob/main",
        )

        self.assertIn(
            "`` example`file.cpp ``",
            result,
        )
        self.assertIn(
            "example%60file.cpp#L5",
            result,
        )

class CommentStateTests(unittest.TestCase):
    def setUp(self):
        if os.name == "nt" or not all(shutil.which(tool) for tool in ("bash", "jq", "sed", "sha256sum")):
            self.skipTest("the workflow step runs in bash with jq, sed and sha256sum on Linux")
        lines = (Path(__file__).resolve().parents[2] / ".github/workflows/pr-overlap.yml").read_text(encoding="utf-8").splitlines()
        start = next(index for index, line in enumerate(lines) if "tools/pr_overlap.py" in line)
        indent = len(lines[start]) - len(lines[start].lstrip())
        while start > 0 and len(lines[start - 1]) - len(lines[start - 1].lstrip()) == indent:
            start -= 1
        step = []
        for line in lines[start:]:
            if line.strip() and len(line) - len(line.lstrip()) < indent:
                break
            step.append(line[indent:])
        self.step = "\n".join(step).replace("${{ github.event.repository.default_branch }}", "main")
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "overlap").mkdir()

    def tearDown(self):
        self.temp.cleanup()

    def report(self, commit, lines):
        found = {7: {"conflicts": {"core/a.cpp": [lines]}, "added": set()}}
        return pr_overlap.report(found, f"https://github.com/owner/project/blob/{commit}")

    def run_step(self, report, comment):
        (self.root / "overlap" / "42.md").write_bytes(report.encode("utf-8"))
        (self.root / "comment").write_bytes((json.dumps([5, comment]) + "\n" if comment else "").encode("utf-8"))
        calls = self.root / "calls"
        calls.write_text("", encoding="utf-8")
        script = "\n".join([
            "set -eo pipefail",
            "python3() { :; }",
            'gh() { printf \'%s\\n\' "$*" >> "$RUNNER_TEMP/calls"; case "$*" in "api repos/"*) cat "$RUNNER_TEMP/comment";; esac; }',
            self.step,
        ])
        (self.root / "step.sh").write_bytes(script.encode("utf-8"))
        environment = dict(os.environ, RUNNER_TEMP=str(self.root), GH_REPO="owner/project")
        result = subprocess.run(["bash", "step.sh"], cwd=self.root, env=environment, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return [call.split(" ")[:3] for call in calls.read_text(encoding="utf-8").splitlines()]

    def test_new_main_commit_alone_makes_no_requests(self):
        first = self.report("a" * 40, (3, 4))
        self.assertEqual(self.run_step(first, None), [["api", "repos/owner/project/issues/42/comments", "--paginate"], ["pr", "comment", "42"]])
        self.assertEqual(self.run_step(self.report("b" * 40, (3, 4)), first), [])

    def test_lost_state_reads_the_comment_without_rewriting_it(self):
        (self.root / "overlap-state.json").write_text("{}", encoding="utf-8")
        self.assertEqual(self.run_step(self.report("b" * 40, (3, 4)), self.report("a" * 40, (3, 4))), [["api", "repos/owner/project/issues/42/comments", "--paginate"]])

    def test_changed_lines_still_update_the_comment(self):
        first = self.report("a" * 40, (3, 4))
        self.run_step(first, None)
        self.assertEqual(self.run_step(self.report("b" * 40, (5, 6)), first), [["api", "repos/owner/project/issues/42/comments", "--paginate"], ["api", "-X", "PATCH"]])

if __name__ == "__main__":
    unittest.main()
