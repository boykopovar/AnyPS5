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


class FoldTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.write(DEBT, BASE)

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

    def test_add_creates_a_missing_section(self):
        self.fragment("a", f"# libSceExample\n\n## add Pending exports\n\n- [sceExampleF]({LINK}) (libSceExample) - throws\n")
        self.run_fold()
        self.assertTrue(self.debt().endswith(f"### Pending exports\n\n- [sceExampleF]({LINK}) (libSceExample) - throws\n"))

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
        self.assertIn("changes/b.md:3 and docs/dev/debt/changes/a.md:3 both remove the same libSceExample entry", error)
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


if __name__ == "__main__":
    unittest.main()
