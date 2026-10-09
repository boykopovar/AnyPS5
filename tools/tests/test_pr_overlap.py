import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

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
                return json.dumps([pull_requests[start:start + 100]
                                   for start in range(0, len(pull_requests), 100)])
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
                patch.object(pr_overlap, "gh", return_value="[[]]") as gh:
            self.assertEqual(pr_overlap.open_prs("topic/one&two"), {})
        gh.assert_called_once_with(
            "api", "--paginate", "--slurp",
            "repos/owner/project/pulls?state=open&base=topic%2Fone%26two&per_page=100",
        )

    def test_single_page_preserves_author_head_and_dependencies(self):
        pages = [[{"number": 7, "user": {"login": "contributor"}, "head": {"sha": "head-sha"},
                   "body": "Depends on: none\nUnrelated issue #42"}]]
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", return_value=json.dumps(pages)):
            found = pr_overlap.open_prs("main")
        self.assertEqual(found, {
            7: {"ref": "refs/pr/7", "sha": "head-sha", "author": "contributor", "depends": set()},
        })


if __name__ == "__main__":
    unittest.main()
