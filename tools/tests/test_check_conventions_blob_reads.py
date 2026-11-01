import os
import subprocess
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions
from check_conventions import Check

EXPORT = "core/libs/prx/libSceExample/Export.cpp"
UNLISTED = "core/libs/prx/libSceExample/Unlisted.cpp"
EXPECTED = [
    ("binary", "data.txt", 0, ""),
    ("comment", EXPORT, 3, ""),
    ("duplicate-export", EXPORT, 2, "sceShared is also in libSceOther"),
    ("silent-stub", UNLISTED, 1, "sceUnlisted"),
]


def repository(root, fillers, exports=0):
    def git(*args):
        subprocess.run(["git", "-c", f"safe.directory={root}", "-c", "user.name=Conventions Test",
                        "-c", "user.email=test@example.com", "-c", "commit.gpgsign=false", *args],
                       cwd=root, capture_output=True, check=True)

    def write(path, data):
        target = root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data if isinstance(data, bytes) else data.encode("utf-8"))

    git("init", "-q")
    write("docs/dev/TechnicalDebt.md", "# Project technical debt\n")
    for index in range(fillers):
        write(f"docs/dev/debt/libSceFiller{index}.md", f"# libSceFiller{index} technical debt\n")
    write("docs/dev/debt/libSceExample.md",
          f"# libSceExample technical debt\n\n### Silent stubs\n\n- [Known](../../../{EXPORT}) - layout unknown.\n")
    write("core/libs/prx/libSceOther/src/Other.cpp", "int APS5_VABI sceShared() { return now(); }\n")
    if exports:
        names = [f"sceMany{index:04d}" + "LongExportName" * 6 for index in range(exports)]
        write("core/libs/prx/libSceOther/src/Many.cpp",
              f"int APS5_VABI {names[-1]}Extra() {{ return now(); }}\nint APS5_VABI {names[-1]}() {{ return now(); }}\n")
    write("removed.txt", "removed\n")
    git("add", ".")
    git("commit", "-qm", "docs: add base")
    git("tag", "base")
    write(EXPORT, "int APS5_VABI sceExample() { return 0; }\nint APS5_VABI sceShared() { return now(); }\nint value; // note\n")
    write(UNLISTED, "int APS5_VABI sceUnlisted() { return 0; }\n")
    if exports:
        write("core/libs/prx/libSceMany/Export.cpp", "".join(f"int APS5_VABI {name}() {{ return now(); }}\n" for name in names))
    write("data.txt", b"\xff\xfe not utf-8\n")
    write("docs/user/Café.md", "# Café\n")
    (root / "removed.txt").unlink()
    git("add", "-A")
    git("commit", "-qm", "feat: add example")


class BlobReadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.roots = {}
        for fillers in (2, 40):
            root = Path(cls.directory.name) / str(fillers)
            root.mkdir()
            repository(root, fillers)
            cls.roots[fillers] = root

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_checks(self, root):
        calls = Counter()
        run = subprocess.run

        def counting(args, *rest, **options):
            command = [str(arg) for arg in args]
            if command[0] == "git":
                index = 1
                while command[index] == "-c":
                    index += 2
                calls[command[index]] += 1
            return run(args, *rest, **options)

        previous = os.getcwd()
        os.chdir(root)
        try:
            with patch.object(check_conventions.subprocess, "run", counting):
                check = Check("base", "HEAD")
                check.code()
                check.build()
                check.repository()
                check.docs()
                check.workflows()
                check.commits()
        finally:
            os.chdir(previous)
        return sorted(check.findings), calls

    def test_findings_come_from_the_batched_contents(self):
        findings, _ = self.run_checks(self.roots[2])
        self.assertEqual(findings, EXPECTED)

    def test_contents_are_read_without_one_process_per_file(self):
        _, calls = self.run_checks(self.roots[2])
        self.assertEqual(calls["show"], 0, calls)
        self.assertLessEqual(calls["cat-file"], 2, calls)

    def test_process_count_does_not_grow_with_the_debt_files(self):
        few_findings, few = self.run_checks(self.roots[2])
        many_findings, many = self.run_checks(self.roots[40])
        self.assertEqual(many_findings, few_findings)
        self.assertEqual(many, few)

    def test_many_exports_keep_duplicate_detection(self):
        root = Path(self.directory.name) / "many-exports"
        root.mkdir()
        repository(root, 2, exports=400)
        findings, _ = self.run_checks(root)
        self.assertEqual(findings, sorted(EXPECTED + [
            ("duplicate-export", "core/libs/prx/libSceMany/Export.cpp", 400,
             "sceMany0399" + "LongExportName" * 6 + " is also in libSceOther")
        ]))


if __name__ == "__main__":
    unittest.main()
