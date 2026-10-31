import contextlib
import io
import json
import re
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_unchanged

MAIN = "m" * 40
OLD = "o" * 40


def node(number=7, base=MAIN, same=True, labels=(), merge=True):
    data = {"number": number, "labels": {"nodes": [{"name": name} for name in labels]}, "potentialMergeCommit": None}
    if merge:
        data["potentialMergeCommit"] = {"tree": {"oid": "t1"},
                                        "parents": {"nodes": [{"oid": base, "tree": {"oid": "t1" if same else "t2"}},
                                                              {"oid": "h" * 40, "tree": {"oid": "t3"}}]}}
    return data


def page(nodes, more=False, cursor=None):
    return {"data": {"repository": {"defaultBranchRef": {"target": {"oid": MAIN, "tree": {"oid": "t9"}}},
                                    "pullRequests": {"pageInfo": {"hasNextPage": more, "endCursor": cursor}, "nodes": nodes}}}}


class Api:
    def __init__(self, nodes, files=(), blobs=None, comments=0):
        self.pages = [page(nodes)]
        self.files = list(files)
        self.blobs = blobs or {}
        self.comments = comments
        self.calls = []

    def __call__(self, *args):
        if args[:2] == ("api", "graphql"):
            query = args[3]
            if "pullRequests" in query:
                return json.dumps(self.pages.pop(0))
            fields = {}
            for alias, expression in re.findall(r'(\w+): object\(expression: ("(?:\\.|[^"\\])*")\)', query):
                folder = json.loads(expression).split(":", 1)[1]
                side = "base" if alias.startswith("b") else "main"
                entries = []
                for path in self.files:
                    parent, _, leaf = path.rpartition("/")
                    item = self.blobs.get((side, path), (33188, "blob", "same"))
                    if parent == folder and item is not None:
                        entries.append({"name": leaf, "mode": item[0], "type": item[1], "oid": item[2]})
                fields[alias] = {"entries": entries}
            return json.dumps({"data": {"repository": fields}})
        if args[:2] == ("api", "--paginate") and "/files" in args[2]:
            return json.dumps(self.files)
        if args[:2] == ("api", "--paginate") and "/comments" in args[2]:
            return str(self.comments)
        self.calls.append(args)
        return ""


class CandidateTests(unittest.TestCase):
    def test_a_test_merge_equal_to_its_base_is_a_candidate(self):
        self.assertEqual(pr_unchanged.candidate(node())["base"], MAIN)

    def test_a_test_merge_that_changes_files_is_not(self):
        self.assertIsNone(pr_unchanged.candidate(node(same=False))["base"])

    def test_no_test_merge_is_not(self):
        self.assertIsNone(pr_unchanged.candidate(node(merge=False))["base"])

    def test_a_missing_number_is_rejected(self):
        with self.assertRaises(ValueError):
            pr_unchanged.candidate({"labels": {"nodes": []}})


class ConfirmTests(unittest.TestCase):
    def confirm(self, base, files=("a.cpp",), blobs=None):
        api = Api([], files, blobs)
        with patch.object(pr_unchanged.pr_labels, "gh", api):
            return pr_unchanged.confirmed("o/r", MAIN, {"number": 7, "labels": set(), "base": base})

    def test_a_merge_tested_on_current_main_is_confirmed(self):
        self.assertTrue(self.confirm(MAIN))

    def test_an_older_merge_is_confirmed_when_its_files_did_not_change(self):
        self.assertTrue(self.confirm(OLD, ["a.cpp", "b.cpp"]))

    def test_an_older_merge_is_not_confirmed_when_a_file_changed(self):
        self.assertFalse(self.confirm(OLD, ["a.cpp", "src/b.cpp"], {("main", "src/b.cpp"): (33188, "blob", "new")}))

    def test_a_mode_change_alone_is_not_confirmed(self):
        self.assertFalse(self.confirm(OLD, ["tools/run.sh"], {("main", "tools/run.sh"): (33261, "blob", "same")}))

    def test_a_file_type_change_alone_is_not_confirmed(self):
        self.assertFalse(self.confirm(OLD, ["link"], {("base", "link"): (40960, "blob", "same")}))

    def test_a_missing_folder_counts_as_changed(self):
        api = Api([], ["gone/a.cpp"])
        real = api.__call__

        def fake(*args):
            if args[:2] == ("api", "graphql") and "pullRequests" not in args[3]:
                data = json.loads(real(*args))
                data["data"]["repository"]["m0"] = None
                return json.dumps(data)
            return real(*args)
        with patch.object(pr_unchanged.pr_labels, "gh", fake):
            self.assertFalse(pr_unchanged.unchanged_since("o/r", OLD, MAIN, ["gone/a.cpp"]))

    def test_a_file_added_since_counts_as_changed(self):
        api = Api([], ["a.cpp"])
        api.blobs = {("base", "a.cpp"): None}
        with patch.object(pr_unchanged.pr_labels, "gh", api):
            self.assertFalse(pr_unchanged.unchanged_since("o/r", OLD, MAIN, ["a.cpp"]))

    def test_too_many_files_is_not_confirmed(self):
        self.assertFalse(self.confirm(OLD, [f"f{n}" for n in range(pr_unchanged.MAX_FILES + 1)]))

    def test_no_files_is_not_confirmed(self):
        self.assertFalse(self.confirm(OLD, []))

    def test_paths_are_quoted_in_the_query(self):
        api = Api([], ['a "b".cpp'])
        with patch.object(pr_unchanged.pr_labels, "gh", api), patch.object(pr_unchanged, "graphql", wraps=pr_unchanged.graphql) as spy:
            pr_unchanged.unchanged_since("o/r", OLD, MAIN, ['a "b".cpp'])
        self.assertIn(json.dumps(OLD + ":"), spy.call_args[0][0])

    def test_folders_are_quoted_in_the_query(self):
        api = Api([], ['x "y"/a.cpp'])
        with patch.object(pr_unchanged.pr_labels, "gh", api), patch.object(pr_unchanged, "graphql", wraps=pr_unchanged.graphql) as spy:
            self.assertTrue(pr_unchanged.unchanged_since("o/r", OLD, MAIN, ['x "y"/a.cpp']))
        self.assertIn(json.dumps(OLD + ':x "y"'), spy.call_args[0][0])


class MainTests(unittest.TestCase):
    def run_main(self, api, argv=()):
        out, err = io.StringIO(), io.StringIO()
        with patch.object(pr_unchanged.pr_labels, "gh", api), patch.object(pr_unchanged.pr_labels, "repository", return_value="o/r"), \
                patch.object(pr_unchanged.time, "sleep"), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = pr_unchanged.main(list(argv))
            except SystemExit as exit:
                code = exit.code
        return code, out.getvalue(), err.getvalue()

    def test_a_confirmed_pull_request_is_labeled_and_commented_once(self):
        api = Api([node()])
        code, out, _ = self.run_main(api)
        self.assertEqual(code, 0)
        self.assertEqual(api.calls[0], ("pr", "edit", "7", "-R", "o/r", "--add-label", "already in main"))
        self.assertIn(pr_unchanged.MARKER, api.calls[1][-1])
        self.assertIn("Nothing is closed automatically", api.calls[1][-1])
        self.assertIn("changed #7: +already in main", out)

    def test_no_second_comment(self):
        api = Api([node()], comments=1)
        self.run_main(api)
        self.assertEqual(api.calls, [("pr", "edit", "7", "-R", "o/r", "--add-label", "already in main")])

    def test_the_label_is_removed_when_merging_changes_main_again(self):
        api = Api([node(same=False, labels=["already in main"])])
        self.run_main(api)
        self.assertEqual(api.calls, [("pr", "edit", "7", "-R", "o/r", "--remove-label", "already in main")])

    def test_a_labeled_confirmed_pull_request_with_its_comment_is_left_alone(self):
        api = Api([node(labels=["already in main"])], comments=1)
        self.run_main(api)
        self.assertEqual(api.calls, [])

    def test_a_labeled_confirmed_pull_request_without_its_comment_gets_it(self):
        api = Api([node(labels=["already in main"])])
        self.run_main(api)
        self.assertEqual(len(api.calls), 1)
        self.assertIn(pr_unchanged.MARKER, api.calls[0][-1])

    def test_a_failed_comment_is_retried_on_the_next_run(self):
        api = Api([node()])
        real = api.__call__

        def failing(*args):
            if args[:3] == ("api", "-X", "POST"):
                raise pr_unchanged.subprocess.CalledProcessError(1, "gh", stderr="HTTP 502")
            return real(*args)
        code, _, err = self.run_main(failing)
        self.assertIn("#7", code)
        self.assertEqual(api.calls, [("pr", "edit", "7", "-R", "o/r", "--add-label", "already in main")])
        retry = Api([node(labels=["already in main"])])
        self.run_main(retry)
        self.assertEqual(len(retry.calls), 1)
        self.assertIn(pr_unchanged.MARKER, retry.calls[0][-1])

    def test_a_dry_run_writes_nothing(self):
        api = Api([node()])
        _, out, _ = self.run_main(api, ["--dry-run"])
        self.assertEqual(api.calls, [])
        self.assertIn("would change #7", out)

    def test_nothing_is_ever_closed(self):
        api = Api([node(number=n) for n in range(1, 4)])
        self.run_main(api)
        self.assertFalse([call for call in api.calls if "close" in call])

    def test_graphql_errors_are_reported(self):
        def fake(*args):
            return json.dumps({"errors": [{"message": "boom"}]})
        code, _, err = self.run_main(fake)
        self.assertEqual(code, 2)
        self.assertIn("boom", err)

    def test_pages_are_followed(self):
        api = Api([])
        api.pages = [page([node(number=1, same=False)], True, "c1"), page([node(number=2, same=False)])]
        with patch.object(pr_unchanged.pr_labels, "gh", api):
            head, prs = pr_unchanged.fetch("o/r")
        self.assertEqual([pr["number"] for pr in prs], [1, 2])
        self.assertEqual(head, MAIN)


if __name__ == "__main__":
    unittest.main()
