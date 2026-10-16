import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOL = Path(__file__).resolve().parents[1] / "select_tests.py"
sys.path.insert(0, str(TOOL.parent))

import select_tests

PROJECT = {
    "CMakeLists.txt": "cmake_minimum_required(VERSION 3.20)\nproject(mini C)\nenable_testing()\n"
                      "add_subdirectory(alpha)\nadd_subdirectory(beta)\n"
                      "add_test(NAME script COMMAND ${CMAKE_COMMAND} -E true)\n",
    "alpha/CMakeLists.txt": "add_executable(alpha_tests main.c)\nadd_test(NAME alpha COMMAND alpha_tests)\n"
                            "add_test(NAME alpha_data COMMAND alpha_tests ${CMAKE_SOURCE_DIR}/data/input.txt)\n",
    "alpha/main.c": "#include \"alpha.h\"\nint main(void) { return ALPHA; }\n",
    "alpha/alpha.h": "#define ALPHA 0\n",
    "beta/CMakeLists.txt": "add_executable(beta_tests main.c)\nadd_executable(setup_tests setup.c)\n"
                           "add_test(NAME beta COMMAND beta_tests)\nadd_test(NAME beta_setup COMMAND setup_tests)\n"
                           "set_tests_properties(beta_setup PROPERTIES FIXTURES_SETUP beta_fixture)\n"
                           "set_tests_properties(beta PROPERTIES FIXTURES_REQUIRED beta_fixture)\n",
    "beta/main.c": "int main(void) { return 0; }\n",
    "beta/setup.c": "int main(void) { return 0; }\n",
    "data/input.txt": "input\n",
}


@unittest.skipUnless(shutil.which("ninja") and (shutil.which("gcc") or shutil.which("cc")), "needs ninja and a C compiler")
class SelectTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        (Path(cls.directory.name) / "real").mkdir()
        (Path(cls.directory.name) / "link").symlink_to("real", target_is_directory=True)
        root = Path(cls.directory.name) / "link"
        cls.source, cls.build, cls.graph = root / "source", root / "build", root / "graph.json"
        for name, content in PROJECT.items():
            (cls.source / name).parent.mkdir(parents=True, exist_ok=True)
            (cls.source / name).write_text(content, encoding="utf-8")
        compiler = shutil.which("gcc") or shutil.which("cc")
        subprocess.run(["cmake", "-S", str(cls.source), "-B", str(cls.build), "-G", "Ninja", f"-DCMAKE_C_COMPILER={compiler}"],
                       check=True, capture_output=True)
        subprocess.run(["cmake", "--build", str(cls.build)], check=True, capture_output=True)
        cls.tool("dump", cls.build, cls.graph)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    @staticmethod
    def tool(*args):
        return subprocess.run([sys.executable, str(TOOL), *map(str, args)], check=True, capture_output=True, text=True).stdout

    def select(self, *changed, graph=None):
        listed = self.build / "changed.txt"
        listed.write_text("".join(f"{c}\n" for c in changed), encoding="utf-8")
        output = self.tool("select", self.build, graph or self.graph, listed)
        tests = self.build / "select.tests"
        if not tests.exists():
            self.assertFalse((self.build / "select.ninja").exists())
            return output, None
        shown = subprocess.run(["ctest", "--test-dir", str(self.build), "-N", "-I", tests.read_text(encoding="utf-8").strip()],
                               check=True, capture_output=True, text=True).stdout
        return output, set(re.findall(r"Test +#\d+: (\S+)", shown))

    def test_header_selects_the_tests_of_its_executable(self):
        self.assertEqual(self.select("alpha/alpha.h")[1], {"alpha", "alpha_data", "script"})

    def test_argument_file_selects_only_the_test_that_reads_it(self):
        self.assertEqual(self.select("data/input.txt")[1], {"alpha_data", "script"})

    def test_fixture_setup_runs_with_its_test(self):
        self.assertEqual(self.select("beta/main.c")[1], {"beta", "beta_setup", "script"})
        self.assertEqual(self.select("beta/setup.c")[1], {"beta_setup", "script"})

    def test_cmake_file_selects_the_tests_it_registers(self):
        self.assertEqual(self.select("beta/CMakeLists.txt")[1], {"beta", "beta_setup", "script"})

    def test_file_outside_every_build_selects_its_directory(self):
        self.assertEqual(self.select("alpha/notes.h")[1], {"alpha", "alpha_data", "script"})
        self.assertEqual(self.select("README.md")[1], {"script"})

    def test_global_files_run_everything(self):
        for changed in ("CMakeLists.txt", "cmake/Toolchain.cmake", ".github/workflows/build.yml", "3rdparty/x/y.c",
                        "tools/select_tests.py"):
            with self.subTest(changed=changed):
                output, selected = self.select("alpha/alpha.h", changed)
                self.assertIsNone(selected)
                self.assertIn(changed, output)

    def test_missing_graph_runs_everything(self):
        self.assertIsNone(self.select("alpha/alpha.h", graph=self.build / "missing.json")[1])

    def test_test_missing_from_the_graph_runs(self):
        graph = json.loads(self.graph.read_text(encoding="utf-8"))
        del graph["tests"]["beta"]
        older = self.build / "older.json"
        older.write_text(json.dumps(graph), encoding="utf-8")
        self.assertEqual(self.select("alpha/alpha.h", graph=older)[1], {"alpha", "alpha_data", "beta", "beta_setup", "script"})

    def test_selected_target_skips_unselected_executables(self):
        self.select("alpha/main.c")
        subprocess.run(["ninja", "-C", str(self.build), "-t", "clean"], check=True, capture_output=True)
        subprocess.run(["ninja", "-C", str(self.build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
        built = {p.stem for p in self.build.rglob("*_tests*") if p.is_file() and os.access(p, os.X_OK)}
        subprocess.run(["cmake", "--build", str(self.build)], check=True, capture_output=True)
        self.assertEqual(built, {"alpha_tests"})


class CaseInsensitivePathTests(unittest.TestCase):
    def build(self):
        build = object.__new__(select_tests.Build)
        build.path, build.source = os.path.join(os.sep, "Work", "Source", "build"), os.path.join(os.sep, "Work", "Source")
        return build

    def test_paths_keep_their_case_when_comparisons_ignore_it(self):
        with mock.patch.object(select_tests.os.path, "normcase", str.lower):
            build = self.build()
            self.assertEqual(build.output(os.path.join(os.sep, "work", "source", "build", "tests", "MemoryPoolTest.exe")), "tests/MemoryPoolTest.exe")
            self.assertEqual(build.output(os.path.join("tests", "MemoryPoolTest.exe")), "tests/MemoryPoolTest.exe")
            self.assertEqual(build.repo(os.path.join(os.sep, "work", "source", "core", "libSceAgc", "Driver.cpp")), "core/libSceAgc/Driver.cpp")
            self.assertIsNone(build.repo(os.path.join(os.sep, "WORK", "SOURCE", "BUILD", "Generated.h")))
            self.assertEqual(build.key(os.path.join("tests", "MemoryPoolTest.exe")), build.key(os.path.join(os.sep, "WORK", "SOURCE", "BUILD", "TESTS", "memorypooltest.exe")))


if __name__ == "__main__":
    unittest.main()
