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
GONE = "../../../core/libs/prx/libSceExample/Gone.cpp"


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

    def test_generic_words_are_not_names(self):
        text = f"- [libSceExample]({LINK}) - parameterless `int` declarations through `_nid_postfix`, see `sceD`"
        self.assertEqual(debt_report.entry_names(text), ["sceD"])


class LabelTests(unittest.TestCase):
    def test_closer(self):
        cases = {
            "read from the firmware 13.42 disassembly, not measured on a console": "console",
            "unverified on a PS5. `(uint16_t mss)` follows Vita3K": "console",
            "the console's answer for an unwritable output is not known": "console",
            "the native behavior have not been measured": "console",
            "on firmware 13.42 both return 0x802A00FF, measured in an emulator": "console",
            "unknown signature (NID GKMjPmDls-w)": "firmware",
            "signature and behavior are unverified, so this parameterless declaration is for import resolution only": "firmware",
            "no credible prototype was found; it is declared without parameters": "firmware",
            "the argument lists need the firmware module": "firmware",
            "fails on RADV before RDNA2": "gpu",
            "not measured on other drivers, and lavapipe hangs": "gpu",
            "works. Closes when: PPSA01234 reaches its menu": "title",
            "no game title has been verified with it": "title",
            "PPSA01234 calls it with a null pointer": "code",
            "throws for a reason that no longer holds": "code",
            "unmeasured. Closes when: a test pins the order": "code",
            "as a payload process on a PS5 (firmware 13.60) answered them; the host send is unbounded": "code",
            "a dispatch takes 13 ms on an RTX 4090": "code",
            "No title use of the fix is established": "code",
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
                "core/libs/tests/GuestSched.cpp": "int main() { return sched_rr_get_interval_nid_postfix(0, nullptr) + sceC_nid_no_patch(); }\n",
                "core/libs/prx/libSceExample/Export.cpp": "int sceB() { return 0; }\n",
            })
            words = debt_report.test_words(root)
        self.assertTrue({"sceA", "fchmodat", "sched_rr_get_interval", "sceC"} <= words)
        self.assertNotIn("sceB", words)

    def test_untested(self):
        words = {"sceA"}
        self.assertFalse(debt_report.untested(f"- [sceA, sceB]({LINK}) - x", words))
        self.assertTrue(debt_report.untested(f"- [sceB]({LINK}) - x", words))
        self.assertIsNone(debt_report.untested(f"- [Nine unnamed exports]({LINK}) - x", words))

    def test_plain_words_count_only_when_exported(self):
        words = {"start", "writes", "fchmodat"}
        text = f"- [writes]({LINK}) - `start` and `size`"
        self.assertIsNone(debt_report.untested(text, words))
        self.assertFalse(debt_report.untested(text, words, {"start"}))
        self.assertTrue(debt_report.untested(f"- [fchmodat]({LINK}) - x", set(), {"fchmodat"}))
        self.assertTrue(debt_report.untested(f"- [sceA]({LINK}) - x", {"start"}))

    def test_exported_names_skip_tests(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {
                "core/libs/prx/libSceExample/Export.cpp": "int APS5_VABI sceA(int a) { return a; }\nextern \"C\" int fchmodat_nid_postfix(int a) { return a; }\n",
                "core/libs/prx/libSceExample/tests/Example.cpp": "int APS5_VABI sceTestOnly() { return 0; }\n",
            })
            self.assertEqual(debt_report.exported_names(root), {"sceA", "fchmodat"})

    def test_helper_names_in_entries_match_test_words(self):
        for suffix in ("_nid_postfix", "_nid_no_patch", "_nid_no_patch_cut"):
            with self.subTest(suffix=suffix):
                text = f"- [sceA{suffix}]({LINK}) - partial"
                self.assertFalse(debt_report.untested(text, {"sceA"}))
                self.assertTrue(debt_report.untested(text, set()))

    def test_plain_helper_exports_keep_their_base_names(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {
                "core/libs/prx/libSceExample/Export.cpp": (
                    "int APS5_VABI fchmodat_nid_no_patch(int a) { return a; }\n"
                    "int APS5_VABI fchownat_nid_no_patch_cut(int a) { return a; }\n"
                    "int APS5_VABI fstatat_nid_postfix(int a) { return a; }\n"
                ),
            })
            exported = debt_report.exported_names(root)
        self.assertEqual(exported, {"fchmodat", "fchownat", "fstatat"})
        for name in exported:
            with self.subTest(name=name):
                self.assertTrue(debt_report.untested(f"- [{name}]({LINK}) - partial", set(), exported))
                self.assertFalse(debt_report.untested(f"- [{name}]({LINK}) - partial", {name}, exported))

    def test_stale_is_a_link_to_a_file_that_is_gone(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, {"core/libs/prx/libSceExample/Export.cpp": "int APS5_VABI sceA() { return 0; }\n"})
            self.assertEqual(debt_report.stale(f"- [sceA]({GONE}) - throws", root), "missing link core/libs/prx/libSceExample/Gone.cpp")
            self.assertIsNone(debt_report.stale(f"- [sceA]({LINK}) - throws", root))


class ReportTests(unittest.TestCase):
    FILES = {
        "docs/dev/TechnicalDebt.md": "# Project technical debt\n",
        DEBT: (
            "# libSceExample technical debt\n\n### Unknown function info\n\n"
            f"- [sceQuiet]({LINK}) - throws until measured on a PS5, on Windows\n"
            f"- [sceLoud]({LINK}) - layout unknown. Labels: firmware, tested, linux\n"
            f"- [sceGone]({GONE}) - layout unknown\n"
        ),
        "core/libs/prx/libSceExample/Export.cpp": "int APS5_VABI sceQuiet() { return 0; }\n",
        "core/libs/prx/libSceExample/tests/Example.cpp": "int main() { return sceLoud(0); }\n",
    }

    def run_main(self, argv):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write(root, self.FILES)
            output = io.StringIO()
            with mock.patch.object(debt_report, "ROOT", root), contextlib.redirect_stdout(output):
                code = debt_report.main(argv)
            return code, output.getvalue()

    def test_json_labels_and_override(self):
        code, output = self.run_main(["--json"])
        rows = json.loads(output)
        self.assertEqual(code, 0)
        self.assertEqual([(r["line"], r["labels"], r["reason"]) for r in rows], [
            (5, ["console", "no-test", "windows"], None),
            (6, ["firmware", "linux"], None),
            (7, ["code", "no-test", "stale?"], "missing link core/libs/prx/libSceExample/Gone.cpp"),
        ])

    def test_text_filter_and_counts(self):
        code, output = self.run_main(["libSceExample", "--label", "no-test"])
        self.assertEqual(code, 0)
        self.assertIn(f"{DEBT}:5: sceQuiet: console, no-test, windows", output)
        self.assertIn(f"{DEBT}:7: sceGone: code, no-test, stale? (missing link core/libs/prx/libSceExample/Gone.cpp)", output)
        self.assertNotIn(f"{DEBT}:6:", output)
        self.assertIn("2 entries: 2 no-test, 1 console, 1 windows, 1 code, 1 stale?", output)

    def test_json_helper_named_entry_is_tested(self):
        files = dict(self.FILES)
        files[DEBT] = f"# libSceExample technical debt\n\n### Functional\n\n- [sceQuiet_nid_no_patch]({LINK}) - partial\n"
        files["core/libs/prx/libSceExample/tests/Example.cpp"] = "int main() { return sceQuiet_nid_no_patch(); }\n"
        with mock.patch.object(self, "FILES", files):
            code, output = self.run_main(["--json"])
        self.assertEqual(code, 0)
        self.assertEqual(json.loads(output)[0]["labels"], ["code"])

    def test_unknown_debt_file_is_a_usage_error(self):
        with contextlib.redirect_stderr(io.StringIO()) as errors, self.assertRaises(SystemExit) as raised:
            self.run_main(["libSceMissing"])
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("no debt file: libSceMissing", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
