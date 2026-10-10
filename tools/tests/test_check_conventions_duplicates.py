import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_conventions


class DuplicateExportTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.addCleanup(os.chdir, Path.cwd())
        self.root = Path(directory.name)
        os.chdir(self.root)
        self.git("init", "-q")
        self.git("config", "user.name", "Conventions Test")
        self.git("config", "user.email", "conventions@example.invalid")
        self.git("config", "commit.gpgsign", "false")

    def git(self, *args):
        return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout.strip()

    def commit(self, path, source):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(source, encoding="utf-8")
        self.git("add", path)
        self.git("commit", "-qm", "feat: add fixture")
        return self.git("rev-parse", "HEAD")

    def check(self, base):
        check = check_conventions.Check(base, "HEAD")
        check.code()
        return check.findings

    def test_duplicate_exports_in_another_library(self):
        source = "int APS5_VABI sceExample(int value) { return value + 1; }\n"
        base = self.commit("core/libs/prx/libSceA/Export.cpp", source)
        self.commit("core/libs/prx/libSceB/Export.cpp", source)
        self.assertEqual(self.check(base), [
            ("duplicate-export", "core/libs/prx/libSceB/Export.cpp", 1, "sceExample is also in libSceA")])

    def test_duplicate_exports_with_tabs(self):
        base = self.commit("core/libs/prx/libSceA/Export.cpp",
                           "int\tAPS5_VABI\tsceExample\t(int value) { return value + 1; }\n")
        self.commit("core/libs/prx/libSceB/Export.cpp",
                    "int APS5_VABI sceExample(int value) { return value + 2; }\n")
        self.assertEqual(self.check(base), [
            ("duplicate-export", "core/libs/prx/libSceB/Export.cpp", 1, "sceExample is also in libSceA")])

    def test_different_names_are_allowed(self):
        base = self.commit("core/libs/prx/libSceA/Export.cpp",
                           "int APS5_VABI sceExample(int value) { return value + 1; }\n")
        self.commit("core/libs/prx/libSceB/Export.cpp",
                    "int APS5_VABI sceOther(int value) { return value + 1; }\n")
        self.assertEqual(self.check(base), [])

    def test_changing_an_export_in_its_library_is_allowed(self):
        path = "core/libs/prx/libSceA/Export.cpp"
        base = self.commit(path, "int APS5_VABI sceExample(int value) { return value + 1; }\n")
        self.commit(path, "int APS5_VABI sceExample(int value) { return value + 2; }\n")
        self.assertEqual(self.check(base), [])

    def test_abi_macro_inside_an_identifier_is_not_an_export(self):
        base = self.commit("core/libs/prx/libSceA/Export.cpp",
                           "int OTHER_APS5_VABI sceExample(int value) { return value + 1; }\n")
        self.commit("core/libs/prx/libSceB/Export.cpp",
                    "int APS5_VABI sceExample(int value) { return value + 1; }\n")
        self.assertEqual(self.check(base), [])


if __name__ == "__main__":
    unittest.main()
