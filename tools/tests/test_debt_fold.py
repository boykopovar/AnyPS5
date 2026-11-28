import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import debt_fold

DEBT = "docs/dev/debt/libSceExample.md"
LINK = "../../../core/libs/prx/libSceExample/Export.cpp"
BASE = (f"# libSceExample technical debt\n\n### Silent stubs\n\n"
        f"- [sceExampleA]({LINK}) (libSceExample) - returns 0\n"
        f"- [sceExampleB]({LINK}) (libSceExample) - returns 0 without reading its argument\n\n"
        f"### Unknown function info\n\n"
        f"- [sceExampleC]({LINK}) (libSceExample) - unknown signature\n"
        f"- On Windows the [example worker]({LINK}) is not joined at exit\n")


BASE_TEXT = (f"# libSceExample technical debt\n\n### Silent stubs\n\n"
             f"- [sceExampleA]({LINK}) (libSceExample) - returns 0\n"
             f"- [sceExampleB]({LINK}) (libSceExample) - returns 0 without reading its argument\n")


class FoldTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.write(DEBT, BASE)

    def test_unreadable_fragment_is_reported_cleanly_in_check_mode(self):
        name = "docs/dev/debt/changes/invalid-encoding.md"
        self.write(name, "")
        fragment = self.root / name
        fragment.write_bytes(b"\xff")
        original = (self.root / DEBT).read_bytes()
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            status = debt_fold.main(["--root", str(self.root), "--check"])
        self.assertEqual(status, 1)
        self.assertIn(str(fragment), errors.getvalue())
        self.assertIn("utf-8", errors.getvalue())
        self.assertEqual((self.root / DEBT).read_bytes(), original)
        self.assertTrue(fragment.is_file())

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def fragment(self, name, text):
        self.write(f"docs/dev/debt/changes/{name}.md", text)

    def debt(self):
        return (self.root / DEBT).read_text(encoding="utf-8")

    def run_fold(self, *args):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = debt_fold.main(["--root", str(self.root), *args])
        return code, stdout.getvalue(), stderr.getvalue()

    def test_two_fragments_add_to_one_section(self):
        self.fragment("a", f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleD]({LINK}) (libSceExample) - returns 0\n")
        self.fragment("b", f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleE]({LINK}) (libSceExample) - returns 1\n")
        self.assertEqual(self.run_fold()[0], 0)
        lines = self.debt().split("\n")
        self.assertEqual(lines[5:8], [f"- [sceExampleB]({LINK}) (libSceExample) - returns 0 without reading its argument",
                                      f"- [sceExampleD]({LINK}) (libSceExample) - returns 0",
                                      f"- [sceExampleE]({LINK}) (libSceExample) - returns 1"])
        self.assertEqual(list((self.root / "docs/dev/debt/changes").glob("*.md")), [])

    def test_add_follows_the_last_entry_that_links_the_same_file(self):
        inventory = "../../../core/libs/prx/libSceExample/InventoryExports.cpp"
        self.write(DEBT, f"# libSceExample technical debt\n\n### Unknown function info\n\n- [sceExampleC]({LINK}) (libSceExample) - unknown signature\n"
                         f"- [`sceExamplePending`]({inventory}) - NID `abc`\n")
        self.fragment("a", f"# libSceExample\n\n## add Unknown function info\n\n- [sceExampleG]({LINK}) (libSceExample) - unknown signature\n")
        self.run_fold()
        self.assertEqual(self.debt().split("\n")[4:7], [f"- [sceExampleC]({LINK}) (libSceExample) - unknown signature",
                                                        f"- [sceExampleG]({LINK}) (libSceExample) - unknown signature",
                                                        f"- [`sceExamplePending`]({inventory}) - NID `abc`"])

    def test_add_keeps_the_blank_line_between_entries_of_a_spaced_file(self):
        self.write(DEBT, f"# libSceExample technical debt\n\n### Silent stubs\n\n- [sceExampleA]({LINK}) (libSceExample) - returns 0\n\n"
                         f"### Unknown function info\n\n- [sceExampleC]({LINK}) (libSceExample) - unknown signature\n")
        self.fragment("a", f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleD]({LINK}) (libSceExample) - returns 0\n"
                           f"- [sceExampleE]({LINK}) (libSceExample) - returns 1\n")
        self.assertEqual(self.run_fold()[0], 0)
        self.assertEqual(self.debt().split("\n")[4:11], [f"- [sceExampleA]({LINK}) (libSceExample) - returns 0", "",
                                                         f"- [sceExampleD]({LINK}) (libSceExample) - returns 0", "",
                                                         f"- [sceExampleE]({LINK}) (libSceExample) - returns 1", "",
                                                         "### Unknown function info"])

    def test_show_writes_a_debt_file_outside_the_ansi_code_page_as_utf8(self):
        self.write(DEBT, BASE.replace("returns 0 without reading its argument", "returns ∞"))
        stdout = io.TextIOWrapper(io.BytesIO(), encoding="cp1252")
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(io.StringIO()):
            code = debt_fold.main(["--root", str(self.root), "--show", "libSceExample"])
            stdout.flush()
        self.assertEqual(code, 0)
        self.assertIn("returns ∞", stdout.buffer.getvalue().decode("utf-8"))

    def test_add_creates_a_missing_section(self):
        self.fragment("a", f"# libSceExample\n\n## add Pending exports\n\n- [sceExampleF]({LINK}) (libSceExample) - throws\n")
        self.run_fold()
        self.assertTrue(self.debt().endswith(f"### Pending exports\n\n- [sceExampleF]({LINK}) (libSceExample) - throws\n"))

    def test_a_fragment_saved_with_a_byte_order_mark_folds_without_it(self):
        path = self.root / "docs/dev/debt/changes/a.md"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(f"﻿# libSceExample\r\n\r\n## add Silent stubs\r\n\r\n- [sceExampleD]({LINK}) (libSceExample) - returns 0\r\n".encode("utf-8"))
        self.assertEqual(self.run_fold("--check")[0], 0)
        self.assertEqual(self.run_fold()[0], 0)
        self.assertIn(f"- [sceExampleD]({LINK}) (libSceExample) - returns 0\n", self.debt())
        self.assertNotIn("﻿", self.debt())

    def test_change_and_remove_by_a_unique_piece_of_text(self):
        self.fragment("a", f"# libSceExample\n\n## change [sceExampleA]\n\n- [sceExampleA]({LINK}) (libSceExample) - clears the pool, as the 4.03 module does\n\n"
                           "## remove On Windows the [example worker]\n")
        self.assertEqual(self.run_fold()[0], 0)
        text = self.debt()
        self.assertIn("clears the pool, as the 4.03 module does", text)
        self.assertNotIn("example worker", text)
        self.assertIn(f"- [sceExampleC]({LINK}) (libSceExample) - unknown signature\n", text)

    def test_refuses_a_target_that_is_not_one_entry(self):
        self.fragment("a", "# libSceExample\n\n## remove returns 0\n")
        code, _, error = self.run_fold()
        self.assertEqual(code, 1)
        self.assertIn("'returns 0' matches 2 entries of libSceExample, not one", error)
        self.assertEqual(self.debt(), BASE)

    def test_refuses_two_fragments_on_one_entry(self):
        self.fragment("a", f"# libSceExample\n\n## change [sceExampleC]\n\n- [sceExampleC]({LINK}) (libSceExample) - takes a handle\n")
        self.fragment("b", "# libSceExample\n\n## remove [sceExampleC]\n")
        code, _, error = self.run_fold()
        self.assertEqual(code, 1)
        self.assertIn("changes/b.md:3 and docs/dev/debt/changes/a.md:3 both target the same libSceExample entry", error)
        self.assertEqual(self.debt(), BASE)

    def test_refuses_an_unknown_library(self):
        self.fragment("a", "# libSceMissing\n\n## remove [sceExampleC]\n")
        code, _, error = self.run_fold()
        self.assertEqual(code, 1)
        self.assertIn("no debt file for libSceMissing", error)

    def test_check_and_show_leave_the_files(self):
        self.fragment("a", "# libSceExample\n\n## remove [sceExampleB]\n")
        self.assertEqual(self.run_fold("--check")[0], 0)
        code, shown, _ = self.run_fold("--show", "libSceExample")
        self.assertEqual(code, 0)
        self.assertNotIn("sceExampleB", shown)
        self.assertEqual(self.debt(), BASE)
        self.assertTrue((self.root / "docs/dev/debt/changes/a.md").is_file())

    def test_folds_index_and_dotted_hyphen_and_underscore_libraries(self):
        index = (
            "# Project technical debt\n\n### Functional\n\n"
            "- [generalItem](core/example.cpp) - original gap\n"
        )
        dotted = (
            "# libSceAjm.native technical debt\n\n### Silent stubs\n\n"
            "- [sceAjmA](../../../core/libs/prx/libSceAjm.native/Export.cpp) (libSceAjm.native) - returns 0\n"
        )
        multi_dot = (
            "# libReactNative.Modules.Vsh.WebAppLauncher technical debt\n\n### Silent stubs\n\n"
            "- [rnA](../../../core/libs/prx/libReactNative.Modules.Vsh.WebAppLauncher/Export.cpp) "
            "(libReactNative.Modules.Vsh.WebAppLauncher) - returns 0\n"
        )
        hyphen = (
            "# libSceFont-module technical debt\n\n### Silent stubs\n\n"
            "- [sceFontA](../../../core/libs/prx/libSceFont-module/Export.cpp) (libSceFont-module) - returns 0\n"
        )
        underscore = (
            "# libkernel_sys technical debt\n\n### Silent stubs\n\n"
            "- [sceKernelA](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - returns 0\n"
        )
        self.write("docs/dev/TechnicalDebt.md", index)
        self.write("docs/dev/debt/libSceAjm.native.md", dotted)
        self.write("docs/dev/debt/libReactNative.Modules.Vsh.WebAppLauncher.md", multi_dot)
        self.write("docs/dev/debt/libSceFont-module.md", hyphen)
        self.write("docs/dev/debt/libkernel_sys.md", underscore)
        self.fragment(
            "valid",
            "# TechnicalDebt\n\n## change [generalItem]\n\n- [generalItem](core/example.cpp) - updated gap\n\n"
            "# libSceAjm.native\n\n## add Silent stubs\n\n"
            "- [sceAjmB](../../../core/libs/prx/libSceAjm.native/Export.cpp) (libSceAjm.native) - returns 1\n\n"
            "# libReactNative.Modules.Vsh.WebAppLauncher\n\n## remove [rnA]\n\n"
            "# libSceFont-module\n\n## add Silent stubs\n\n"
            "- [sceFontB](../../../core/libs/prx/libSceFont-module/Export.cpp) (libSceFont-module) - returns 1\n\n"
            "# libkernel_sys\n\n## add Silent stubs\n\n"
            "- [sceKernelB](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) - returns 1\n",
        )
        code, shown_index, error = self.run_fold("--show", "TechnicalDebt")
        self.assertEqual(code, 0, error)
        self.assertIn("updated gap", shown_index)
        code, shown_multi, error = self.run_fold("--show", "libReactNative.Modules.Vsh.WebAppLauncher")
        self.assertEqual(code, 0, error)
        self.assertNotIn("rnA", shown_multi)
        self.assertEqual((self.root / "docs/dev/TechnicalDebt.md").read_text(encoding="utf-8"), index)
        self.assertEqual(self.run_fold()[0], 0)
        self.assertIn("updated gap", (self.root / "docs/dev/TechnicalDebt.md").read_text(encoding="utf-8"))
        self.assertIn("sceAjmB", (self.root / "docs/dev/debt/libSceAjm.native.md").read_text(encoding="utf-8"))
        self.assertNotIn("rnA", (self.root / "docs/dev/debt/libReactNative.Modules.Vsh.WebAppLauncher.md").read_text(encoding="utf-8"))
        self.assertIn("sceFontB", (self.root / "docs/dev/debt/libSceFont-module.md").read_text(encoding="utf-8"))
        self.assertIn("sceKernelB", (self.root / "docs/dev/debt/libkernel_sys.md").read_text(encoding="utf-8"))

    def test_refuses_traversal_outside_debt_dir_and_leaves_victim_untouched(self):
        readme = "# Example\n\n### Notes\n\n- [victim](core/example.cpp) - original\n"
        sub = "# Sub\n\n### Notes\n\n- [subVictim](core/example.cpp) - original\n"
        dot = "# Dot\n\n### Notes\n\n- [dotVictim](core/example.cpp) - original\n"
        victims = {
            "README.md": readme,
            "docs/dev/debt/sub/target.md": sub,
            "docs/dev/debt/.md": dot,
            "docs/dev/debt/..md": dot,
            "docs/dev/debt/...md": dot,
        }
        cases = [
            ("../../../README", "victim"),
            ("..\\..\\..\\README", "victim"),
            ("sub/target", "subVictim"),
            ("sub\\target", "subVictim"),
            ("", "dotVictim"),
            (".", "dotVictim"),
            ("..", "dotVictim"),
        ]
        for target_name, entry in cases:
            with self.subTest(target=target_name):
                for rel_path, content in victims.items():
                    self.write(rel_path, content)
                fragment_path = self.root / "docs/dev/debt/changes/escape.md"
                fragment_text = (
                    f"# {target_name}\n\n## change {entry}\n\n"
                    f"- [{entry}](core/example.cpp) - hijacked\n"
                )
                self.fragment("escape", fragment_text)
                with self.assertRaises(debt_fold.FoldError):
                    debt_fold.fold(self.root, write=True)
                code, stdout, stderr = self.run_fold()
                self.assertEqual(code, 1)
                self.assertEqual(stdout, "")
                self.assertIn("invalid library", stderr)
                self.assertTrue(fragment_path.is_file())
                self.assertEqual(fragment_path.read_text(encoding="utf-8"), fragment_text)
                self.assertEqual(self.debt(), BASE)
                for rel_path, content in victims.items():
                    self.assertEqual((self.root / rel_path).read_text(encoding="utf-8"), content)
                fragment_path.unlink()

    def test_refuses_cross_platform_malformed_library_names(self):
        readme = "# Example\n\n### Notes\n\n- [victim](core/example.cpp) - original\n"
        self.write("README.md", readme)
        malformed = [
            "../../../README",
            "../README",
            "a/b",
            "./libSceExample",
            "..\\..\\..\\README",
            "..\\README",
            "a\\b",
            ".\\libSceExample",
            "/etc/passwd",
            "/README",
            "\\Windows\\win.ini",
            "\\README",
            "//server/share/README",
            "\\\\server\\share\\README",
            "C:README",
            "C:/README",
            "C:\\README",
            "C:",
            "libSceExample:stream",
            "README:$DATA",
            ":stream",
            "",
            ".",
            "..",
            "...",
            ".hidden",
            "libSceExample.",
            "lib\x00SceExample",
            "lib\nSceExample",
            "lib\rSceExample",
            "lib\tSceExample",
            "lib\x1fSceExample",
            "lib\x7fSceExample",
        ]
        for bad in malformed:
            with self.subTest(library=bad):
                with self.assertRaises(debt_fold.FoldError) as ctx:
                    debt_fold.debt_file(bad)
                self.assertIn("invalid library", str(ctx.exception))
                if "\n" not in bad and "\r" not in bad:
                    fragment_text = f"# {bad}\n\n## remove [sceExampleA]\n"
                    self.fragment("bad", fragment_text)
                    code, stdout, stderr = self.run_fold()
                    self.assertEqual(code, 1)
                    self.assertEqual(stdout, "")
                    self.assertIn("docs/dev/debt/changes/bad.md:1:", stderr)
                    self.assertIn("invalid library", stderr)
                    self.assertEqual(self.debt(), BASE)
                    self.assertEqual((self.root / "README.md").read_text(encoding="utf-8"), readme)
                    self.assertTrue((self.root / "docs/dev/debt/changes/bad.md").is_file())
                    (self.root / "docs/dev/debt/changes/bad.md").unlink()

    def test_refuses_invalid_show_targets_without_traceback_or_mutation(self):
        readme = "# Example\n\n- [victim](core/example.cpp) - secret\n"
        self.write("README.md", readme)
        self.write("docs/dev/debt/sub/target.md", "# Sub\n")
        self.write("docs/dev/debt/.md", "# Empty\n")
        self.write("docs/dev/debt/..md", "# Dot\n")
        self.write("docs/dev/debt/...md", "# DotDot\n")
        pending = f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleD]({LINK}) (libSceExample) - returns 0\n"
        self.fragment("pending", pending)
        bad_targets = [
            "../../../README",
            "..\\..\\..\\README",
            "sub/target",
            "sub\\target",
            "/README",
            "\\README",
            "//server/share/README",
            "\\\\server\\share\\README",
            "C:README",
            "C:\\README",
            "libSceExample:stream",
            "README:$DATA",
            "",
            ".",
            "..",
            "lib\x00SceExample",
            "lib\nSceExample",
            "lib\tSceExample",
            "lib\x7fSceExample",
        ]
        for bad in bad_targets:
            with self.subTest(show=bad):
                code, stdout, stderr = self.run_fold("--show", bad)
                self.assertEqual(code, 1)
                self.assertEqual(stdout, "")
                self.assertIn("invalid library", stderr)
                self.assertNotIn("Traceback", stderr)
                self.assertNotIn("no debt file", stderr)
                self.assertEqual(self.debt(), BASE)
                self.assertEqual((self.root / "README.md").read_text(encoding="utf-8"), readme)
                self.assertEqual((self.root / "docs/dev/debt/changes/pending.md").read_text(encoding="utf-8"), pending)
        code, stdout, stderr = self.run_fold("--show", "libSceMissing")
        self.assertEqual(code, 1)
        self.assertEqual(stdout, "")
        self.assertIn("no debt file for libSceMissing", stderr)

    def test_refuses_mixed_valid_and_malformed_fragments_atomically(self):
        readme = "# Example\n\n### Notes\n\n- [victim](core/example.cpp) - original\n"
        index = "# Project technical debt\n\n### Functional\n\n- [generalItem](core/example.cpp) - original\n"
        self.write("README.md", readme)
        self.write("docs/dev/TechnicalDebt.md", index)
        valid_frag = (
            f"# libSceExample\n\n## change [sceExampleA]\n\n"
            f"- [sceExampleA]({LINK}) (libSceExample) - updated\n\n"
            f"# TechnicalDebt\n\n## change [generalItem]\n\n"
            f"- [generalItem](core/example.cpp) - updated\n"
        )
        malformed_frag = "# ../../../README\n\n## change victim\n\n- [victim](core/example.cpp) - hijacked\n"
        self.fragment("a", valid_frag)
        self.fragment("b", malformed_frag)
        code, _, stderr = self.run_fold()
        self.assertEqual(code, 1)
        self.assertIn("invalid library", stderr)
        self.assertEqual(self.debt(), BASE)
        self.assertEqual((self.root / "README.md").read_text(encoding="utf-8"), readme)
        self.assertEqual((self.root / "docs/dev/TechnicalDebt.md").read_text(encoding="utf-8"), index)
        self.assertEqual((self.root / "docs/dev/debt/changes/a.md").read_text(encoding="utf-8"), valid_frag)
        self.assertEqual((self.root / "docs/dev/debt/changes/b.md").read_text(encoding="utf-8"), malformed_frag)
        (self.root / "docs/dev/debt/changes/a.md").unlink()
        (self.root / "docs/dev/debt/changes/b.md").unlink()
        mixed_frag = f"{valid_frag}\n{malformed_frag}"
        self.fragment("mixed", mixed_frag)
        code, _, stderr = self.run_fold()
        self.assertEqual(code, 1)
        self.assertIn("invalid library", stderr)
        self.assertEqual(self.debt(), BASE)
        self.assertEqual((self.root / "README.md").read_text(encoding="utf-8"), readme)
        self.assertEqual((self.root / "docs/dev/TechnicalDebt.md").read_text(encoding="utf-8"), index)
        self.assertEqual((self.root / "docs/dev/debt/changes/mixed.md").read_text(encoding="utf-8"), mixed_frag)


class SpaceTests(unittest.TestCase):
    def test_space_adds_blank_before_entries_after_text(self):
        import debt_fold
        self.assertEqual(debt_fold.space(["- a", "- b"]), ["- a", "", "- b"])
        self.assertEqual(debt_fold.space(["text", "- a"]), ["text", "", "- a"])
        self.assertEqual(debt_fold.space(["- a", "", "- b"]), ["- a", "", "- b"])


class SpacedTests(unittest.TestCase):
    def test_spaced_is_false_when_two_entries_touch(self):
        import debt_fold
        apart = [(0, 1, "- a"), (2, 3, "- b")]
        touching = [(0, 1, "- a"), (1, 2, "- b")]
        self.assertTrue(debt_fold.spaced(apart))
        self.assertFalse(debt_fold.spaced(touching))
class TargetTests(unittest.TestCase):
    def test_target_is_none_without_a_target_marker(self):
        import debt_fold
        self.assertIsNone(debt_fold.target("- plain entry with no target"))


class ParseFragmentTests(unittest.TestCase):
    def test_parse_refuses_a_library_with_no_debt_file(self):
        import debt_fold
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fragment = root / "fragment.md"
            fragment.write_text("# libMissing\n", encoding="utf-8")
            with self.assertRaisesRegex(debt_fold.FoldError, "no debt file for libMissing"):
                debt_fold.parse(fragment, root)

class UnitsTests(unittest.TestCase):
    def test_units_keeps_data_rows_and_skips_header_and_separator(self):
        text = "| Name | Value |\n| --- | --- |\n| x | y |\n"
        self.assertEqual(debt_fold.units(text), {3: "| x | y |"})

    def test_units_lists_bullet_entries_by_their_line(self):
        found = debt_fold.units(BASE_TEXT)
        self.assertEqual(sorted(found), [5, 6])
        self.assertTrue(found[5].startswith(f"- [sceExampleA]({LINK})"))



class FindTests(unittest.TestCase):
    def operation(self, target):
        return debt_fold.Operation("fragment.md", 9, "libSceExample", "change", target, "")

    def test_find_returns_the_span_of_the_one_matching_entry(self):
        self.assertEqual(debt_fold.find(debt_fold.spans(BASE_TEXT)[0], self.operation("sceExampleA")), (4, 5))

    def test_find_refuses_a_target_matching_several_entries(self):
        with self.assertRaisesRegex(debt_fold.FoldError, "matches 2 entries"):
            debt_fold.find(debt_fold.spans(BASE_TEXT)[0], self.operation("sceExample"))

    def test_find_refuses_a_target_matching_no_entry(self):
        with self.assertRaisesRegex(debt_fold.FoldError, "matches 0 entries"):
            debt_fold.find(debt_fold.spans(BASE_TEXT)[0], self.operation("sceMissing"))


if __name__ == "__main__":
    unittest.main()
