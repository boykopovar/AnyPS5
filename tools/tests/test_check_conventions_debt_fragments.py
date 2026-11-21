import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_conventions import Check

DEBT = "docs/dev/debt/libSceExample.md"
FRAGMENT = "docs/dev/debt/changes/example.md"
LINK = "../../../core/libs/prx/libSceExample/Export.cpp"
OTHER = "../../../core/libs/prx/libSceOther/Export.cpp"
BASE = (f"# libSceExample technical debt\n\n### Silent stubs\n\n"
        f"- [sceExampleA]({LINK}) (libSceExample) - returns 0\n"
        f"- [sceExampleB]({LINK}) (libSceExample) - returns 0 without reading its argument\n")
INDEX = "docs/dev/TechnicalDebt.md"
TOOL = "../../tools/example.py"
INDEX_BASE = ("# Project technical debt\n\n#### Fragments\n\n### Functional\n\n"
              f"- [Example tool]({TOOL}) does not handle every input\n")


class DebtFragmentTests(unittest.TestCase):
    def check(self, fragment):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                                       "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                                      cwd=root, capture_output=True, check=True, encoding="utf-8").stdout

            def write(name, text):
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_bytes(text.encode("utf-8"))

            git("init", "-q")
            git("config", "core.safecrlf", "false")
            write(DEBT, BASE)
            write(INDEX, INDEX_BASE)
            write("tools/example.py", "pass\n")
            for library in ("libSceExample", "libSceOther"):
                write(f"core/libs/prx/{library}/Export.cpp", "int x;\n")
            git("add", ".")
            git("commit", "-qm", "docs: add base")
            write(FRAGMENT, fragment)
            git("add", ".")
            git("commit", "-qm", "docs(debt): add a fragment")
            head = git("rev-parse", "HEAD").strip()
            base = git("rev-parse", "HEAD~1").strip()
            previous = Path.cwd()
            try:
                os.chdir(root)
                check = Check(base, head)
                check.docs()
                check.debt_fragments()
            finally:
                os.chdir(previous)
        return check.findings

    def test_a_fragment_that_applies_passes_with_links_relative_to_the_debt_files(self):
        self.assertEqual(self.check(f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleC]({LINK}) (libSceExample) - returns 1\n\n"
                                    "## remove [sceExampleA]\n"), [])

    def test_a_target_that_is_not_one_entry_is_an_error(self):
        findings = self.check("# libSceExample\n\n## remove returns 0\n")
        self.assertEqual([(rule, path, line) for rule, path, line, _ in findings], [("debt-fragment", FRAGMENT, 3)])
        self.assertIn("matches 2 entries", findings[0][3])

    def test_an_entry_for_another_library_is_in_the_wrong_file(self):
        findings = self.check(f"# libSceExample\n\n## add Silent stubs\n\n- [sceOtherA]({OTHER}) (libSceOther) - returns 0\n")
        self.assertEqual(findings, [("debt-file", FRAGMENT, 3, "docs/dev/debt/libSceOther.md")])

    def test_a_link_to_a_missing_file_is_reported(self):
        findings = self.check("# libSceExample\n\n## add Silent stubs\n\n- [sceExampleD](../../../core/libs/prx/libSceExample/Gone.cpp) (libSceExample) - throws\n")
        self.assertEqual(findings, [("doc-link", FRAGMENT, 5, "../../../core/libs/prx/libSceExample/Gone.cpp")])


    def test_a_technical_debt_fragment_resolves_links_from_the_index(self):
        self.assertEqual(self.check(f"# TechnicalDebt\n\n## change does not handle every input\n\n"
                                    f"- [Example tool]({TOOL}) rejects empty input\n"), [])

    def test_a_technical_debt_fragment_link_relative_to_the_library_files_is_reported(self):
        findings = self.check("# TechnicalDebt\n\n## change does not handle every input\n\n"
                              "- [Example tool](../../../tools/example.py) rejects empty input\n")
        self.assertEqual(findings, [("doc-link", FRAGMENT, 5, "../../../tools/example.py")])

    def test_each_section_of_a_fragment_resolves_links_from_its_own_debt_file(self):
        self.assertEqual(self.check(f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleC]({LINK}) (libSceExample) - returns 1\n\n"
                                    f"# TechnicalDebt\n\n## change does not handle every input\n\n"
                                    f"- [Example tool]({TOOL}) rejects empty input\n"), [])

    def test_a_technical_debt_fragment_resolves_anchors_in_the_index(self):
        findings = self.check(f"# TechnicalDebt\n\n## change does not handle every input\n\n"
                              f"- [Example tool]({TOOL}) rejects empty input, see [fragments](#fragments) "
                              f"and [missing](TechnicalDebt.md#missing)\n")
        self.assertEqual(findings, [("doc-anchor", FRAGMENT, 5, "TechnicalDebt.md#missing")])


    def test_a_library_fragment_resolves_anchors_in_its_debt_file(self):
        findings = self.check(f"# libSceExample\n\n## add Silent stubs\n\n- [sceExampleC]({LINK}) (libSceExample) - returns 1, "
                              "see [stubs](#silent-stubs) and [missing](#missing)\n")
        self.assertEqual(findings, [("doc-anchor", FRAGMENT, 5, "#missing")])


if __name__ == "__main__":
    unittest.main()
