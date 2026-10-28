from pathlib import Path
import contextlib
import io
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sanitizer_report

REPOSITORY = Path(__file__).resolve().parents[2]
KNOWN_FAILURES = REPOSITORY / ".github" / "sanitizer-known-failures.txt"


def case(name, status="run", body=""):
    return f'<testcase name="{name}" classname="{name}" time="0.1" status="{status}">{body}</testcase>'


def suite(*cases):
    return f'<testsuite name="(empty)" tests="{len(cases)}">' + "".join(cases) + "</testsuite>"


class SanitizerReportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)

    def write(self, text, name="ctest.xml"):
        path = Path(self.directory.name) / name
        path.write_text(text, encoding="utf-8")
        return path

    def run_main(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = sanitizer_report.main([str(arg) for arg in args])
        return code, stdout.getvalue(), stderr.getvalue()

    def test_classifies_passed_skipped_failed_and_not_run_tests(self):
        path = self.write(suite(
            case("passes"),
            case("fails", "fail", '<failure message=""/>'),
            case("skips", "run", '<skipped message="SKIP_RETURN_CODE=77"/>'),
            case("never_built", "notrun"),
            case("switched_off", "disabled"),
        ))
        self.assertEqual(sanitizer_report.results(path), {
            "passes": "passed", "fails": "failed", "skips": "skipped", "never_built": "failed", "switched_off": "skipped",
        })

    def test_not_run_tests_are_skips_only_with_a_ctest_skip_status(self):
        path = self.write(suite(
            case("skip_code", "notrun", '<skipped message="SKIP_RETURN_CODE=77"/><system-out>skipped, no display</system-out>'),
            case("skip_regex", "notrun", '<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/>'),
            case("missing", "notrun", '<skipped message="Unable to find executable"/>'
                                      '<system-out>Unable to find executable: build/tests/missing_tests</system-out>'),
            case("no_message", "notrun", "<skipped/>"),
        ))
        self.assertEqual(sanitizer_report.results(path), {
            "skip_code": "skipped", "skip_regex": "skipped", "missing": "failed", "no_message": "failed",
        })
        code, stdout, _ = self.run_main(path)
        self.assertEqual(code, 1)
        self.assertIn("4 tests: 0 passed, 2 skipped, 2 failed.", stdout)

    def test_summary_counts_and_sorts_the_failed_tests(self):
        text, failing = sanitizer_report.summarize("ASan", {"b": "failed", "a": "failed", "c": "passed", "d": "skipped"})
        self.assertTrue(failing)
        self.assertEqual(text, "### ASan\n\n4 tests: 1 passed, 1 skipped, 2 failed.\n\n"
                               "| Failed test |\n| - |\n| `a` |\n| `b` |\n\n")

    def test_summary_without_failures_has_no_table(self):
        text, failing = sanitizer_report.summarize("ASan", {"a": "passed"})
        self.assertFalse(failing)
        self.assertNotIn("| Failed test |", text)

    def test_exit_code_follows_the_failed_tests(self):
        self.assertEqual(self.run_main(self.write(suite(case("passes"))))[0], 0)
        code, stdout, _ = self.run_main(self.write(suite(case("fails", "fail", '<failure message=""/>'))))
        self.assertEqual(code, 1)
        self.assertIn("| `fails` |", stdout)

    def test_unreadable_reports_exit_with_2(self):
        for text in ("<results/>", suite('<testcase status="run"/>'), "not xml"):
            with self.subTest(text=text):
                code, stdout, stderr = self.run_main(self.write(text))
                self.assertEqual(code, 2)
                self.assertEqual(stdout, "")
                self.assertTrue(stderr.startswith("sanitizer_report: "))
        code, _, stderr = self.run_main(Path(self.directory.name) / "missing.xml")
        self.assertEqual(code, 2)

    def test_a_report_without_tests_is_rejected(self):
        summary = self.write("", "summary.md")
        for text in (suite(), '<testsuites><testsuite name="(empty)" tests="0"/></testsuites>'):
            with self.subTest(text=text):
                code, stdout, stderr = self.run_main(self.write(text), "--summary", summary)
                self.assertEqual(code, 2)
                self.assertEqual(stdout, "")
                self.assertIn("lists no tests", stderr)
        self.assertEqual(summary.read_text(encoding="utf-8"), "")

    def test_summary_file_is_appended_to(self):
        summary = self.write("existing\n", "summary.md")
        code, stdout, _ = self.run_main(self.write(suite(case("passes"))), "--title", "Nightly", "--summary", summary)
        self.assertEqual(code, 0)
        self.assertEqual(summary.read_text(encoding="utf-8"), "existing\n" + stdout)
        self.assertTrue(stdout.startswith("### Nightly\n"))

    def test_summary_write_failure_is_reported_without_a_successful_report(self):
        summary = Path(self.directory.name) / "missing" / "summary.md"
        code, stdout, stderr = self.run_main(self.write(suite(case("passes"))), "--summary", summary)
        self.assertEqual(code, 2)
        self.assertEqual(stdout, "")
        self.assertIn(summary.name, stderr)
        self.assertTrue(stderr.startswith("sanitizer_report: "))
        self.assertNotIn("Traceback", stderr)

    def write_known(self, *lines):
        return self.write("\n".join(lines) + "\n", "known.txt")

    def test_known_failures_apply_only_to_the_selected_sanitizers(self):
        known = self.write_known(
            "fiber address stack switch",
            "",
            "racy thread interceptor gap",
            "both address,undefined shared   reason  ",
        )
        self.assertEqual(sanitizer_report.load_known(known, ["address", "undefined"]),
                         {"fiber": "stack switch", "both": "shared   reason"})
        self.assertEqual(sanitizer_report.load_known(known, ["thread"]), {"racy": "interceptor gap"})

    def test_known_failures_move_to_their_own_table_and_keep_the_run_passing(self):
        found = {"fiber": "failed", "new_bug": "failed", "ok": "passed"}
        text, failing = sanitizer_report.summarize("ASan", found, {"fiber": "guest stack | switch"})
        self.assertTrue(failing)
        self.assertIn("3 tests: 1 passed, 0 skipped, 2 failed (1 known).", text)
        self.assertIn("| Failed test |\n| - |\n| `new_bug` |\n", text)
        self.assertIn("| Known failure | Reason |\n| - | - |\n| `fiber` | guest stack \\| switch |\n", text)
        text, failing = sanitizer_report.summarize("ASan", {"fiber": "failed", "ok": "passed"}, {"fiber": "guest stack switch"})
        self.assertFalse(failing)
        self.assertNotIn("| Failed test |", text)

    def test_main_passes_when_every_failure_is_known(self):
        junit = self.write(suite(case("fiber", "fail", '<failure message=""/>'), case("ok")))
        known = self.write_known("fiber address stack switch")
        self.assertEqual(self.run_main(junit, "--known", known, "--sanitizer", "address")[0], 0)
        self.assertEqual(self.run_main(junit, "--known", known, "--sanitizer", "thread")[0], 1)

    def test_known_needs_a_sanitizer(self):
        known = self.write_known("")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            sanitizer_report.main([str(self.write(suite(case("ok")))), "--known", str(known)])
        self.assertEqual(raised.exception.code, 2)

    def test_malformed_known_failures_are_rejected_with_their_line(self):
        broken = [
            ("fiber address",),
            ("fiber",),
            ("fiber memory stack switch",),
            ("fiber address, stack switch",),
            ("fiber ,address stack switch",),
            ("fiber address stack switch", "fiber address again"),
            ("ok address fine", "fiber address,address twice"),
        ]
        junit = self.write(suite(case("ok")))
        for lines in broken:
            with self.subTest(lines=lines):
                code, _, stderr = self.run_main(junit, "--known", self.write_known(*lines), "--sanitizer", "address")
                self.assertEqual(code, 2)
                self.assertIn(f"known.txt:{len(lines)}: ", stderr)
        code, _, _ = self.run_main(junit, "--known", Path(self.directory.name) / "missing.txt", "--sanitizer", "address")
        self.assertEqual(code, 2)

    def test_known_failures_that_pass_or_did_not_run_are_listed_without_failing(self):
        found = {"fixed": "passed", "still_broken": "failed", "skipped_now": "skipped"}
        known = {"fixed": "was broken", "still_broken": "stack switch", "skipped_now": "device", "renamed": "old name"}
        text, failing = sanitizer_report.summarize("ASan", found, known)
        self.assertFalse(failing)
        self.assertIn("| Listed as known but passed | Reason |\n| - | - |\n| `fixed` | was broken |\n", text)
        self.assertIn("| Listed as known but not in this run |\n| - |\n| `renamed` |\n", text)
        self.assertNotIn("`skipped_now`", text)

    def test_no_stale_tables_when_every_known_failure_still_fails(self):
        text, _ = sanitizer_report.summarize("ASan", {"fiber": "failed"}, {"fiber": "stack switch"})
        self.assertNotIn("Listed as known but", text)

    def test_first_report_prefers_the_summary_then_the_error_then_a_runtime_error(self):
        asan = ("==7==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60d0\n"
                "READ of size 16\n"
                "SUMMARY: AddressSanitizer: heap-buffer-overflow (/build/tests/x+0x12) in Inspect\n")
        self.assertEqual(sanitizer_report.first_report(asan),
                         "SUMMARY: AddressSanitizer: heap-buffer-overflow (/build/tests/x+0x12) in Inspect")
        self.assertEqual(sanitizer_report.first_report("==7==ERROR: LeakSanitizer: detected memory leaks\n"),
                         "ERROR: LeakSanitizer: detected memory leaks")
        self.assertEqual(sanitizer_report.first_report("WARNING: ThreadSanitizer: data race (pid=9)\n"),
                         "WARNING: ThreadSanitizer: data race (pid=9)")
        self.assertEqual(sanitizer_report.first_report("start\n/src/Copy.cpp:35:16: runtime error: null pointer passed as argument 2\n"),
                         "/src/Copy.cpp:35:16: runtime error: null pointer passed as argument 2")
        self.assertEqual(sanitizer_report.first_report("FAIL: plain assertion\n"), "")

    def test_first_report_is_shortened(self):
        report = sanitizer_report.first_report("SUMMARY: AddressSanitizer: " + "x" * 400)
        self.assertEqual(len(report), sanitizer_report.REPORT_LENGTH)
        self.assertTrue(report.endswith("..."))

    def test_failed_tests_show_their_first_report(self):
        junit = self.write(suite(
            case("overread", "fail", '<failure message=""/><system-out>SUMMARY: AddressSanitizer: heap-buffer-overflow in A|B</system-out>'),
            case("plain", "fail", '<failure message=""/><system-out>FAIL: assertion</system-out>'),
            case("ub", "fail", '<failure message=""/><system-err>/a.cpp:1:2: runtime error: shift exponent 64</system-err>'),
        ))
        self.assertEqual(sanitizer_report.reports(junit), {
            "overread": "SUMMARY: AddressSanitizer: heap-buffer-overflow in A|B",
            "ub": "/a.cpp:1:2: runtime error: shift exponent 64",
        })
        code, stdout, _ = self.run_main(junit)
        self.assertEqual(code, 1)
        self.assertIn("| Failed test | First sanitizer report |\n| - | - |\n"
                      "| `overread` | SUMMARY: AddressSanitizer: heap-buffer-overflow in A\\|B |\n"
                      "| `plain` |  |\n"
                      "| `ub` | /a.cpp:1:2: runtime error: shift exponent 64 |\n", stdout)

    def test_failed_tests_without_reports_keep_the_single_column_table(self):
        code, stdout, _ = self.run_main(self.write(suite(case("plain", "fail", '<failure message=""/><system-out>FAIL</system-out>'))))
        self.assertEqual(code, 1)
        self.assertIn("| Failed test |\n| - |\n| `plain` |\n", stdout)



class RepositoryKnownFailuresTests(unittest.TestCase):
    def test_every_entry_parses(self):
        self.assertTrue(sanitizer_report.load_known(KNOWN_FAILURES, list(sanitizer_report.SANITIZERS)))

    def test_entries_stay_sorted_by_test_name(self):
        names = [line.split(None, 1)[0] for line in KNOWN_FAILURES.read_text(encoding="utf-8").splitlines() if line.strip()]
        self.assertEqual(names, sorted(names))


if __name__ == "__main__":
    unittest.main()
