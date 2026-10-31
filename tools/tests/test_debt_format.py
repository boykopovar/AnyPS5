import io
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
from check_conventions import Check

TOOL = Path(__file__).resolve().parents[1] / "debt_format.py"
DEBT = "docs/dev/debt/libSceA.md"
OTHER = "docs/dev/debt/libSceB.md"
A = "- [sceAOne](../../../core/libs/prx/libSceA/Export.cpp) (libSceA) - returns 0"
B = "- [sceATwo](../../../core/libs/prx/libSceA/Export.cpp) (libSceA) - ignores its flags\n  - flag 1 is unknown"
C = "- [sceAThree](../../../core/libs/prx/libSceA/Export.cpp) (libSceA) - layout unknown"
D = "- [sceBOne](../../../core/libs/prx/libSceB/Export.cpp) (libSceB) - returns 0"
E = "- [sceBTwo](../../../core/libs/prx/libSceB/Export.cpp) (libSceB) - returns 1"


def debt(name, entries, separator="\n"):
    return f"# {name} technical debt\n\n### Functional\n\n" + separator.join(entries) + "\n"


class SpaceDebtTests(unittest.TestCase):
    def test_separates_top_level_entries_only(self):
        text = "# libSceA technical debt\n\n### Functional\n- one\n  - nested\n- two\nlazy line\n- three\n\n| a |\n|---|\n- four\n"
        self.assertEqual(check_conventions.space_debt(text),
                         "# libSceA technical debt\n\n### Functional\n\n- one\n  - nested\n\n- two\nlazy line\n\n- three\n\n| a |\n|---|\n\n- four\n")
        self.assertEqual(check_conventions.unspaced_debt(text), [4, 6, 8, 12])

    def test_is_idempotent_and_keeps_line_endings(self):
        spaced = check_conventions.space_debt(debt("libSceA", [A, B, C]))
        self.assertEqual(check_conventions.space_debt(spaced), spaced)
        self.assertEqual(check_conventions.unspaced_debt(spaced), [])
        crlf = "### Functional\r\n- one\r\n- two\r\n"
        self.assertEqual(check_conventions.space_debt(crlf), "### Functional\r\n\r\n- one\r\n\r\n- two\r\n")


class RepositoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.git("init", "-q", "-b", "main")

    def tearDown(self):
        self.directory.cleanup()

    def git(self, *args, check=True):
        return subprocess.run(
            ["git", "-c", f"safe.directory={self.root}", "-c", "user.name=Debt Test", "-c", "user.email=test@example.com",
             "-c", "commit.gpgsign=false", "-c", "maintenance.auto=false", *args],
            cwd=self.root, capture_output=True, text=True, check=check, env={**os.environ, "GIT_EDITOR": "true"})

    def write(self, path, text):
        (self.root / path).parent.mkdir(parents=True, exist_ok=True)
        (self.root / path).write_bytes(text.encode("utf-8"))

    def read(self, path):
        return (self.root / path).read_bytes().decode("utf-8")

    def commit(self, message):
        self.git("add", "-A")
        self.git("commit", "-qm", message)
        return self.git("rev-parse", "HEAD").stdout.strip()

    def tool(self, *args):
        return subprocess.run([sys.executable, str(TOOL), *args], cwd=self.root, capture_output=True, text=True, timeout=20)

    def findings(self, base, head):
        previous = Path.cwd()
        try:
            os.chdir(self.root)
            check = Check(base, head)
            check.debt_entries()
            with redirect_stdout(io.StringIO()):
                status = check_conventions.output(check, "example/repo")
        finally:
            os.chdir(previous)
        return [finding for finding in check.findings if finding[0] == "debt-spacing"], status

    def test_check_reports_entries_added_next_to_another(self):
        self.write(DEBT, debt("libSceA", [A, C]))
        self.write(OTHER, debt("libSceB", [D, E]))
        base = self.commit("docs: add debt")
        self.write(DEBT, debt("libSceA", [A, B, C]))
        self.write(OTHER, debt("libSceB", [D, E]) + "\nUnrelated note\n")
        head = self.commit("docs: add an entry")
        findings, status = self.findings(base, head)
        self.assertEqual(findings, [("debt-spacing", DEBT, 6, ""), ("debt-spacing", DEBT, 8, "")])
        self.assertEqual(status, 1)

    def test_check_accepts_separated_entries(self):
        self.write(DEBT, debt("libSceA", [A, C], "\n\n"))
        base = self.commit("docs: add debt")
        self.write(DEBT, debt("libSceA", [A, B, C], "\n\n"))
        head = self.commit("docs: add an entry")
        self.assertEqual(self.findings(base, head), ([], 0))

    def test_format_adds_blank_lines_and_check_lists_files(self):
        self.write(DEBT, debt("libSceA", [A, B, C]))
        self.write(OTHER, debt("libSceB", [D, E], "\n\n"))
        self.write("docs/dev/TechnicalDebt.md", "# Project technical debt\n\n### Build\n\n- one\n- two\n")
        self.write("docs/dev/OTHER.md", "- one\n- two\n")
        self.commit("docs: add debt")
        result = self.tool("--check")
        self.assertEqual((result.returncode, result.stdout.split()), (1, ["docs/dev/TechnicalDebt.md", DEBT]))
        self.assertEqual(self.git("status", "--porcelain").stdout, "")
        result = self.tool()
        self.assertEqual((result.returncode, result.stdout.split()), (0, ["docs/dev/TechnicalDebt.md", DEBT]))
        self.assertEqual(self.read(DEBT), debt("libSceA", [A, B, C], "\n\n"))
        self.assertEqual(self.read("docs/dev/OTHER.md"), "- one\n- two\n")
        self.assertEqual(self.tool("--check").returncode, 0)

    def rebase(self, base, change, main):
        self.write(DEBT, base)
        self.commit("docs: record debt")
        self.git("checkout", "-qb", "change")
        for path, text in change.items():
            self.write(path, text)
        self.commit("feat: change debt")
        self.git("checkout", "-q", "main")
        self.write(DEBT, main)
        self.commit("docs: separate debt entries")
        self.git("checkout", "-q", "change")
        result = self.git("rebase", "main", check=False)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return self.tool("resolve")

    def test_resolve_merges_an_insertion_between_separated_entries(self):
        result = self.rebase(debt("libSceA", [A, C]),
                             {DEBT: debt("libSceA", [A, B, C]), OTHER: debt("libSceB", [D, E])},
                             debt("libSceA", [A, C], "\n\n"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.split(), [DEBT, OTHER])
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        self.assertEqual(self.git("diff", "--name-only").stdout, "")
        self.git("rebase", "--continue")
        self.assertEqual(self.read(DEBT), debt("libSceA", [A, B, C], "\n\n"))
        self.assertEqual(self.read(OTHER), debt("libSceB", [D, E], "\n\n"))

    def test_resolve_leaves_a_real_conflict_marked(self):
        result = self.rebase(debt("libSceA", [A, C]),
                             {DEBT: debt("libSceA", [A, C + " on the PR side"])},
                             debt("libSceA", [A, C + " on main"], "\n\n"))
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"{DEBT}: 1 conflict(s)", result.stderr)
        text = self.read(DEBT)
        self.assertIn("<<<<<<< ours\n" + C + " on main\n", text)
        self.assertIn("=======\n" + C + " on the PR side\n>>>>>>> theirs\n", text)
        self.assertIn(A + "\n\n<<<<<<<", text)
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout.split(), [DEBT])

    def test_resolve_takes_main_where_the_change_only_renumbers_a_count_main_dropped(self):
        counted = "- [Pending exports](../../../core/libs/prx/libSceA/Export.cpp) - 12 named exports are listed in the 3.20 table"
        result = self.rebase(debt("libSceA", [A, counted]),
                             {DEBT: debt("libSceA", [A, counted.replace("12", "11")])},
                             debt("libSceA", [A, counted.replace("12 named", "these named")], "\n\n"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("rebase", "--continue")
        self.assertEqual(self.read(DEBT), debt("libSceA", [A, counted.replace("12 named", "these named")], "\n\n"))

    def test_resolve_merges_a_renumbering_with_words_main_added(self):
        counted = "- [Pending exports](../../../core/libs/prx/libSceA/Export.cpp) - 12 named exports are listed in the 3.20 table"
        result = self.rebase(debt("libSceA", [A, counted]),
                             {DEBT: debt("libSceA", [A, counted.replace("12", "11")])},
                             debt("libSceA", [A, counted + " and in 13.60"], "\n\n"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("rebase", "--continue")
        self.assertEqual(self.read(DEBT), debt("libSceA", [A, counted.replace("12", "11") + " and in 13.60"], "\n\n"))

    def test_resolve_keeps_a_count_both_sides_renumbered(self):
        counted = "- [Pending exports](../../../core/libs/prx/libSceA/Export.cpp) - 12 named exports are listed in the 3.20 table"
        result = self.rebase(debt("libSceA", [A, counted]),
                             {DEBT: debt("libSceA", [A, counted.replace("12", "11")])},
                             debt("libSceA", [A, counted.replace("12", "13")], "\n\n"))
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"{DEBT}: 1 conflict(s)", result.stderr)

    def test_resolve_keeps_a_sentence_both_sides_rewrote(self):
        result = self.rebase(debt("libSceA", [A, C]),
                             {DEBT: debt("libSceA", [A, C.replace("layout unknown", "layout from shadPS4")])},
                             debt("libSceA", [A, C.replace("layout unknown", "layout not measured")], "\n\n"))
        self.assertEqual(result.returncode, 1)

    def test_resolve_needs_a_conflict(self):
        self.write(DEBT, debt("libSceA", [A]))
        self.commit("docs: record debt")
        result = self.tool("resolve")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No technical debt file is in a conflict", result.stderr)


if __name__ == "__main__":
    unittest.main()
