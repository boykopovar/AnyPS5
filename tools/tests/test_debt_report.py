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


class LabelTests(unittest.TestCase):
    def test_closer(self):
        cases = {
            "read from the firmware 13.42 disassembly, not measured on a console": "console",
            "the argument lists need the firmware module": "firmware",
            "fails on RADV before RDNA2": "gpu",
            "works. Closes when: PPSA01234 reaches its menu": "title",
            "no game title has been verified with it": "title",
            "PPSA01234 calls it with a null pointer": "code",
            "throws for a reason that no longer holds": "code",
            "unmeasured. Closes when: a test pins the order": "code",
        }
        for text, expected in cases.items():
            with self.subTest(text=text):
                self.assertEqual(debt_report.closer(f"- [sceA]({LINK}) - {text}"), expected)

    def test_platforms(self):
        self.assertEqual(debt_report.platforms("on Windows and macOS"), ["windows", "macos"])
        self.assertEqual(debt_report.platforms("on Windows, macOS and Linux"), [])
        self.assertEqual(debt_report.platforms("on a windows host"), [])

    def test_test_words_read_only_test_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {
                "core/libs/prx/libSceExample/tests/Example.cpp": "int main() { return sceA(); }\n",
                "core/libs/tests/test_example.py": "NAME = 'fchmodat'\n",
                "core/libs/prx/libSceExample/Export.cpp": "int sceB() { return 0; }\n",
            })
            words = debt_report.test_words(root)
        self.assertTrue({"sceA", "fchmodat"} <= words)
        self.assertNotIn("sceB", words)

    def test_untested(self):
        words = {"sceA"}
        self.assertFalse(debt_report.untested(f"- [sceA, sceB]({LINK}) - x", words))
        self.assertTrue(debt_report.untested(f"- [sceB]({LINK}) - x", words))
        self.assertIsNone(debt_report.untested(f"- [Nine unnamed exports]({LINK}) - x", words))


SOURCE = (
    "int APS5_VABI sceQuiet(int a) { return a; }\n"
    "int APS5_VABI sceLoud(int a) { NotImplemented_nid_no_patch(__func__); }\n"
    "int APS5_VABI sceThrow(int a) { if (a) throw std::runtime_error(\"x\"); return 0; }\n"
)


class StaleTests(unittest.TestCase):
    def check(self, section, text):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {"core/libs/prx/libSceExample/Export.cpp": SOURCE})
            debt_report.file_definitions.cache_clear()
            return debt_report.stale(debt_report.Entry(DEBT, 1, section, text), root)

    def test_reasons(self):
        self.assertEqual(self.check("Functional", "- [sceA](../../../core/libs/prx/libSceExample/Gone.cpp) - x"),
                         "missing link core/libs/prx/libSceExample/Gone.cpp")
        self.assertEqual(self.check("Unknown function info", f"- [sceQuiet]({LINK}) - throws until measured"), "no longer throws: sceQuiet")
        self.assertIsNone(self.check("Unknown function info", f"- [sceLoud, sceThrow]({LINK}) - throws until measured"))
        self.assertEqual(self.check("Silent stubs", f"- [sceLoud]({LINK}) - returns 0"), "stub now throws: sceLoud")
        self.assertIsNone(self.check("Silent stubs", f"- [sceQuiet]({LINK}) - returns 0"))
        self.assertIsNone(self.check("Unknown function info", f"- [sceMissing]({LINK}) - throws"))
        self.assertIsNone(self.check("Functional", f"- [sceQuiet]({LINK}) - layout unknown"))


if __name__ == "__main__":
    unittest.main()
