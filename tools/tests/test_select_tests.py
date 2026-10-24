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
        root = Path(cls.directory.name) / "link"
        try:
            root.symlink_to("real", target_is_directory=True)
        except OSError as error:
            if getattr(error, "winerror", None) != 1314:
                raise
            import _winapi
            _winapi.CreateJunction(str(Path(cls.directory.name) / "real"), str(root))
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

    def test_changed_list_with_a_byte_order_mark_and_crlf_keeps_its_first_path(self):
        listed = self.build / "changed bom.txt"
        listed.write_bytes(b"\xef\xbb\xbfCMakeLists.txt\r\nalpha/alpha.h\r\n")
        output = self.tool("select", self.build, self.graph, listed)
        self.assertIn("CMakeLists.txt", output)
        self.assertFalse((self.build / "select.tests").exists())

    def test_missing_graph_runs_everything(self):
        self.assertIsNone(self.select("alpha/alpha.h", graph=self.build / "missing.json")[1])

    def test_test_missing_from_the_graph_runs(self):
        graph = json.loads(self.graph.read_text(encoding="utf-8"))
        del graph["tests"]["beta"]
        older = self.build / "older.json"
        older.write_text(json.dumps(graph), encoding="utf-8")
        self.assertEqual(self.select("alpha/alpha.h", graph=older)[1], {"alpha", "alpha_data", "beta", "beta_setup", "script"})

    def test_selected_target_skips_unselected_executables(self):
        subprocess.run(["ninja", "-C", str(self.build), "-t", "clean"], check=True, capture_output=True)
        self.select("alpha/main.c")
        subprocess.run(["ninja", "-C", str(self.build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
        built = {p.stem for p in self.build.rglob("*_tests*") if p.is_file() and os.access(p, os.X_OK)}
        subprocess.run(["cmake", "--build", str(self.build)], check=True, capture_output=True)
        self.assertEqual(built, {"alpha_tests"})

    def test_fresh_build_keeps_a_shared_selected_executable(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "fresh"
            compiler = shutil.which("gcc") or shutil.which("cc")
            subprocess.run(["cmake", "-S", str(self.source), "-B", str(build), "-G", "Ninja", f"-DCMAKE_C_COMPILER={compiler}"],
                           check=True, capture_output=True)
            changed = build / "changed.txt"
            changed.write_text("data/input.txt\n", encoding="utf-8")
            self.tool("select", build, self.graph, changed)
            subprocess.run(["ninja", "-C", str(build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
            built = {path.stem for path in build.rglob("*_tests*") if path.is_file() and os.access(path, os.X_OK)}
            self.assertEqual(built, {"alpha_tests"})
            output = self.tool("ctest", build, "--output-on-failure")
            self.assertEqual(set(re.findall(r"Test +#\d+: (\S+)", output)), {"alpha_data", "script"})

    def test_empty_selection_does_not_build_unselected_executables(self):
        cmake = self.source / "CMakeLists.txt"
        original = cmake.read_text(encoding="utf-8")
        try:
            cmake.write_text(original.replace('add_test(NAME script COMMAND ${CMAKE_COMMAND} -E true)\n', ''), encoding="utf-8")
            subprocess.run(["cmake", "-S", str(self.source), "-B", str(self.build)], check=True, capture_output=True)
            subprocess.run(["ninja", "-C", str(self.build), "-t", "clean"], check=True, capture_output=True)
            self.assertEqual(self.select("README.md")[1], set())
            subprocess.run(["ninja", "-C", str(self.build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
            self.assertFalse(any(path.is_file() and os.access(path, os.X_OK) for path in self.build.rglob("*_tests*")))
        finally:
            cmake.write_text(original, encoding="utf-8")
            subprocess.run(["cmake", "-S", str(self.source), "-B", str(self.build)], check=True, capture_output=True)
            subprocess.run(["cmake", "--build", str(self.build)], check=True, capture_output=True)

    def test_retargeted_test_does_not_prune_its_previous_executable(self):
        cmake = self.source / "alpha/CMakeLists.txt"
        original = cmake.read_text(encoding="utf-8")
        try:
            cmake.write_text(original.replace("add_executable(alpha_tests main.c)",
                                             "add_executable(alpha_tests main.c)\nadd_executable(replacement_tests main.c)")
                             .replace("COMMAND alpha_tests", "COMMAND replacement_tests"), encoding="utf-8")
            subprocess.run(["cmake", "-S", str(self.source), "-B", str(self.build)], check=True, capture_output=True)
            subprocess.run(["ninja", "-C", str(self.build), "-t", "clean"], check=True, capture_output=True)
            self.select("beta/main.c")
            subprocess.run(["ninja", "-C", str(self.build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
            built = {path.stem for path in self.build.rglob("*_tests*") if path.is_file() and os.access(path, os.X_OK)}
            self.assertEqual(built, {"alpha_tests", "beta_tests", "setup_tests"})
        finally:
            cmake.write_text(original, encoding="utf-8")
            subprocess.run(["cmake", "-S", str(self.source), "-B", str(self.build)], check=True, capture_output=True)
            subprocess.run(["cmake", "--build", str(self.build)], check=True, capture_output=True)

    def test_explain_reports_input_and_fixture_reasons_without_a_build_cache(self):
        changed = self.build / "explain-changed.txt"
        cache = self.build / "CMakeCache.txt"
        saved = self.build / "CMakeCache.saved"
        cache.rename(saved)
        try:
            for path in ("alpha/alpha.h", "beta/main.c", "CMakeLists.txt"):
                with self.subTest(path=path):
                    changed.write_text(path + "\n", encoding="utf-8")
                    report = json.loads(self.tool("explain", self.graph, changed))
                    picked, full = select_tests.select(json.loads(self.graph.read_text(encoding="utf-8")), [path])
                    self.assertEqual(report["tests"], sorted(picked) if picked is not None else None)
                    self.assertEqual(report["full_run_paths"], full)
                    if path == "alpha/alpha.h":
                        self.assertIn("input: alpha/alpha.h", report["reasons"]["alpha"])
                        self.assertIn("no build executable", report["reasons"]["script"])
                    elif path == "beta/main.c":
                        self.assertIn("fixture setup for: beta", report["reasons"]["beta_setup"])
        finally:
            saved.rename(cache)



@unittest.skipUnless(shutil.which("ninja") and (shutil.which("gcc") or shutil.which("cc")), "needs ninja and a C compiler")
class FixtureBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        root = Path(cls.directory.name)
        cls.source, cls.build, cls.graph = root / "source", root / "build", root / "graph.json"
        project = dict(PROJECT)
        project["CMakeLists.txt"] += "add_subdirectory(cleanup)\n"
        project["cleanup/CMakeLists.txt"] = (
            "add_executable(cleanup_tests main.c)\nadd_test(NAME beta_cleanup COMMAND cleanup_tests)\n"
            "set_tests_properties(beta_cleanup PROPERTIES FIXTURES_CLEANUP beta_fixture)\n")
        project["cleanup/main.c"] = ('#include <stdio.h>\nint main(void) { FILE *f = fopen("cleanup.ran", "w"); '
                                     'if (!f) return 1; return fclose(f) != 0; }\n')
        for name, content in project.items():
            (cls.source / name).parent.mkdir(parents=True, exist_ok=True)
            (cls.source / name).write_text(content, encoding="utf-8")
        compiler = shutil.which("gcc") or shutil.which("cc")
        subprocess.run(["cmake", "-S", str(cls.source), "-B", str(cls.build), "-G", "Ninja", f"-DCMAKE_C_COMPILER={compiler}"],
                       check=True, capture_output=True)
        subprocess.run(["cmake", "--build", str(cls.build)], check=True, capture_output=True)
        SelectTests.tool("dump", cls.build, cls.graph)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_selected(self, changed, graph):
        subprocess.run(["cmake", "--build", str(self.build)], check=True, capture_output=True)
        marker = self.build / "cleanup/cleanup.ran"
        marker.unlink(missing_ok=True)
        listed = self.build / "changed.txt"
        listed.write_text(changed + "\n", encoding="utf-8")
        subprocess.run(["ninja", "-C", str(self.build), "-t", "clean"], check=True, capture_output=True)
        SelectTests.tool("select", self.build, graph, listed)
        subprocess.run(["ninja", "-C", str(self.build), "-f", "select.ninja", "selected"], check=True, capture_output=True)
        result = subprocess.run([sys.executable, str(TOOL), "ctest", str(self.build), "--output-on-failure"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(marker.is_file(), "CTest did not execute the fixture cleanup")
        return result.stdout

    def test_cleanup_executable_is_built_when_ctest_adds_it(self):
        output = self.run_selected("beta/main.c", self.graph)
        self.assertIn("beta_cleanup", output)
        self.assertNotIn("alpha", output)
        self.assertFalse(any(path.is_file() for path in (self.build / "alpha").glob("alpha_tests*")))

    def test_new_test_gets_its_current_setup_and_cleanup(self):
        graph = json.loads(self.graph.read_text(encoding="utf-8"))
        del graph["tests"]["beta"]
        older = self.build / "older.json"
        older.write_text(json.dumps(graph), encoding="utf-8")
        output = self.run_selected("alpha/main.c", older)
        for name in ("alpha", "alpha_data", "beta", "beta_setup", "beta_cleanup"):
            self.assertIn(name, output)


class BuildErrorTests(unittest.TestCase):
    def run_tool(self, *args):
        return subprocess.run([sys.executable, "-I", str(TOOL), *map(str, args)], capture_output=True, text=True, timeout=60)

    def test_missing_build_directory_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for command in (("dump", root / "no build", root / "graph.json"),
                            ("select", root / "no build", root / "graph.json", root / "changed.txt")):
                with self.subTest(command[0]):
                    result = self.run_tool(*command)
                    self.assertEqual(result.returncode, 2)
                    self.assertIn("not a configured CMake build directory", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)

    def test_cache_without_the_build_locations_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "CMakeCache.txt").write_text("# empty\n", encoding="utf-8")
            result = self.run_tool("dump", directory, Path(directory) / "graph.json")
        self.assertEqual(result.returncode, 2)
        self.assertIn("CMAKE_CACHEFILE_DIR", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_cache_with_utf8_bom_is_read(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "CMakeCache.txt"
            cache.write_text("\ufeffCMAKE_CACHEFILE_DIR:INTERNAL=C:/work/build\nCMAKE_HOME_DIRECTORY:INTERNAL=C:/work/source\n", encoding="utf-8")
            with mock.patch.object(select_tests.Build, "run", return_value="Build file encoding: UTF-8"):
                build = select_tests.Build(directory)
                self.assertEqual(build.path, os.path.normpath("C:/work/build"))
                self.assertEqual(build.source, os.path.normpath("C:/work/source"))

    def test_explain_accepts_utf8_bom_in_graph_and_changed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            graph_path = root / "graph.json"
            graph = {
                "tests": {
                    "unit": {
                        "target": "tests/unit",
                        "arguments": [],
                        "registered": [],
                        "sets": [],
                        "requires": [],
                        "depends": [],
                    }
                },
                "closures": {"tests/unit": ["src/main.c"]},
            }
            graph_path.write_text("\ufeff" + json.dumps(graph), encoding="utf-8")
            changed_path = root / "changed.txt"
            changed_path.write_text("\ufeffsrc/main.c\n", encoding="utf-8")
            result = self.run_tool("explain", graph_path, changed_path)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = json.loads(result.stdout)
            self.assertEqual(data["tests"], ["unit"])


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

    def test_trailing_separator_in_root_is_handled(self):
        with mock.patch.object(select_tests.os.path, "normcase", str.lower):
            build = self.build()
            build.source = build.source + os.sep
            build.path = build.path + os.sep
            self.assertEqual(build.output(os.path.join(os.sep, "work", "source", "build", "tests", "MemoryPoolTest.exe")), "tests/MemoryPoolTest.exe")
            self.assertEqual(build.output(os.path.join("tests", "MemoryPoolTest.exe")), "tests/MemoryPoolTest.exe")
            self.assertEqual(build.repo(os.path.join(os.sep, "work", "source", "core", "libSceAgc", "Driver.cpp")), "core/libSceAgc/Driver.cpp")
            self.assertIsNone(build.repo(os.path.join(os.sep, "WORK", "SOURCE", "BUILD", "Generated.h")))


class ChunkTests(unittest.TestCase):
    def test_windows_batches_fit_the_serialized_utf16_budget(self):
        cases = (
            (20000, ["\U0001f600" * 50 + f"/{index}.obj" for index in range(400)]),
            (64, ['space name\\', 'embedded"quote', "\t", "", 'both \\" slash\\'] * 20),
            (64, ["\ud800" * 10, "\U0001f600" * 10] * 20),
        )
        for budget, names in cases:
            with self.subTest(budget=budget, first=names[0]), mock.patch.object(select_tests.sys, "platform", "win32"), \
                    mock.patch.object(select_tests, "BATCH", budget):
                batches = list(select_tests.chunks(iter(names)))
                self.assertEqual([name for batch in batches for name in batch], names)
                self.assertGreater(len(batches), 1)
                for batch in batches:
                    self.assertTrue(batch)
                    command = subprocess.list2cmdline(batch)
                    self.assertLessEqual(len(command.encode("utf-16-le", "surrogatepass")) // 2 + 1, budget)

    def test_windows_batch_keeps_an_exact_boundary(self):
        with mock.patch.object(select_tests.sys, "platform", "win32"), mock.patch.object(select_tests, "BATCH", 8):
            self.assertEqual(list(select_tests.chunks(["a b", "c", "d"])), [["a b", "c"], ["d"]])

    def test_other_platforms_keep_the_existing_character_budget(self):
        names = ["\U0001f600" * 3, "abc", "\udcff"]
        for platform in ("linux", "darwin"):
            with self.subTest(platform=platform), mock.patch.object(select_tests.sys, "platform", platform), \
                    mock.patch.object(select_tests, "BATCH", 8):
                self.assertEqual(list(select_tests.chunks(iter(names))), [names[:2], names[2:]])

    def test_empty_input_and_an_oversized_single_argument_are_preserved(self):
        for platform in ("win32", "linux"):
            with self.subTest(platform=platform), mock.patch.object(select_tests.sys, "platform", platform), \
                    mock.patch.object(select_tests, "BATCH", 4):
                self.assertEqual(list(select_tests.chunks(iter([]))), [])
                self.assertEqual(list(select_tests.chunks(["long target", "x"])), [["long target"], ["x"]])

    @unittest.skipUnless(sys.platform == "win32", "needs the Windows command line limit")
    def test_windows_batches_launch_without_losing_arguments(self):
        names = ["\U0001f600" * 50 + f"/{index}.obj" for index in range(400)]
        for batch in select_tests.chunks(names):
            result = subprocess.run([sys.executable, "-I", "-c", "import json,sys; print(json.dumps(sys.argv[1:]))", *batch],
                                    capture_output=True, text=True, check=True, timeout=5)
            self.assertEqual(json.loads(result.stdout), batch)


class PlanTests(unittest.TestCase):
    def test_unresolved_selected_command_keeps_unselected_build_targets(self):
        build = mock.Mock()
        listed = [{"name": "selected"}, {"name": "other", "command": ["/build/tests/shared_tests"]}]
        build.tests.side_effect = [({}, listed), ({}, [listed[0]])]
        build.leaves.return_value = ["tests/shared_tests", "helper"]
        build.target.side_effect = lambda test: "tests/shared_tests" if test.get("command") else None
        graph = {"tests": {"selected": {}, "other": {}}}
        self.assertEqual(select_tests.plan(build, graph, {"selected"}), ([1], ["tests/shared_tests", "helper"], 2))

    def test_plan_empty_run_avoids_invoking_ctest(self):
        build = mock.Mock()
        build.tests.return_value = ({}, [{"name": "alpha", "command": ["/build/alpha_tests"]}])
        build.leaves.return_value = ["tests/alpha_tests"]
        build.target.side_effect = lambda t: "tests/alpha_tests"
        graph = {"tests": {"alpha": {}}}
        run, leaves, count = select_tests.plan(build, graph, set())
        self.assertEqual(run, [])
        self.assertEqual(leaves, [])
        self.assertEqual(count, 1)
        build.tests.assert_called_once_with()


class MissingCommandTests(unittest.TestCase):
    def test_only_unambiguous_absolute_commands_in_reachable_files_are_recovered(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            active, stale = root / "active tests", root / "stale"
            active.mkdir()
            stale.mkdir()
            executable = (root / "tests/unit_tests").as_posix()
            helper = (root / "tests/helper_tests").as_posix()
            (root / "CTestTestfile.cmake").write_text('subdirs("active tests")\n', encoding="utf-8")
            (active / "CTestTestfile.cmake").write_text(
                f'add_test([=[absolute]=] "{executable}" "{helper}")\n'
                'add_test(relative tests/relative_tests)\n'
                'add_test(path unit_tests)\n'
                f'if(CONFIG)\nadd_test(ambiguous "{executable}")\nelse()\nadd_test(ambiguous "{helper}")\nendif()\n'
                f'add_test(existing "{executable}")\n'
                'subdirs("..")\n', encoding="utf-8")
            (stale / "CTestTestfile.cmake").write_text(f'add_test(stale "{executable}")\n', encoding="utf-8")
            names = ("absolute", "relative", "path", "ambiguous", "existing", "stale")
            listed = [{"name": name} for name in names]
            listed[4]["command"] = [helper]
            build = object.__new__(select_tests.Build)
            build.path = str(root)
            with mock.patch.object(select_tests.subprocess, "run", return_value=mock.Mock(stdout=json.dumps({"tests": listed}))):
                _, recovered = build.tests()
            commands = {test["name"]: test.get("command") for test in recovered}
            self.assertEqual(commands, {"absolute": [executable, helper], "relative": None, "path": None,
                                        "ambiguous": None, "existing": [helper], "stale": None})


    def test_main_reconfigures_streams(self):
        class Stream:
            def __init__(self):
                self.encoding = None
                self.errors = None

            def reconfigure(self, encoding=None, errors=None):
                self.encoding = encoding
                self.errors = errors

            def write(self, text):
                pass

            def flush(self):
                pass

        out, err = Stream(), Stream()
        with mock.patch.object(select_tests.sys, "stdout", out), mock.patch.object(select_tests.sys, "stderr", err):
            try:
                select_tests.main([])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
