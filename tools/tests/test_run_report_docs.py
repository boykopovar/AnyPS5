"""The run report docs must describe the switch, where the file goes and what the system fields and the privacy filter do."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUN_REPORT = ROOT / "docs/user/run-report.md"
USAGE = ROOT / "docs/user/USAGE.md"


def rows(text):
    table = {}
    for line in text.splitlines():
        cells = [cell.strip().strip("`") for cell in line.strip().strip("|").split("|")]
        if len(cells) == 4 and cells[0] in ("Linux", "Windows", "macOS"):
            table[cells[0]] = cells[1:]
    return table


class DocsTests(unittest.TestCase):
    def test_every_system_has_a_row_in_both_docs(self):
        for path in (RUN_REPORT, USAGE):
            self.assertEqual(sorted(rows(path.read_text(encoding="utf-8"))), ["Linux", "Windows", "macOS"], path.name)

    def test_loaded_and_started_are_macos_only(self):
        for path in (RUN_REPORT, USAGE):
            table = rows(path.read_text(encoding="utf-8"))
            self.assertTrue(table["Linux"][0].startswith(("not", "never")), path.name)
            self.assertTrue(table["Windows"][0].startswith(("not", "never")), path.name)
            self.assertEqual(table["macOS"][0], "recorded", path.name)
            for system in table:
                self.assertEqual(table[system][1], "recorded", f"{path.name}: {system} firstDraw/firstPresent")
                self.assertEqual(table[system][2], "recorded", f"{path.name}: {system} stage and frames")

    def test_a_missing_milestone_is_not_presented_as_not_reached(self):
        text = RUN_REPORT.read_text(encoding="utf-8")
        self.assertRegex(text, r"never as .not reached.")
        self.assertIn("coverage.milestones", text)

    def test_usage_says_the_same(self):
        text = USAGE.read_text(encoding="utf-8")
        self.assertRegex(text, r"either one the run did not reach or one this system does not record")

    def test_the_switch_is_documented_in_both_docs(self):
        for path in (RUN_REPORT, USAGE):
            text = path.read_text(encoding="utf-8")
            self.assertIn("--aps5-report=off", text, path.name)
            self.assertIn("APS5_REPORT=0", text, path.name)

    def test_usage_says_where_the_report_goes_and_that_a_killed_run_has_none(self):
        text = USAGE.read_text(encoding="utf-8")
        self.assertIn("reports/<run id>/anyps5-report.json", text)
        self.assertRegex(text, r"A run that is killed, crashes or aborts writes no report")

    def test_privacy_is_described_as_a_filter(self):
        text = USAGE.read_text(encoding="utf-8")
        self.assertRegex(text, r"a filter, not a guarantee")

    def test_system_text_rules_are_documented(self):
        text = RUN_REPORT.read_text(encoding="utf-8")
        for phrase in ("a fixed list in the source", "`NAME` and `ID_LIKE` never count", "unmatched quote", "also clears `Intel Arc`", "long git hash",
                       "A login is never given that allowance", "first three numbers of the kernel release", "UUID"):
            self.assertIn(phrase, text)

    def test_system_fields_are_documented(self):
        text = RUN_REPORT.read_text(encoding="utf-8")
        for field in ("system.osVersion", "system.cpu", "system.ramGiB", "system.gpu"):
            self.assertIn(field, text)

    def test_a_section_that_is_not_collected_is_not_presented_as_empty(self):
        text = RUN_REPORT.read_text(encoding="utf-8")
        self.assertIn("coverage", text)
        self.assertRegex(text, r"don't read its empty list as \"nothing missing\"")


if __name__ == "__main__":
    unittest.main()
