import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import debt_split

TOOL = Path(__file__).resolve().parents[1] / "debt_split.py"
INDEX = "docs/dev/TechnicalDebt.md"

A_STUB = "- [libSceA](../../core/libs/prx/libSceA/Export.cpp) - `sceAOne` returns 0"
A_FUNCTIONAL = "- `sceATwo` in [libSceA](../../core/libs/prx/libSceA/src/Two.cpp) ignores its flags\n  - flag 1 is unknown"
B_STUB = "- [libSceB](../../core/libs/prx/libSceB/Export.cpp) - `sceBOne` returns 0"
C_STUB = "- [libSceC](../../core/libs/prx/libSceC/Export.cpp) - `sceCOne` returns 0"
SHADER = "- [Recompiler](../../core/shader/recompiler/Lower.cpp) rounds once, pinned in [a test](../../core/libs/prx/libSceA/tests/Round.cpp)"
SHARED = "- A limitation of every library, see [architecture](ARCHITECTURE.md)"


def index(stubs, functional):
    return ("# Project technical debt\n\n### Build\n\n- A toolchain limitation\n\n### Silent stubs\n\n"
            "Everything throws, except:\n\n" + "\n".join(stubs) + "\n\n### Functional\n\n" + "\n".join(functional) + "\n")


ORIGINAL = index([A_STUB, B_STUB, C_STUB], [A_FUNCTIONAL, SHADER, SHARED])


class DebtSplitTests(unittest.TestCase):
    def test_split_preserves_root_relative_links(self):
        targets = ("/AnyPS5/guide.html#usage", "/images/example.png", "/")
        references = " ".join(f"[reference]({target})" for target in targets)
        self.write(INDEX, ORIGINAL.replace(A_STUB, A_STUB + " " + references))
        self.split()
        moved = self.read("docs/dev/debt/libSceA.md")
        for target in targets:
            with self.subTest(target=target):
                self.assertIn(f"[reference]({target})", moved)
        self.assertIn("../../../core/libs/prx/libSceA/Export.cpp", moved)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.git("init", "-q", "-b", "main")
        self.write(INDEX, ORIGINAL)
        self.git("add", ".")
        self.git("commit", "-qm", "docs: record technical debt")

    def tearDown(self):
        self.directory.cleanup()

    def git(self, *args, check=True):
        return subprocess.run(
            ["git", "-c", "user.name=Debt Test", "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
            cwd=self.root, capture_output=True, text=True, check=check,
            env={**os.environ, "GIT_EDITOR": "true"},
        )

    def write(self, path, text):
        (self.root / path).parent.mkdir(parents=True, exist_ok=True)
        (self.root / path).write_text(text, encoding="utf-8")

    def read(self, path):
        return (self.root / path).read_text(encoding="utf-8")

    def tool(self, command):
        return subprocess.run([sys.executable, str(TOOL), command], cwd=self.root, capture_output=True, text=True, timeout=20)

    def split(self):
        result = self.tool("split")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("add", "-A")
        self.git("commit", "-qm", "docs: split technical debt")

    def test_split_preserves_external_urls_and_rewrites_local_links(self):
        links = ["HTTPS://example.test/guide#intro", "//example.test/guide#intro",
                 "https://example.test/guide", "MAILTO:author@example.test"]
        references = " ".join(f"[reference]({target})" for target in links)
        self.write(INDEX, ORIGINAL.replace(A_STUB, A_STUB + " " + references
                   + " [local](ARCHITECTURE.md#layout)"))
        self.split()
        moved = self.read("docs/dev/debt/libSceA.md")
        for target in links:
            with self.subTest(target=target):
                self.assertIn(f"[reference]({target})", moved)
        self.assertIn("[local](../ARCHITECTURE.md#layout)", moved)
        self.assertIn("../../../core/libs/prx/libSceA/Export.cpp", moved)

    def test_split_moves_entries_by_their_first_core_link(self):
        self.split()
        self.assertEqual(self.read("docs/dev/debt/libSceA.md"),
                         "# libSceA technical debt\n\n### Silent stubs\n\n" + A_STUB.replace("../../", "../../../") +
                         "\n\n### Functional\n\n" + A_FUNCTIONAL.replace("../../", "../../../") + "\n")
        self.assertIn(SHADER.replace("../../", "../../../"), self.read("docs/dev/debt/shader.md"))
        text = self.read(INDEX)
        for entry in (A_STUB, B_STUB, C_STUB, SHADER):
            self.assertNotIn(entry.split(" - ")[0], text)
        for kept in ("- A toolchain limitation", "Everything throws, except:", SHARED, "### Silent stubs", "[debt](debt/)"):
            self.assertIn(kept, text)
        for path in (INDEX, "docs/dev/debt/libSceA.md"):
            self.assertNotIn(b"\r", (self.root / path).read_bytes())

    def test_split_again_appends_to_existing_files(self):
        self.split()
        moved = self.read("docs/dev/debt/libSceA.md")
        extra = "- [libSceA](../../core/libs/prx/libSceA/Three.cpp) - `sceAThree` returns 0"
        self.write(INDEX, self.read(INDEX).replace("Everything throws, except:\n", "Everything throws, except:\n\n" + extra))
        result = self.tool("split")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout.split(), ["docs/dev/debt/libSceA.md"])
        stub = A_STUB.replace("../../", "../../../")
        self.assertEqual(self.read("docs/dev/debt/libSceA.md"), moved.replace(stub, stub + "\n" + extra.replace("../../", "../../../")))
        self.assertEqual(self.read(INDEX).count("[debt](debt/)"), 1)

    def rebase(self, stubs, functional):
        self.git("checkout", "-qb", "change")
        self.write(INDEX, index(stubs, functional))
        self.git("commit", "-qam", "feat: change debt")
        self.git("checkout", "-q", "main")
        self.split()
        self.git("checkout", "-q", "change")
        rebase = self.git("rebase", "main", check=False)
        self.assertNotEqual(rebase.returncode, 0, rebase.stdout + rebase.stderr)
        return self.tool("resolve")

    def test_resolve_moves_added_edited_and_removed_entries(self):
        added = "- [libSceA](../../core/libs/prx/libSceA/Export.cpp) - `sceAFour` returns 0"
        edited = B_STUB + " and logs once"
        result = self.rebase([A_STUB, added, edited], [A_FUNCTIONAL, SHADER, SHARED])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(sorted(result.stdout.split()),
                         ["docs/dev/debt/libSceA.md", "docs/dev/debt/libSceB.md", "docs/dev/debt/libSceC.md"])
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        for path in (INDEX, "docs/dev/debt/libSceA.md"):
            self.assertNotIn(b"\r", (self.root / path).read_bytes())
        self.git("rebase", "--continue")
        a = self.read("docs/dev/debt/libSceA.md")
        self.assertLess(a.index("`sceAOne`"), a.index("`sceAFour`"))
        self.assertLess(a.index("`sceAFour`"), a.index("### Functional"))
        self.assertIn(edited.replace("../../", "../../../"), self.read("docs/dev/debt/libSceB.md"))
        self.assertFalse((self.root / "docs/dev/debt/libSceC.md").exists())
        self.assertEqual(self.read(INDEX), self.git("show", "main:" + INDEX).stdout)

    def test_resolve_moves_unchanged_entry_to_functional(self):
        result = self.rebase([A_STUB, B_STUB], [C_STUB, A_FUNCTIONAL, SHADER, SHARED])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        self.git("rebase", "--continue")
        self.assertEqual(self.read("docs/dev/debt/libSceC.md"),
                         "# libSceC technical debt\n\n### Functional\n\n" + C_STUB.replace("../../", "../../../") + "\n")
        self.assertEqual(result.stdout.split(), ["docs/dev/debt/libSceC.md"])
        self.assertEqual(self.read(INDEX), self.git("show", "main:" + INDEX).stdout)

    def test_resolve_moves_unchanged_entry_to_silent_stubs(self):
        result = self.rebase([A_STUB, B_STUB, C_STUB, A_FUNCTIONAL], [SHADER, SHARED])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.git("diff", "--name-only", "--diff-filter=U").stdout, "")
        self.git("rebase", "--continue")
        self.assertEqual(self.read("docs/dev/debt/libSceA.md"),
                         "# libSceA technical debt\n\n### Silent stubs\n\n" + A_STUB.replace("../../", "../../../") +
                         "\n" + A_FUNCTIONAL.replace("../../", "../../../") + "\n")
        self.assertEqual(result.stdout.split(), ["docs/dev/debt/libSceA.md"])
        self.assertEqual(self.read(INDEX), self.git("show", "main:" + INDEX).stdout)

    def test_resolve_refuses_changes_to_lines_that_stay_in_the_index(self):
        result = self.rebase([A_STUB, B_STUB, C_STUB], [A_FUNCTIONAL, SHADER, SHARED + " too"])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("resolve those by hand", result.stderr)
        self.assertIn("<<<<<<<", self.read(INDEX))

    def test_resolve_refuses_an_entry_main_changed_too(self):
        self.git("checkout", "-qb", "change")
        self.write(INDEX, index([A_STUB, B_STUB + " here", C_STUB], [A_FUNCTIONAL, SHADER, SHARED]))
        self.git("commit", "-qam", "feat: change b")
        self.git("checkout", "-q", "main")
        self.split()
        path = "docs/dev/debt/libSceB.md"
        self.write(path, self.read(path).replace("returns 0", "returns 1"))
        self.git("commit", "-qam", "docs: change b")
        self.git("checkout", "-q", "change")
        self.assertNotEqual(self.git("rebase", "main", check=False).returncode, 0)
        result = self.tool("resolve")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("entry not found", result.stderr)

    def test_resolve_needs_a_conflict(self):
        result = self.tool("resolve")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not in a three-way conflict", result.stderr)

    def test_resolve_preserves_entries_inserted_before_the_first_existing_entry(self):
        first = "- [libSceA](../../core/libs/prx/libSceA/First.cpp) - `sceAFirst` returns 0"
        second = "- [libSceA](../../core/libs/prx/libSceA/Second.cpp) - `sceASecond` returns 0"
        result = self.rebase([first, second, A_STUB, B_STUB, C_STUB], [A_FUNCTIONAL, SHADER, SHARED])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("rebase", "--continue")
        self.assertEqual(self.read("docs/dev/debt/libSceA.md"),
                         "# libSceA technical debt\n\n### Silent stubs\n\n" +
                         "\n".join(entry.replace("../../", "../../../") for entry in (first, second, A_STUB)) +
                         "\n\n### Functional\n\n" + A_FUNCTIONAL.replace("../../", "../../../") + "\n")
        self.assertEqual(self.read(INDEX), self.git("show", "main:" + INDEX).stdout)

    def test_resolve_places_an_entry_inserted_at_the_top_of_a_later_section(self):
        added = "- [libSceA](../../core/libs/prx/libSceA/Added.cpp) - `sceAAdded` returns 0"
        result = self.rebase([A_STUB, B_STUB, C_STUB], [added, A_FUNCTIONAL, SHADER, SHARED])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.git("rebase", "--continue")
        self.assertEqual(self.read("docs/dev/debt/libSceA.md"),
                         "# libSceA technical debt\n\n### Silent stubs\n\n" + A_STUB.replace("../../", "../../../") +
                         "\n\n### Functional\n\n" +
                         "\n".join(entry.replace("../../", "../../../") for entry in (added, A_FUNCTIONAL)) + "\n")

    def test_indented_continuation_after_a_blank_line_moves_with_its_entry(self):
        note = "  - sub note about sceAOne"
        self.write(INDEX, "# Project technical debt\n\n### Silent stubs\n\n" + A_STUB + "\n\n" + note + "\n")
        self.split()
        self.assertIn(note, self.read("docs/dev/debt/libSceA.md"))
        self.assertNotIn(note, self.read(INDEX))

    def test_unindented_paragraph_after_a_blank_line_stays_in_the_index(self):
        paragraph = "A plain paragraph after the stubs"
        self.write(INDEX, "# Project technical debt\n\n### Silent stubs\n\n" + A_STUB + "\n\n" + paragraph + "\n")
        self.split()
        self.assertIn(paragraph, self.read(INDEX))
        self.assertNotIn(paragraph, self.read("docs/dev/debt/libSceA.md"))

    def test_read_debt_and_split_accept_utf8_bom(self):
        self.write("docs/dev/debt/libSceA.md", "\ufeff### Silent stubs\n\n- entry 1\n")
        sections = debt_split.read_debt(self.root, "libSceA")
        self.assertEqual(sections, {"Silent stubs": ["- entry 1"]})
        self.write(INDEX, "\ufeff" + ORIGINAL)
        self.split()
        self.assertTrue((self.root / "docs/dev/debt/libSceA.md").is_file())
        self.assertIn("libSceA technical debt", self.read("docs/dev/debt/libSceA.md"))

    def test_split_keeps_blank_line_between_staying_entry_and_paragraph(self):
        paragraph = "A paragraph that belongs to no library."
        self.write(INDEX, "# Project technical debt\n\n### Silent stubs\n\n" + A_STUB
                          + "\n- A toolchain limitation\n\n" + paragraph + "\n")
        self.split()
        self.assertIn("- A toolchain limitation\n\n" + paragraph + "\n", self.read(INDEX))


if __name__ == "__main__":
    unittest.main()
