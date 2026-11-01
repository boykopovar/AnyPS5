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

DEBT = "docs/dev/debt/libSceExample.md"
SHORT = "- [sceShort](../../../core/libs/prx/libSceExample/Export.cpp) (libSceExample) - layout unknown"
LONG = "- [sceLong](../../../core/libs/prx/libSceExample/Export.cpp) (libSceExample) - " + "x" * 1000
PROSE = "Not an entry. " + "y" * 1000


class DebtLengthTests(unittest.TestCase):
    def test_long_debt_entry(self):
        self.assertFalse(check_conventions.long_debt_entry(SHORT))
        self.assertFalse(check_conventions.long_debt_entry("- " + "z" * (check_conventions.DEBT_ENTRY_LIMIT - 2)))
        self.assertTrue(check_conventions.long_debt_entry("- " + "z" * (check_conventions.DEBT_ENTRY_LIMIT - 1)))
        self.assertFalse(check_conventions.long_debt_entry(PROSE))

    def test_added_long_entry_warns_without_failing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def git(*args):
                return subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                                       "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                                      cwd=root, capture_output=True, check=True, encoding="utf-8").stdout

            def write(text):
                (root / DEBT).parent.mkdir(parents=True, exist_ok=True)
                (root / DEBT).write_bytes(text.encode("utf-8"))

            git("init", "-q")
            write(f"# libSceExample technical debt\n\n### Functional\n\n{SHORT}\n")
            git("add", ".")
            git("commit", "-qm", "docs: add base")
            write(f"# libSceExample technical debt\n\n{PROSE}\n\n### Functional\n\n{SHORT}\n{LONG}\n")
            git("commit", "-qam", "docs(debt): add a long entry")
            head = git("rev-parse", "HEAD").strip()
            base = git("rev-parse", "HEAD~1").strip()

            previous = Path.cwd()
            try:
                os.chdir(root)
                check = Check(base, head)
                check.debt_entries()
                with redirect_stdout(io.StringIO()):
                    status = check_conventions.output(check, "example/repo")
            finally:
                os.chdir(previous)

        self.assertEqual(check.findings, [("debt-length", DEBT, 8, f"{len(LONG)} characters")])
        self.assertEqual(status, 0)


if __name__ == "__main__":
    unittest.main()
