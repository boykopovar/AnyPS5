import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tools.run_report_summary import render


ROOT = Path(__file__).resolve().parents[2]


class RunReportSummaryTests(unittest.TestCase):
    def report(self, name):
        return json.loads((ROOT / "docs/run-report-fixtures" / name).read_text(encoding="utf-8"))

    def test_summary_includes_run_context_and_missing_import(self):
        summary = render(self.report("stop-missing-function.json"))
        self.assertIn("Game: Example Game (PPSA12345)", summary)
        self.assertIn("Result: abort (exit code 134)", summary)
        self.assertIn("Progress: first-draw after 48.9 seconds", summary)
        self.assertIn("libSceNpTrophy::sceNpTrophyCreateContext (1 call)", summary)
        self.assertIn("This summary uses only the fields present in the local run report; it does not upload data.", summary)

    def test_summary_marks_uncollected_sections(self):
        report = self.report("stop-missing-function.json")
        report["coverage"]["milestones"] = False
        report["coverage"]["missing"] = False
        report["coverage"]["shaders"] = False
        report["coverage"]["unsupported"] = False
        summary = render(report)
        self.assertIn("Rendering: milestone data was not collected", summary)
        self.assertIn("Missing functions: not collected", summary)
        self.assertIn("Shaders: compilation data was not collected", summary)
        self.assertIn("Unsupported shader instructions: not collected", summary)

    def test_summary_reports_recovery_incomplete_data_and_truncation(self):
        report = self.report("killed-recovered.json")
        report["truncated"]["functions"] = 2
        summary = render(report)
        self.assertIn("Warning: recovered report", summary)
        self.assertIn("Warning: one or more observations were dropped", summary)
        self.assertIn("Warning: one or more diagnostic lists were truncated", summary)

    def test_unknown_text_does_not_emit_terminal_control_characters(self):
        report = self.report("stop-none.json")
        report["game"]["name"] = "Example\x1b[31mGame"
        summary = render(report)
        self.assertNotIn("\x1b", summary)

    def test_rejects_other_report_formats(self):
        with self.assertRaisesRegex(ValueError, "AnyPS5 run report"):
            render({"format": "other", "version": 1})

    def test_cli_writes_utf8_to_redirected_stdout(self):
        report = self.report("stop-missing-function.json")
        title = "\u65e5\u672c\u8a9e"
        report["game"]["name"] = title
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "anyps5-report.json"
            path.write_text(json.dumps(report, ensure_ascii=False), encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(ROOT / "tools/run_report_summary.py"), str(path)],
                env=dict(os.environ, PYTHONIOENCODING="cp1251"), capture_output=True, timeout=10,
            )
        self.assertEqual(result.returncode, 0, result.stderr.decode("ascii", errors="backslashreplace"))
        self.assertIn(f"Game: {title} (PPSA12345)", result.stdout.decode("utf-8").splitlines())


if __name__ == "__main__":
    unittest.main()
