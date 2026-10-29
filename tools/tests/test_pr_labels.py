import contextlib
import copy
import io
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_labels

SUMMARY = {
    "commit": "abc123", "dirty": False, "env": {"APS5_TRACE_FRAMES": "600:4"},
    "gpu": "GPU", "driver": "Driver 1", "os": "OS 1", "resolution": "1920x1080",
    "window": {"spec": "frames 600:4", "first_present": 600, "ms": 80.0}, "presents": 4,
    "frame_ms": {"p50": 20.0, "p90": 40.0, "p99": 40.0, "max": 40.0},
    "gpu_busy_ms_per_present": 5.0, "worker_busy_pct": {"queue worker 0x0": 50.0},
    "top_phases_ms_per_frame": {"Driver.Packet": 12.0, "Driver.Present": 2.0},
    "trace_dropped": False, "trace": "aps5-trace-abc123-7.json.gz",
}
TEMPLATE = ("- [ ] Unblocks: <!-- title ID and Game bring-up discussion, e.g. PPSA01325 #3232, or none -->\n"
            "- [ ] Depends on: none\n")


def summary(commit):
    record = copy.deepcopy(SUMMARY)
    record["commit"] = commit
    return json.dumps(record)


def pr(title="fix: x", body="", labels=()):
    return {"number": 7, "title": title, "body": body, "labels": list(labels), "state": "open"}


class TitleIdTests(unittest.TestCase):
    def test_forms(self):
        cases = [
            (TEMPLATE, set()),
            ("- [x] Unblocks: PPSA01325 #3232\n", {"PPSA01325"}),
            ("- [ ] Unblocks: **PPSA01325 #3232, CUSA12345 #1**", {"PPSA01325", "CUSA12345"}),
            ("- [ ] Unblocks: none\nSeen on PPSA01325.\n", set()),
            ("- [x] Unblocks: none by itself; foundation for PPSA27836 in #6570\n", set()),
            ("- [x] Unblocks: None (PPSA03912 #5146 is already in game; this is a speedup)\n", set()),
            ("- [x] Unblocks: PPSA01325 #3232 (none of the others)\n", {"PPSA01325"}),
            ("- [ ] Unblocks:\n  - PPSA01325 #3232\n  - PPSA00001\n- [ ] Depends on: none\n", {"PPSA01325", "PPSA00001"}),
            ("- [ ] Unblocks:\n- [ ] Depends on: PPSA01325\n", set()),
            ("- [ ] Unblocks: <!-- PPSA01325 -->\n", set()),
            ("- [x] Unblocks: PPSA01325_00 #3232\n", {"PPSA01325"}),
            ("- [x] Unblocks: PPSA013250, XPPSA01325\n", set()),
            ("", set()),
        ]
        for body, expected in cases:
            with self.subTest(body=body):
                self.assertEqual(pr_labels.title_ids(body), expected)

    def test_none_body(self):
        self.assertEqual(pr_labels.title_ids(None), set())


class PerfTests(unittest.TestCase):
    def test_titles(self):
        for title, expected in [("perf(agc): x", True), ("perf: x", True), ("perf!: x", True), ("perf(agc)!: x", True),
                                ("fix(perf): x", False), ("feat: perf", False), ("performance: x", False)]:
            with self.subTest(title=title):
                self.assertEqual(pr_labels.is_perf(title), expected)


class NumbersTests(unittest.TestCase):
    def test_counts(self):
        cases = [
            ("", False),
            (summary("a"), False),
            (summary("a") + "\n" + summary("b"), True),
            ("before\n" + summary("a") + "\nafter\n" + summary("b") + "\n", True),
            (summary("a") + "\n" + summary("a"), False),
            (summary("a") + "\n{not json", False),
            ("{}\n{}", False),
            ("```json\n" + json.dumps(json.loads(summary("a")), indent=2) + "\n```\n```json\n" + summary("b") + "\n```", True),
            ("~~~json\n" + json.dumps(json.loads(summary("a")), indent=2) + "\n~~~\n~~~json\n" + summary("b") + "\n~~~", True),
            ("<!-- " + summary("a") + " -->\n" + summary("b"), False),
            (summary("a") + "\r\n" + summary("b") + "\r\n", True),
            (summary("a") + "\n{" + "[" * 100000 + "\n" + summary("b"), True),
        ]
        for body, expected in cases:
            with self.subTest(body=body[:40]):
                self.assertEqual(pr_labels.has_numbers(body), expected)

    def test_invalid_summary_not_counted(self):
        broken = json.loads(summary("b"))
        broken["presents"] = 0
        self.assertFalse(pr_labels.has_numbers(summary("a") + "\n" + json.dumps(broken)))


class LabelTests(unittest.TestCase):
    def test_labels(self):
        self.assertEqual(pr_labels.labels_for(pr()), (set(), set()))
        self.assertEqual(pr_labels.labels_for(pr("perf: x", TEMPLATE)), ({"perf", "perf numbers missing"}, set()))
        both = TEMPLATE + summary("a") + "\n" + summary("b")
        self.assertEqual(pr_labels.labels_for(pr("perf: x", both, ["perf", "perf numbers missing"])), (set(), {"perf numbers missing"}))
        self.assertEqual(pr_labels.labels_for(pr("fix: x", "Unblocks: PPSA01325", ["perf", "other"])), ({"unblocks title"}, {"perf"}))
        self.assertEqual(pr_labels.labels_for(pr("fix: x", "Unblocks: none", ["unblocks title", "other"])), (set(), {"unblocks title"}))


class ProcessTests(unittest.TestCase):
    def run_process(self, pull, comments=0, dry_run=False, comment=True):
        calls = []

        def fake(*args):
            calls.append(args)
            return ""

        with patch.object(pr_labels, "gh", side_effect=fake), patch.object(pr_labels, "commented", return_value=comments) as counted:
            result = pr_labels.process("o/r", pull, dry_run, comment)
        self.comment_lookups = counted.call_count
        return result, calls

    def test_backfill_labels_without_commenting(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE), comment=False)
        self.assertEqual(result, ({"perf", "perf numbers missing"}, set(), False))
        self.assertEqual(calls, [("pr", "edit", "7", "-R", "o/r", "--add-label", "perf,perf numbers missing")])
        self.assertEqual(self.comment_lookups, 0)

    def test_no_edit_when_labels_match(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE, ["perf", "perf numbers missing"]), comments=1)
        self.assertEqual(result, (set(), set(), False))
        self.assertEqual(calls, [])

    def test_edit_and_single_comment(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE))
        self.assertEqual(result, ({"perf", "perf numbers missing"}, set(), True))
        self.assertEqual(calls[0], ("pr", "edit", "7", "-R", "o/r", "--add-label", "perf,perf numbers missing"))
        self.assertIn("-f", calls[1])
        self.assertIn(pr_labels.MARKER, calls[1][-1])
        self.assertEqual(len(calls), 2)

    def test_comment_not_posted_twice(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE, ["perf", "perf numbers missing"]), comments=1)
        self.assertFalse(result[2])
        result, calls = self.run_process(pr("perf: x", TEMPLATE), comments=1)
        self.assertEqual(len(calls), 1)

    def test_comment_restored_for_labeled_pr_without_one(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE, ["perf", "perf numbers missing"]))
        self.assertTrue(result[2])
        self.assertEqual(len(calls), 1)

    def test_numbers_present_removes_label_keeps_comment(self):
        body = summary("a") + "\n" + summary("b")
        result, calls = self.run_process(pr("perf: x", body, ["perf", "perf numbers missing"]), comments=1)
        self.assertEqual(result, (set(), {"perf numbers missing"}, False))
        self.assertEqual(calls, [("pr", "edit", "7", "-R", "o/r", "--remove-label", "perf numbers missing")])

    def test_dry_run_writes_nothing(self):
        result, calls = self.run_process(pr("perf: x", TEMPLATE), dry_run=True)
        self.assertEqual(result[2], True)
        self.assertEqual(calls, [])

    def test_closed_pull_request_ignored(self):
        pull = pr("perf: x", TEMPLATE)
        pull["state"] = "closed"
        self.assertEqual(self.run_process(pull)[1], [])


class MainTests(unittest.TestCase):
    def test_invalid_pages_fail_before_any_label_or_comment_write(self):
        valid = json.dumps([{"number": 7, "title": "perf: example", "body": "", "labels": [], "state": "open"}])
        for response in ("", " \r\n", "{", "{}", valid + "\n{", "[" * 2000):
            with self.subTest(response=response[:20]):
                calls = []

                def api(*arguments):
                    calls.append(arguments)
                    self.assertEqual(arguments[:2], ("api", "--paginate"))
                    return response

                errors = io.StringIO()
                with patch.object(pr_labels, "gh", side_effect=api), \
                        patch.dict(pr_labels.os.environ, {"GITHUB_REPOSITORY": "fixture/repo"}), \
                        contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(errors):
                    with self.assertRaises(SystemExit) as raised:
                        pr_labels.main(["--all"])
                self.assertEqual(raised.exception.code, 2)
                self.assertIn("cannot read pull requests", errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())
                self.assertEqual(len(calls), 1)

    def test_legitimate_empty_arrays_remain_successful(self):
        for response in ("[]", " []\n[] \n"):
            with self.subTest(response=response), patch.object(pr_labels, "gh", return_value=response) as api, \
                    patch.dict(pr_labels.os.environ, {"GITHUB_REPOSITORY": "fixture/repo"}):
                self.assertEqual(pr_labels.main(["--all", "--dry-run"]), 0)
                api.assert_called_once()

    def test_failed_pull_request_does_not_stop_the_others(self):
        listed = [{"number": number, "title": "perf: x", "body": "", "labels": [], "state": "open"} for number in (1, 2)]
        edited = []

        def fake(*args):
            if args[0] == "api":
                return json.dumps(listed)
            edited.append(args[2])
            if args[2] == "1":
                raise pr_labels.subprocess.CalledProcessError(1, "gh", stderr="HTTP 403")
            return ""

        with patch.object(pr_labels, "gh", side_effect=fake), patch.object(pr_labels, "WRITE_PAUSE", 0), \
                patch.dict(pr_labels.os.environ, {"GITHUB_REPOSITORY": "o/r"}):
            with self.assertRaises(SystemExit) as raised:
                pr_labels.main(["--all"])
        self.assertEqual(edited, ["1", "2"])
        self.assertIn("#1", str(raised.exception.code))

    def test_main_handles_called_process_error_when_fetching_pr(self):
        def fake(*args):
            raise pr_labels.subprocess.CalledProcessError(1, "gh", stderr="PR not found")

        with patch.object(pr_labels, "gh", side_effect=fake), \
                patch.dict(pr_labels.os.environ, {"GITHUB_REPOSITORY": "o/r"}), \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                pr_labels.main(["--pr", "999"])
        self.assertEqual(raised.exception.code, 2)

    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with patch.object(pr_labels.sys, "stdout", out), patch.object(pr_labels.sys, "stderr", err):
            try:
                pr_labels.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
