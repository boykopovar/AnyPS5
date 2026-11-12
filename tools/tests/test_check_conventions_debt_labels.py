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
LABELLED = "- [sceLabelled](../../../core/libs/prx/libSceExample/Export.cpp) (libSceExample) - layout unknown. Labels: console, nightly."


class DebtLabelTests(unittest.TestCase):
    def test_trailer(self):
        self.assertEqual(check_conventions.debt_labels(LABELLED), ["console", "nightly"])
        self.assertEqual(check_conventions.debt_labels(SHORT), [])
        self.assertEqual(check_conventions.debt_labels("- [x](y) - z. Closes when: measured on a PS5. Labels: no-test"), ["no-test"])
        self.assertEqual(check_conventions.debt_labels("- [x](y) - gap. Labels:\n  - sub bullet text"), [])

    def test_unknown_label_warns_without_failing(self):
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
            write(f"# libSceExample technical debt\n\nAn entry may end with `Labels:` and labels such as these.\n\n    Labels: not-a-debt-label\n\n### Functional\n\n{SHORT}\n{LABELLED}\n  - more detail. Labels: consle\n")
            git("commit", "-qam", "docs(debt): label an entry")
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

        self.assertEqual(check.findings, [("debt-labels", DEBT, 10, "nightly"), ("debt-labels", DEBT, 11, "consle")])
        self.assertEqual(status, 0)


if __name__ == "__main__":
    unittest.main()
