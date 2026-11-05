from pathlib import Path
import contextlib
import io
import itertools
import json
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compare_test_results


def case(name, status=None, body=""):
    node = ET.Element("testcase", name=name)
    if status is not None:
        node.set("status", status)
    if body:
        node.extend(ET.fromstring("<body>" + body + "</body>"))
    return ET.tostring(node, encoding="unicode")


def suite(*cases):
    return f'<testsuite tests="{len(cases)}">' + "".join(cases) + "</testsuite>"


class ReportFixture(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def write(self, text, name="results.xml"):
        path = self.root / name
        path.write_text(text, encoding="utf-8")
        return path


class ReportReaderTests(ReportFixture):
    def test_reads_ctest_outcomes_and_skip_reasons(self):
        path = self.write(suite(
            case("passes", "run"),
            case("junit_pass"),
            case("fails", "fail"),
            case("failure", body="<failure/>"),
            case("error", body="<error/>"),
            case("disabled", "disabled"),
            case("junit_skip", body="<skipped/>"),
            case("skip_code", "notrun", '<skipped message="SKIP_RETURN_CODE=77"/>'),
            case("skip_regex", "notrun", '<skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/>'),
            case("missing_binary", "notrun", '<skipped message="Unable to find executable"/>'),
            case("not_run", "notrun"),
        ))
        self.assertEqual(compare_test_results.load_results(path), {
            "passes": "passed", "junit_pass": "passed", "fails": "failed", "failure": "failed",
            "error": "failed", "disabled": "skipped", "junit_skip": "skipped", "skip_code": "skipped",
            "skip_regex": "skipped", "missing_binary": "failed", "not_run": "failed",
        })

    def test_explicit_failure_is_not_hidden_by_skip_metadata(self):
        for status in (None, "run", "fail", "disabled", "notrun"):
            for failure in ("<failure/>", "<error/>"):
                with self.subTest(status=status, failure=failure):
                    path = self.write(suite(case("test", status, failure + '<skipped message="SKIP_RETURN_CODE=77"/>')))
                    self.assertEqual(compare_test_results.load_results(path), {"test": "failed"})
        path = self.write(suite(case("test", "fail", "<skipped/>")))
        self.assertEqual(compare_test_results.load_results(path), {"test": "failed"})

    def test_reads_multiple_suites_and_preserves_names(self):
        names = ["test 界", "test|<>&`\nnext", " spaced "]
        path = self.write('<testsuites tests="3">' + "".join(suite(case(name)) for name in names) + "</testsuites>")
        self.assertEqual(compare_test_results.load_results(path), dict.fromkeys(names, "passed"))

    def test_rejects_duplicate_names_across_suites(self):
        for text in (suite(case("same"), case("same", "fail")),
                     "<testsuites>" + suite(case("same")) + suite(case("same")) + "</testsuites>"):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "listed more than once"):
                    compare_test_results.load_results(self.write(text))

    def test_rejects_empty_unnamed_and_unknown_reports(self):
        for text, message in (("<results/>", "expected a JUnit"), (suite(), "lists no tests"),
                              (suite('<testcase/>'), "has no name"), (suite(case("")), "has no name"),
                              (suite(case("test", "unknown")), "unknown CTest status")):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, message):
                    compare_test_results.load_results(self.write(text))

    def test_rejects_incomplete_or_invalid_declared_counts(self):
        for count in ("0", "2", "-1", "1.0", "many", ""):
            with self.subTest(count=count):
                text = '<testsuite tests="' + count + '">' + case("test") + "</testsuite>"
                with self.assertRaisesRegex(ValueError, "declared test count"):
                    compare_test_results.load_results(self.write(text))
        text = '<testsuites tests="2">' + suite(case("test")) + "</testsuites>"
        with self.assertRaisesRegex(ValueError, "declared test count"):
            compare_test_results.load_results(self.write(text))

    def test_accepts_reports_without_declared_counts_and_utf8_bom(self):
        path = self.write('\ufeff<testsuite>' + case("test") + "</testsuite>")
        self.assertEqual(compare_test_results.load_results(path), {"test": "passed"})

    def test_propagates_unreadable_and_malformed_input(self):
        with self.assertRaises(OSError):
            compare_test_results.load_results(self.root / "missing.xml")
        with self.assertRaises(ET.ParseError):
            compare_test_results.load_results(self.write("<testsuite>"))


class ComparisonTests(unittest.TestCase):
    def test_every_outcome_transition(self):
        expected = {
            ("passed", "passed"): "unchanged", ("passed", "failed"): "new_failure",
            ("passed", "skipped"): "newly_skipped", ("failed", "passed"): "fixed",
            ("failed", "failed"): "existing_failure", ("failed", "skipped"): "newly_skipped",
            ("skipped", "passed"): "resumed", ("skipped", "failed"): "new_failure",
            ("skipped", "skipped"): "unchanged", (None, "passed"): "added",
            (None, "failed"): "new_failure", (None, "skipped"): "added",
            ("passed", None): "missing", ("failed", None): "missing", ("skipped", None): "missing",
        }
        for (before, after), category in expected.items():
            with self.subTest(before=before, after=after):
                report = compare_test_results.compare({"test": before} if before else {}, {"test": after} if after else {})
                self.assertEqual(report["tests"], [{"name": "test", "baseline": before, "candidate": after, "change": category}])
                self.assertEqual(sum(report["changes"].values()), 1)
                self.assertEqual(report["changes"][category], 1)
                self.assertEqual(report["has_regressions"], category in ("new_failure", "newly_skipped", "missing"))

    def test_reports_sorted_names_and_run_totals(self):
        baseline = {"z": "failed", "a": "passed", "b": "skipped"}
        candidate = {"z": "failed", "a": "passed", "new": "passed"}
        report = compare_test_results.compare(baseline, candidate)
        self.assertEqual([test["name"] for test in report["tests"]], ["a", "b", "new", "z"])
        self.assertEqual(report["baseline"], {"total": 3, "passed": 1, "failed": 1, "skipped": 1})
        self.assertEqual(report["candidate"], {"total": 3, "passed": 2, "failed": 1, "skipped": 0})
        self.assertEqual(baseline, {"z": "failed", "a": "passed", "b": "skipped"})
        text = compare_test_results.markdown(report)
        self.assertIn("| <code>z</code> | failed | failed | Existing failure |", text)
        self.assertNotIn("<code>a</code>", text)

    def test_markdown_preserves_names_without_injecting_markup(self):
        name = 'test|<script>&`"\nnext\tpart\r'
        text = compare_test_results.markdown(compare_test_results.compare({name: "passed"}, {name: "failed"}))
        self.assertIn('<code>test&#124;&lt;script&gt;&amp;&#96;&quot;&#10;next&#9;part&#13;</code>', text)
        self.assertNotIn("<script>", text)
        self.assertEqual(len([line for line in text.splitlines() if "<code>" in line]), 1)

    def test_markdown_syntax_in_names_is_literal(self):
        name = '[link](https://example.invalid)*bold*_italic_~deleted~\\`code`'
        value = compare_test_results.cell(name)
        for character in "\\`*_[]!~":
            self.assertNotIn(character, value)
        self.assertIn("&#91;link&#93;", value)


class CommandTests(ReportFixture):
    def run_main(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            result = compare_test_results.main([str(arg) for arg in args])
        return result, stdout.getvalue(), stderr.getvalue()

    def test_exit_codes_cover_regressions_and_existing_failures(self):
        for before, after in itertools.product(compare_test_results.OUTCOMES, repeat=2):
            with self.subTest(before=before, after=after):
                baseline = self.write(suite(case("test", body={"passed": "", "failed": "<failure/>", "skipped": "<skipped/>"}[before])), "before.xml")
                candidate = self.write(suite(case("test", body={"passed": "", "failed": "<failure/>", "skipped": "<skipped/>"}[after])), "after.xml")
                code, stdout, stderr = self.run_main(baseline, candidate, "--json")
                expected = after == "failed" and before != "failed" or after == "skipped" and before != "skipped"
                self.assertEqual(code, int(expected))
                self.assertEqual(stderr, "")
                self.assertEqual(json.loads(stdout)["has_regressions"], expected)

    def test_invalid_inputs_have_no_success_output_or_traceback(self):
        baseline = self.write(suite(case("test")), "before.xml")
        for path in (self.write("<broken>", "broken.xml"), self.write(suite(), "empty.xml"), self.root / "absent.xml"):
            for arguments in ((baseline, path), (path, baseline)):
                with self.subTest(arguments=arguments):
                    code, stdout, stderr = self.run_main(*arguments)
                    self.assertEqual(code, 2)
                    self.assertEqual(stdout, "")
                    self.assertTrue(stderr.startswith("compare_test_results: "))
                    self.assertNotIn("Traceback", stderr)

    def test_real_command_preserves_unicode_json_and_input_bytes(self):
        baseline = self.write(suite(case("test 界")), "baseline 界.xml")
        candidate = self.write(suite(case("test 界", body="<failure/>")), "candidate 界.xml")
        originals = [path.read_bytes() for path in (baseline, candidate)]
        for flags in ([], ["--json"]):
            result = subprocess.run([sys.executable, str(Path(compare_test_results.__file__)), str(baseline), str(candidate), *flags],
                                    capture_output=True, encoding="utf-8", timeout=5)
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("test 界", result.stdout)
            if flags:
                self.assertEqual(json.loads(result.stdout)["format"], "ctest-comparison/1")
            else:
                self.assertTrue(result.stdout.startswith("# CTest comparison\n"))
        self.assertEqual([path.read_bytes() for path in (baseline, candidate)], originals)


if __name__ == "__main__":
    unittest.main()
