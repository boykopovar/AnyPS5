import contextlib
import io
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_ready

PASSING = [("conventions", "completed", "success"), ("relinker", "completed", "success")]


def run(name, status="completed", conclusion="success", id=1):
    return {"__typename": "CheckRun", "databaseId": id, "name": name, "status": status.upper(),
            "conclusion": conclusion.upper() if conclusion else None}


def node(number=7, labels=(), draft=False, mergeable="MERGEABLE", checks=None, changes=(), requested=(),
         created="2026-01-01T00:00:00Z", title="fix: x", author="alice", more=False):
    contexts = [run(name, status, conclusion) for name, status, conclusion in (PASSING if checks is None else checks)]
    return {
        "number": number, "title": title, "isDraft": draft, "mergeable": mergeable, "createdAt": created,
        "author": {"login": author} if author else None,
        "labels": {"nodes": [{"name": name} for name in labels]},
        "reviewRequests": {"nodes": [{"requestedReviewer": {"login": login}} for login in requested]},
        "latestOpinionatedReviews": {"nodes": [{"state": "CHANGES_REQUESTED", "author": {"login": login} if login else None}
                                               for login in changes]},
        "commits": {"nodes": [{"commit": {"statusCheckRollup": {"contexts": {"pageInfo": {"hasNextPage": more}, "nodes": contexts}}}}]},
    }


def make(**kwargs):
    return pr_ready.pull(node(**kwargs))


class BlockerTests(unittest.TestCase):
    def test_a_pull_request_with_passing_checks_has_no_blocker(self):
        self.assertEqual(pr_ready.blockers(make()), [])

    def test_a_draft_is_blocked(self):
        self.assertEqual(pr_ready.blockers(make(draft=True)), ["draft"])

    def test_conflicts_come_from_github_or_the_label(self):
        self.assertEqual(pr_ready.blockers(make(mergeable="CONFLICTING")), ["conflicts with main"])
        self.assertEqual(pr_ready.blockers(make(labels=["needs rebase"])), ["conflicts with main"])

    def test_an_unknown_mergeable_state_does_not_block(self):
        self.assertEqual(pr_ready.blockers(make(mergeable="UNKNOWN")), [])

    def test_other_waiting_labels_block(self):
        self.assertEqual(pr_ready.blockers(make(labels=["perf numbers missing", "open PR limit"])),
                         ["label: open PR limit", "label: perf numbers missing"])

    def test_the_conventions_check_must_have_run(self):
        self.assertIn("conventions has not run", pr_ready.blockers(make(checks=[("relinker", "completed", "success")])))

    def test_a_failing_check_blocks(self):
        reasons = pr_ready.blockers(make(checks=PASSING + [("windows", "completed", "failure")]))
        self.assertEqual(reasons, ["failing: windows"])

    def test_a_running_check_blocks(self):
        reasons = pr_ready.blockers(make(checks=PASSING + [("linux", "in_progress", None)]))
        self.assertEqual(reasons, ["still running: linux"])

    def test_bot_jobs_are_not_counted(self):
        checks = PASSING + [("intake", "completed", "failure"), ("labels", "in_progress", None), ("eligibility", "queued", None)]
        self.assertEqual(pr_ready.blockers(make(checks=checks)), [])

    def test_a_failing_progress_report_does_not_block(self):
        self.assertEqual(pr_ready.blockers(make(checks=PASSING + [("report", "completed", "failure")])), [])

    def test_skipped_conventions_do_not_pass(self):
        checks = [("conventions", "completed", "skipped"), ("relinker", "completed", "success")]
        self.assertEqual(pr_ready.blockers(make(checks=checks)), ["conventions did not pass"])

    def test_skipped_builds_do_not_block(self):
        checks = [("conventions", "completed", "success"), ("linux", "completed", "skipped")]
        self.assertEqual(pr_ready.blockers(make(checks=checks)), [])

    def test_a_pull_request_counts_as_built_only_after_a_passing_build(self):
        self.assertTrue(pr_ready.built(make()))
        self.assertTrue(pr_ready.built(make(checks=[("conventions", "completed", "success"),
                                                    ("relinker (ubuntu-latest, g++)", "completed", "success")])))
        self.assertFalse(pr_ready.built(make(checks=[("conventions", "completed", "success"), ("linux", "completed", "skipped")])))

    def test_the_newest_run_of_a_check_wins(self):
        data = node(checks=[("conventions", "completed", "success")])
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"] += [
            run("relinker", "completed", "cancelled", id=5), run("relinker", "completed", "success", id=9)]
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), [])
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"][-1]["databaseId"] = 2
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["failing: relinker"])

    def test_commit_statuses_are_read(self):
        data = node()
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"] += [
            {"__typename": "StatusContext", "context": "ci/other", "state": "PENDING"}]
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["still running: ci/other"])
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"][-1]["state"] = "FAILURE"
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["failing: ci/other"])

    def test_more_checks_than_one_page_is_not_ready(self):
        self.assertEqual(pr_ready.blockers(make(more=True)), ["too many checks to read"])

    def test_no_checks_at_all_is_not_ready(self):
        data = node()
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"] = None
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["conventions has not run"])

    def test_a_change_request_blocks_until_the_reviewer_is_asked_again(self):
        self.assertEqual(pr_ready.blockers(make(changes=["bob"])),
                         ["change request not answered by a re-request: bob"])
        self.assertEqual(pr_ready.blockers(make(changes=["bob"], requested=["bob"])), [])

    def test_a_re_request_only_clears_that_reviewer(self):
        self.assertEqual(pr_ready.blockers(make(changes=["bob", "carol"], requested=["bob"])),
                         ["change request not answered by a re-request: carol"])

    def test_a_deleted_reviewer_still_blocks(self):
        self.assertEqual(pr_ready.blockers(make(changes=[None])),
                         ["change request not answered by a re-request: ghost"])

    def test_a_deleted_author_is_read(self):
        self.assertEqual(make(author=None)["author"], "ghost")

    def test_a_requested_team_is_ignored(self):
        data = node(changes=["bob"])
        data["reviewRequests"]["nodes"].append({"requestedReviewer": {}})
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["change request not answered by a re-request: bob"])


class PullTests(unittest.TestCase):
    def test_a_missing_number_is_rejected(self):
        with self.assertRaises(ValueError):
            pr_ready.pull({"isDraft": False})

    def test_a_missing_draft_flag_is_rejected(self):
        data = node()
        del data["isDraft"]
        with self.assertRaises(ValueError):
            pr_ready.pull(data)

    def test_a_connection_without_nodes_is_rejected(self):
        data = node()
        data["labels"] = {}
        with self.assertRaises(ValueError):
            pr_ready.pull(data)


class QueueTests(unittest.TestCase):
    def test_titles_come_first_then_the_oldest(self):
        prs = [make(number=1, created="2026-03-01T00:00:00Z"), make(number=2, created="2026-02-01T00:00:00Z"),
               make(number=3, created="2026-04-01T00:00:00Z", labels=["unblocks title"]), make(number=4, draft=True)]
        self.assertEqual([pr["number"] for pr in pr_ready.ranked(prs)], [3, 2, 1])

    def test_the_body_does_not_mention_users_or_break_markdown(self):
        body = pr_ready.queue_body([make(title="fix @bob [x] | `y`", author="eve", labels=["unblocks title"])], 10, 4)
        self.assertIn("oldest 1 of 4", body)
        self.assertIn("(10 open)", body)
        self.assertIn("(unblocks a title)", body)
        self.assertNotIn("(no build ran)", body)
        self.assertNotIn("@bob", body)
        self.assertNotIn("[x]", body)
        self.assertTrue(body.startswith(pr_ready.MARKER))

    def test_the_body_marks_pull_requests_without_a_build(self):
        body = pr_ready.queue_body([make(checks=[("conventions", "completed", "success")])], 1, 1)
        self.assertIn("by alice (no build ran)", body)

    def test_checks_that_are_not_builds_do_not_count_as_a_build(self):
        checks = [("conventions", "completed", "success"), ("report", "completed", "success"),
                  ("library-portability", "completed", "success"), ("linux", "completed", "skipped")]
        pr = make(checks=checks)
        self.assertEqual(pr_ready.blockers(pr), [])
        self.assertFalse(pr_ready.built(pr))

    def test_long_titles_are_cut(self):
        self.assertEqual(len(pr_ready.clean("a" * 500)), 100)


class MainTests(unittest.TestCase):
    def run_main(self, argv, prs, calls, closed=(), current=""):
        pages = [{"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": False, "endCursor": None}, "nodes": prs}}}}]

        def fake(*args):
            if args[:2] == ("api", "graphql"):
                if any("CLOSED" in arg for arg in args):
                    return json.dumps({"data": {"repository": {"pullRequests": {"nodes": [{"number": n} for n in closed]}}}})
                return json.dumps(pages.pop(0))
            if args[:2] == ("issue", "view"):
                return current
            calls.append(args)
            return ""
        out, err = io.StringIO(), io.StringIO()
        with patch("pr_ready.pr_labels.gh", fake), patch("pr_ready.pr_labels.repository", return_value="o/r"), \
                patch("pr_ready.time.sleep"), contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = pr_ready.main(argv)
            except SystemExit as exit:
                code = exit.code
        return code, out.getvalue(), err.getvalue()

    def test_a_ready_pull_request_gets_the_label(self):
        calls = []
        code, out, _ = self.run_main([], [node(number=7)], calls)
        self.assertEqual(code, 0)
        self.assertEqual(calls, [("pr", "edit", "7", "-R", "o/r", "--add-label", "review queue")])
        self.assertIn("changed #7: +review queue", out)

    def test_a_queued_pull_request_that_is_blocked_loses_the_label(self):
        calls = []
        self.run_main([], [node(number=7, labels=["review queue"], draft=True)], calls)
        self.assertEqual(calls, [("pr", "edit", "7", "-R", "o/r", "--remove-label", "review queue")])

    def test_a_pull_request_that_is_already_right_is_left_alone(self):
        calls = []
        self.run_main([], [node(number=7, labels=["review queue"]), node(number=8, draft=True)], calls)
        self.assertEqual(calls, [])

    def test_only_the_first_pull_requests_are_queued(self):
        calls = []
        prs = [node(number=n, created=f"2026-01-{n:02d}T00:00:00Z") for n in range(1, 4)]
        with patch.object(pr_ready, "QUEUE_SIZE", 2):
            self.run_main([], prs, calls)
        self.assertEqual([call[2] for call in calls], ["1", "2"])

    def test_a_dry_run_writes_nothing(self):
        calls = []
        _, out, _ = self.run_main(["--dry-run", "--issue", "3"], [node(number=7)], calls)
        self.assertEqual(calls, [])
        self.assertIn("would change #7", out)

    def test_the_issue_body_is_updated(self):
        calls = []
        self.run_main(["--issue", "3"], [node(number=7)], calls)
        self.assertEqual(calls[-1][:6], ("issue", "edit", "3", "-R", "o/r", "--body"))
        self.assertIn("#7 fix: x by alice", calls[-1][6])

    def test_an_unchanged_issue_body_is_not_rewritten(self):
        calls = []
        body = pr_ready.queue_body([make(number=7)], 1, 1)
        self.run_main(["--issue", "3"], [node(number=7, labels=["review queue"])], calls, current=body)
        self.assertEqual(calls, [])

    def test_closed_pull_requests_lose_the_label(self):
        calls = []
        _, out, _ = self.run_main([], [], calls, closed=[5])
        self.assertEqual(calls, [("pr", "edit", "5", "-R", "o/r", "--remove-label", "review queue")])
        self.assertIn("changed #5: -review queue", out)

    def test_explain_prints_the_blockers_and_writes_nothing(self):
        calls = []
        _, out, _ = self.run_main(["--explain", "7"], [node(number=7, draft=True)], calls)
        self.assertEqual(out.strip(), "draft")
        self.assertEqual(calls, [])

    def test_explain_an_unknown_pull_request_fails(self):
        code, _, err = self.run_main(["--explain", "9"], [node(number=7)], [])
        self.assertEqual(code, 2)
        self.assertIn("#9 is not an open pull request", err)

    def test_a_malformed_response_is_an_error(self):
        code, _, err = self.run_main([], [{"number": 7}], [])
        self.assertEqual(code, 2)
        self.assertIn("cannot read pull requests", err)

    def test_graphql_errors_are_reported(self):
        out, err = io.StringIO(), io.StringIO()
        with patch("pr_ready.pr_labels.gh", return_value=json.dumps({"errors": [{"message": "boom"}]})), \
                patch("pr_ready.pr_labels.repository", return_value="o/r"), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            with self.assertRaises(SystemExit):
                pr_ready.main([])
        self.assertIn("boom", err.getvalue())

    def test_pages_are_followed(self):
        first = {"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": True, "endCursor": "c1"}, "nodes": [node(number=1)]}}}}
        second = {"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": False, "endCursor": None}, "nodes": [node(number=2)]}}}}
        seen = []

        def fake(*args):
            seen.append(args)
            return json.dumps(first if len(seen) == 1 else second)
        with patch("pr_ready.pr_labels.gh", fake):
            prs = pr_ready.fetch("o/r")
        self.assertEqual([pr["number"] for pr in prs], [1, 2])
        self.assertIn("after=c1", seen[1])


if __name__ == "__main__":
    unittest.main()
