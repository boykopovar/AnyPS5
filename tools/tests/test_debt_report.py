import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import debt_report

DEBT = "docs/dev/debt/libSceExample.md"
LINK = "../../../core/libs/prx/libSceExample/Export.cpp"


def write(root, files):
    for name, text in files.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


class EntryTests(unittest.TestCase):
    def test_entries_keep_their_section_and_skip_prose(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {
                "docs/dev/TechnicalDebt.md": "# Project technical debt\n\nProse, not an entry.\n",
                DEBT: f"# libSceExample technical debt\n\n### Silent stubs\n\n- [sceA]({LINK}) - returns 0\n\n### Functional\n\n- [sceB]({LINK}) - partial\n  - a sub-item\n",
            })
            found = debt_report.entries(root)
        self.assertEqual([(e.file, e.line, e.section) for e in found], [(DEBT, 5, "Silent stubs"), (DEBT, 9, "Functional")])
        self.assertIn("a sub-item", found[1].text)

    def test_names_come_from_link_subjects_and_lowercase_ticked_identifiers(self):
        text = f"- [sceA, sceB and `sceC`]({LINK}) - `fchmodat` returns `ENOSPC` on `std::string`"
        self.assertEqual(debt_report.subjects(text), ["sceA", "sceB", "sceC"])
        self.assertEqual(debt_report.entry_names(text), ["sceA", "sceB", "sceC", "fchmodat"])
        self.assertEqual(debt_report.entry_names(f"- [Nine unnamed exports]({LINK}) - return 0"), [])


if __name__ == "__main__":
    unittest.main()
