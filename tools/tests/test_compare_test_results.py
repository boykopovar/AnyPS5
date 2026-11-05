from pathlib import Path
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


class ReportReaderTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def write(self, text, name="results.xml"):
        path = self.root / name
        path.write_text(text, encoding="utf-8")
        return path

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


if __name__ == "__main__":
    unittest.main()
