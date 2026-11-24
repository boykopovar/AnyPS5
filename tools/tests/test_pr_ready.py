import contextlib
import io
import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_ready

PASSING = [("conventions", "completed", "success"), ("relinker", "completed", "success")]


def run(name, status="completed", conclusion="success", id=1, app="github-actions", workflow=None):
    return {"__typename": "CheckRun", "databaseId": id, "name": name, "status": status.upper(),
            "conclusion": conclusion.upper() if conclusion else None,
            "checkSuite": {"app": {"slug": app},
                           "workflowRun": {"workflow": {"databaseId": workflow}} if workflow is not None else None}}


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
    def with_contexts(self, *additional, checks=None):
        data = node(checks=checks)
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"].extend(additional)
        return pr_ready.blockers(pr_ready.pull(data))

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

    def test_the_conflict_label_job_does_not_block(self):
        for state, conclusion in (("in_progress", None), ("completed", "cancelled"), ("completed", "failure")):
            with self.subTest(conclusion=conclusion):
                self.assertEqual(pr_ready.blockers(make(checks=PASSING + [("pr", state, conclusion)])), [])

    def test_each_bot_check_name_is_a_job_of_its_bot_workflow_only(self):
        owners = {name: set() for name in pr_ready.BOT_CHECKS}
        for workflow in (Path(__file__).resolve().parents[2] / ".github/workflows").glob("*.yml"):
            text = workflow.read_text(encoding="utf-8")
            if "\njobs:\n" not in text:
                continue
            for line in text.split("\njobs:\n", 1)[1].split("\n"):
                name = line[2:].split(":", 1)[0]
                if line.startswith("  ") and not line.startswith("   ") and line.rstrip().endswith(":") and name in owners:
                    owners[name].add(workflow.name)
        self.assertEqual(owners, {"intake": {"pr-intake.yml"}, "labels": {"pr-labels.yml"}, "eligibility": {"build.yml"},
                                  "report": {"progress-report.yml"}, "pr": {"conflict-label-pr.yml"}})

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

    def test_every_build_workflow_job_counts_as_a_build(self):
        workflow = (Path(__file__).resolve().parents[2] / ".github/workflows/build.yml").read_text(encoding="utf-8")
        jobs = workflow.split("\njobs:\n", 1)[1]
        names = {line[2:].split(":", 1)[0] for line in jobs.split("\n") if line.startswith("  ") and not line.startswith("   ")
                 and line.rstrip().endswith(":")}
        self.assertEqual(names - pr_ready.BOT_CHECKS, pr_ready.BUILD_CHECKS)
        for name in ("pipe-write (macos-15)", "edge-watch (macos-15-intel)"):
            with self.subTest(name=name):
                self.assertTrue(pr_ready.built(make(checks=[("conventions", "completed", "success"), (name, "completed", "success")])))

    def test_the_newest_run_of_a_check_wins(self):
        data = node(checks=[("conventions", "completed", "success")])
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"] += [
            run("relinker", "completed", "cancelled", id=5), run("relinker", "completed", "success", id=9)]
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), [])
        data["commits"]["nodes"][0]["commit"]["statusCheckRollup"]["contexts"]["nodes"][-1]["databaseId"] = 2
        self.assertEqual(pr_ready.blockers(pr_ready.pull(data)), ["failing: relinker"])

    def test_same_job_name_keeps_a_failed_workflow_until_its_own_rerun(self):
        failed = run("windows", "completed", "failure", id=4, workflow=11)
        other = run("windows", id=5, workflow=22)
        self.assertEqual(self.with_contexts(failed, other), ["failing: windows"])
        self.assertEqual(self.with_contexts(failed, other, run("windows", id=6, workflow=11)), [])

    def test_same_check_name_keeps_another_app_or_commit_status(self):
        passing = run("windows", id=5, app="github-actions")
        for failing in (run("windows", "completed", "failure", id=4, app="example-ci"),
                        {"__typename": "StatusContext", "context": "windows", "state": "FAILURE"}):
            with self.subTest(failing=failing):
                self.assertEqual(self.with_contexts(failing, passing), ["failing: windows"])

    def test_conventions_require_success_without_hiding_another_workflow_failure(self):
        success = run("conventions", id=4, workflow=11)
        for conclusion in ("skipped", "neutral"):
            with self.subTest(conclusion=conclusion):
                alternate = run("conventions", "completed", conclusion, id=5, workflow=22)
                self.assertEqual(self.with_contexts(success, alternate, checks=[]), [])
                self.assertEqual(self.with_contexts(alternate, checks=[]), ["conventions did not pass"])
        failed = run("conventions", "completed", "failure", id=3, workflow=22)
        self.assertEqual(self.with_contexts(failed, success, checks=[]), ["failing: conventions"])

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
    def test_checks_must_belong_to_the_reported_head(self):
        data = node()
        data["headRefOid"] = "a" * 40
        commit = data["commits"]["nodes"][0]["commit"]
        commit["oid"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "head commit changed"):
            pr_ready.pull(data)
        commit["oid"] = data["headRefOid"]
        self.assertTrue(pr_ready.pull(data)["checks"])

    def test_pull_request_number_must_be_a_positive_integer(self):
        for number in (True, 0, -1):
            with self.subTest(number=number), self.assertRaisesRegex(ValueError, "positive pull request number"):
                pr_ready.pull(node(number=number))

    def test_rereview_requests_match_the_reviewer_regardless_of_login_case(self):
        self.assertEqual(make(changes=("CodeReviewer", "Other"), requested=("codereviewer",))["changes"], ["Other"])

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
        self.assertEqual(pr_ready.clean("a" * 99 + "*b"), "a" * 99 + "\\*")

    def test_backslashes_in_titles_do_not_unescape_markdown(self):
        self.assertEqual(pr_ready.clean(r"fix: \[click\](https://example.invalid) \*bold\*"), r"fix: \\\[click\\\](https://example.invalid) \\\*bold\\\*")


class MainTests(unittest.TestCase):
    def run_main(self, argv, prs, calls, closed=(), current=""):
        pages = [{"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": False, "endCursor": None}, "nodes": prs}}}}]

        def fake(*args):
            if args[:2] == ("api", "graphql"):
                if "--explain" in argv:
                    number = int(argv[argv.index("--explain") + 1])
                    self.assertIn("resource(url: $url)", args[3])
                    self.assertEqual(args[-1], f"url=https://github.com/o/r/pull/{number}")
                    match = next((pr for pr in prs if pr["number"] == number), None)
                    resource = {"state": "OPEN", "baseRefName": "main", **match} if match else None
                    return json.dumps({"data": {"resource": resource}})
                if any("CLOSED" in arg for arg in args):
                    return json.dumps({"data": {"repository": {"pullRequests": {
                        "pageInfo": {"hasNextPage": False, "endCursor": None},
                        "nodes": [{"number": n} for n in closed]}}}})
                if "--author" in argv:
                    author = argv[argv.index("--author") + 1]
                    self.assertIn("user(login: $login)", args[3])
                    self.assertIn('baseRefName: "main"', args[3])
                    self.assertIn(f"login={author}", args)
                    nodes = [{"repository": {"nameWithOwner": "O/R"}, **pr} for pr in prs]
                    nodes.append({"repository": {"nameWithOwner": "elsewhere/elsewhere"}, **node(number=3, author=author)})
                    return json.dumps({"data": {"user": {"pullRequests": {
                        "pageInfo": {"hasNextPage": False}, "nodes": nodes}}}})
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

    def test_author_explains_each_open_pull_request_of_that_author_and_writes_nothing(self):
        calls = []
        prs = [node(number=9, author="Alice", draft=True), node(number=8, author="bob"), node(number=7, author="alice", changes=["carol"])]
        code, out, _ = self.run_main(["--author", "alice"], prs, calls, closed=[3])
        self.assertEqual(code, 0)
        self.assertEqual(calls, [])
        self.assertEqual(out.splitlines(), [f"#7: {'; '.join(pr_ready.blockers(make(number=7, changes=['carol'])))}",
                                            f"#9: {'; '.join(pr_ready.blockers(make(number=9, draft=True)))}"])

    def test_author_without_open_pull_requests_says_so(self):
        calls = []
        code, out, _ = self.run_main(["--author", "dave"], [node(number=7)], calls)
        self.assertEqual((code, out, calls), (0, "dave has no open pull request to main\n", []))

    def test_author_reports_an_unblocked_pull_request(self):
        code, out, _ = self.run_main(["--author", "alice"], [node(number=7)], [])
        self.assertEqual((code, out), (0, "#7: nothing blocks it\n"))

    def test_author_and_explain_cannot_be_combined(self):
        code, _, err = self.run_main(["--author", "alice", "--explain", "7"], [node(number=7)], [])
        self.assertEqual(code, 2)
        self.assertIn("cannot be combined", err)

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

    def test_explain_reports_only_the_selected_pr_and_leaves_the_queue_alone(self):
        calls = []
        prs = [node(number=1, draft=True), node(number=7, checks=[("conventions", "completed", "failure")],
                                                changes=["bob"])]
        code, out, _ = self.run_main(["--explain", "7", "--issue", "3"], prs, calls, closed=[2])
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ["failing: conventions", "change request not answered by a re-request: bob"])
        self.assertEqual(calls, [])

    def test_explain_an_unknown_pull_request_fails(self):
        code, _, err = self.run_main(["--explain", "9"], [node(number=7)], [])
        self.assertEqual(code, 2)
        self.assertIn("#9 is not an open pull request", err)

    def test_explain_rejects_a_closed_or_retargeted_pull_request(self):
        for fields in ({"state": "MERGED"}, {"state": "CLOSED"}, {"baseRefName": "release"}):
            with self.subTest(fields=fields):
                calls = []
                code, _, err = self.run_main(["--explain", "7"], [{**node(), **fields}], calls)
                self.assertEqual((code, calls), (2, []))
                self.assertIn("#7 is not an open pull request to main", err)

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

    def test_open_pages_reject_invalid_next_cursors_without_restarting(self):
        for cursor in (None, "", 17, []):
            with self.subTest(cursor=cursor):
                page = {"data": {"repository": {"pullRequests": {"nodes": [node()],
                        "pageInfo": {"hasNextPage": True, "endCursor": cursor}}}}}
                calls = []
                def fake(*args):
                    calls.append(args)
                    if len(calls) > 2:
                        raise ValueError("fixture read budget exceeded")
                    return json.dumps(page)
                with patch("pr_ready.pr_labels.gh", fake), self.assertRaisesRegex(ValueError, "no cursor"):
                    pr_ready.fetch("o/r")
                self.assertEqual(len(calls), 1)

    def test_open_and_closed_page_cursors_must_advance(self):
        for closed in (False, True):
            for cursors in (("repeat", "repeat"), ("first", "second", "first")):
                with self.subTest(closed=closed, cursors=cursors):
                    calls = []
                    def fake(*args):
                        calls.append(args)
                        if len(calls) > len(cursors):
                            raise ValueError("fixture read budget exceeded")
                        return json.dumps({"data": {"repository": {"pullRequests": {"nodes": [node()],
                            "pageInfo": {"hasNextPage": True, "endCursor": cursors[len(calls) - 1]}}}}})
                    action = pr_ready.closed_with_label if closed else pr_ready.fetch
                    with patch("pr_ready.pr_labels.gh", fake), self.assertRaisesRegex(ValueError, "did not advance"):
                        action("o/r")
                    self.assertEqual(len(calls), len(cursors))

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

    def test_repeated_pull_number_in_open_pages_is_rejected(self):
        replies = [
            {"data": {"repository": {"pullRequests": {"nodes": [node()], "pageInfo": page}}}}
            for page in ({"hasNextPage": True, "endCursor": "next"}, {"hasNextPage": False})
        ]
        with patch("pr_ready.pr_labels.gh", side_effect=list(map(json.dumps, replies))):
            with self.assertRaisesRegex(ValueError, "duplicate pull request #7"):
                pr_ready.fetch("o/r")

    def test_closed_labels_follow_all_pages_before_cleanup(self):
        pages = [
            {"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": True, "endCursor": "c1"},
                                                     "nodes": [{"number": number} for number in range(1, 101)]}}}},
            {"data": {"repository": {"pullRequests": {"pageInfo": {"hasNextPage": False, "endCursor": None},
                                                     "nodes": [{"number": 101}]}}}},
        ]
        seen = []

        def fake(*args):
            seen.append(args)
            return json.dumps(pages.pop(0))

        with patch("pr_ready.pr_labels.gh", fake):
            closed = pr_ready.closed_with_label("o/r")
        self.assertEqual(closed, list(range(1, 102)))
        self.assertEqual(len(seen), 2)
        self.assertIn("after=c1", seen[1])

    def test_closed_label_page_without_next_cursor_fails_closed(self):
        page = {"data": {"repository": {"pullRequests": {
            "pageInfo": {"hasNextPage": True, "endCursor": None}, "nodes": [{"number": 5}]}}}}
        with patch("pr_ready.pr_labels.gh", return_value=json.dumps(page)), self.assertRaisesRegex(ValueError, "no cursor"):
            pr_ready.closed_with_label("o/r")


class AuthorFetchTests(unittest.TestCase):
    def test_repeated_pull_number_in_author_pages_is_rejected(self):
        matching = {"repository": {"nameWithOwner": "owner/repo"}, **node()}
        replies = [
            {"data": {"user": {"pullRequests": {"nodes": [matching], "pageInfo": page}}}}
            for page in ({"hasNextPage": True, "endCursor": "next"}, {"hasNextPage": False})
        ]
        with patch("pr_ready.pr_labels.gh", side_effect=list(map(json.dumps, replies))):
            with self.assertRaisesRegex(ValueError, "duplicate pull request #7"):
                pr_ready.fetch_author("owner/repo", "alice")

    def test_author_lookup_follows_pages_and_keeps_only_the_requested_repository(self):
        matching = {"repository": {"nameWithOwner": "Owner/Repo"}, **node(number=3)}
        another = {"repository": {"nameWithOwner": "owner/other"}, **node(number=4)}
        pages = [
            {"pageInfo": {"hasNextPage": True, "endCursor": "next"}, "nodes": [another]},
            {"pageInfo": {"hasNextPage": False, "endCursor": None}, "nodes": [matching]},
        ]
        replies = [json.dumps({"data": {"user": {"pullRequests": page}}}) for page in pages]
        with patch("pr_ready.pr_labels.gh", side_effect=replies) as gh, patch("pr_ready.fetch") as fetch:
            self.assertEqual(pr_ready.fetch_author("owner/repo", "alice"), [make(number=3)])
        self.assertEqual(gh.call_count, 2)
        self.assertEqual(gh.call_args.args[-2:], ("-f", "after=next"))
        fetch.assert_not_called()

    def test_author_without_a_user_node_falls_back_to_the_repository(self):
        missing = {"errors": [{"type": "NOT_FOUND", "path": ["user"]}]}
        error = subprocess.CalledProcessError(1, ["gh"], output=json.dumps(missing))
        with patch("pr_ready.pr_labels.gh", side_effect=error), patch("pr_ready.fetch", return_value=[]) as fetch:
            self.assertEqual(pr_ready.fetch_author("owner/repo", "example[bot]"), [])
        fetch.assert_called_once_with("owner/repo")

    def test_deleted_authors_can_still_be_reported_as_ghost(self):
        ghost = make(author=None)
        with patch("pr_ready.pr_labels.gh") as gh, patch("pr_ready.fetch", return_value=[ghost]) as fetch:
            self.assertEqual(pr_ready.fetch_author("owner/repo", "Ghost"), [ghost])
        gh.assert_not_called()
        fetch.assert_called_once_with("owner/repo")

    def test_other_query_errors_do_not_start_a_repository_scan(self):
        denied = {"errors": [{"type": "FORBIDDEN", "path": ["user"]}]}
        error = subprocess.CalledProcessError(1, ["gh"], output=json.dumps(denied))
        with patch("pr_ready.pr_labels.gh", side_effect=error), patch("pr_ready.fetch") as fetch:
            with self.assertRaises(subprocess.CalledProcessError):
                pr_ready.fetch_author("owner/repo", "alice")
        fetch.assert_not_called()


class PaginationTests(unittest.TestCase):
    def test_pagination_cursor_returns_each_new_cursor_once(self):
        seen = set()
        self.assertEqual(pr_ready.pagination_cursor({'pageInfo': {'endCursor': 'c1'}}, seen, 'x'), 'c1')
        with self.assertRaisesRegex(ValueError, 'did not advance'):
            pr_ready.pagination_cursor({'pageInfo': {'endCursor': 'c1'}}, seen, 'x')
        with self.assertRaisesRegex(ValueError, 'has no cursor'):
            pr_ready.pagination_cursor({'pageInfo': {'endCursor': ''}}, set(), 'x')

    def test_listed_returns_nodes_and_refuses_other_shapes(self):
        self.assertEqual(pr_ready.listed({'nodes': [1, 2]}), [1, 2])
        with self.assertRaisesRegex(ValueError, 'expected a connection'):
            pr_ready.listed([])
class CheckMappingTests(unittest.TestCase):
    def test_check_of_maps_commit_status_states(self):
        pending = pr_ready.check_of({'__typename': 'StatusContext', 'context': 'ci', 'state': 'PENDING'})
        self.assertEqual(pending, {'id': 0, 'name': 'ci', 'source': ('status',), 'done': False, 'conclusion': None})
        self.assertEqual(pr_ready.check_of({'__typename': 'StatusContext', 'context': 'ci', 'state': 'SUCCESS'})['conclusion'], 'success')
        self.assertEqual(pr_ready.check_of({'__typename': 'StatusContext', 'context': 'ci', 'state': 'ERROR'})['conclusion'], 'failure')

    def test_check_of_maps_check_runs_and_refuses_non_objects(self):
        run = pr_ready.check_of({'databaseId': 7, 'name': 'build', 'status': 'COMPLETED', 'conclusion': 'SUCCESS'})
        self.assertEqual(run, {'id': 7, 'name': 'build', 'source': ('run', None, None), 'done': True, 'conclusion': 'success'})
        with self.assertRaisesRegex(ValueError, 'check is not an object'):
            pr_ready.check_of([])
class LatestCheckTests(unittest.TestCase):
    def test_latest_checks_keeps_the_newest_run_and_drops_bot_checks(self):
        checks = [pr_ready.check_of(run(name, id=id)) for name, id in [('a', 1), ('a', 3), ('b', 2), ('labels', 9)]]
        self.assertEqual(pr_ready.latest_checks(checks), checks[1:3])
class LabelApplyTests(unittest.TestCase):
    def test_apply_dry_run_reports_queue_label_changes_without_writing(self):
        import contextlib, io
        prs = [{'number': 1, 'labels': []}, {'number': 2, 'labels': [pr_ready.QUEUE_LABEL]}]
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            pr_ready.apply('owner/repo', prs, [{'number': 1}], True, closed=(3,))
        text = out.getvalue()
        self.assertIn(f'would change #1: +{pr_ready.QUEUE_LABEL}', text)
        self.assertIn(f'would change #2: -{pr_ready.QUEUE_LABEL}', text)
        self.assertIn(f'would change #3: -{pr_ready.QUEUE_LABEL}', text)

    def test_apply_edits_the_label_through_gh_when_not_dry_run(self):
        import contextlib, io
        from unittest import mock
        prs = [{'number': 1, 'labels': []}]
        with mock.patch.object(pr_ready.pr_labels, 'gh') as gh, mock.patch.object(pr_ready.time, 'sleep'), contextlib.redirect_stdout(io.StringIO()):
            pr_ready.apply('owner/repo', prs, [{'number': 1}], False)
        gh.assert_called_once_with('pr', 'edit', '1', '-R', 'owner/repo', '--add-label', pr_ready.QUEUE_LABEL)


if __name__ == "__main__":
    unittest.main()
