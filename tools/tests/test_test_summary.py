import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import test_summary


class TestSummaryTests(unittest.TestCase):
    def run_report(self, report):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.xml"
            if report is not None:
                path.write_text(report, encoding="utf-8")
            summary = Path(directory) / "summary.md"
            summary.write_text("Existing summary\n", encoding="utf-8")
            process = subprocess.run(
                [sys.executable, test_summary.__file__, str(path)],
                capture_output=True, encoding="utf-8", timeout=10,
                env={**os.environ, "GITHUB_STEP_SUMMARY": str(summary)},
            )
            self.assertEqual(summary.read_text(encoding="utf-8"), "Existing summary\n" + process.stdout.rstrip("\n") + "\n")
            return process

    def test_pass_skip_failure_and_not_run_are_distinct(self):
        process = self.run_report('''<testsuite tests="6">
            <testcase name="passed" status="run" time="0.1"/>
            <testcase name="failed" status="fail"><failure message="Timeout"/></testcase>
            <testcase name="capability" status="notrun"><skipped message="SKIP_RETURN_CODE"/></testcase>
            <testcase name="skip regex" status="notrun"><skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/></testcase>
            <testcase name="disabled" status="disabled"/>
            <testcase name="blocked" status="notrun"><skipped message="Failed test dependencies: setup"/></testcase>
        </testsuite>''')
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertIn("| 6 | 1 | 1 | 2 | 2 |", process.stdout)
        self.assertIn("| disabled | Not run | Disabled |", process.stdout)
        self.assertIn("| blocked | Not run | Failed test dependencies: setup |", process.stdout)
        self.assertIn("| failed | Failed | Timeout |", process.stdout)

    def test_all_passed(self):
        process = self.run_report('<testsuite tests="1"><testcase name="pass" status="run"/></testsuite>')
        self.assertEqual(process.returncode, 0)
        self.assertIn("| 1 | 1 | 0 | 0 | 0 |", process.stdout)
        self.assertNotIn("| Test |", process.stdout)

    def test_missing_empty_and_malformed_reports_fail(self):
        for report in (None, "broken", '<testsuite tests="0"/>',
                       '<testsuite tests="2"><testcase status="run"/></testsuite>',
                       '<testsuite tests="1"><testcase status="unknown"/></testsuite>',
                       '<other/>'):
            with self.subTest(report=report):
                process = self.run_report(report)
                self.assertEqual(process.returncode, 1)
                self.assertIn("No usable CTest report:", process.stdout)
                self.assertNotIn("| Selected |", process.stdout)

    def test_unicode_and_markdown_are_preserved_safely(self):
        process = self.run_report('''<testsuite tests="1">
            <testcase name="Yōtei | &lt;tag&gt;" status="notrun">
                <skipped message="Missing | fixture"/>
            </testcase>
        </testsuite>''')
        self.assertEqual(process.returncode, 0)
        self.assertIn("Yōtei \\| &lt;tag&gt;", process.stdout)
        self.assertIn("Missing \\| fixture", process.stdout)


if __name__ == "__main__":
    unittest.main()
