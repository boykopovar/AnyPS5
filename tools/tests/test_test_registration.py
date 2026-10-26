from pathlib import Path
import contextlib
import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_test_registration


class RegistrationTests(unittest.TestCase):
    def test_bracket_arguments_ignore_only_one_initial_newline(self):
        values = (("path", "path"), ("\npath", "path"), ("\n\npath", "\npath"),
                  ("\n path", " path"), (" \npath", " \npath"), ("\\npath", "\\npath"),
                  ("\n", ""), ("\npath\n", "path\n"))
        for equals in ("", "=", "==="):
            for content, expected in values:
                with self.subTest(equals=equals, content=content):
                    command = f"subdirs([{equals}[{content}]{equals}])\n"
                    self.assertEqual(list(check_test_registration.commands(command)), [("subdirs", [expected])])
        self.assertEqual(list(check_test_registration.commands('subdirs("\npath")\n')),
                         [("subdirs", ["\npath"])])

    def test_bracket_initial_newlines_do_not_hide_disabled_tests(self):
        content = ('add_test([=[\nbounds]=] "/build/tests/buffer_bounds_tests")\n'
                   'set_tests_properties(bounds PROPERTIES DISABLED [=[\nTRUE]=])\n')
        self.assertNotIn("buffer_bounds_tests", self.registered(content))

    def test_bracket_initial_newline_in_subdirectory_is_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "nested").mkdir()
            (build / "CTestTestfile.cmake").write_text('subdirs([=[\nnested]=])\n', encoding="utf-8")
            (build / "nested/CTestTestfile.cmake").write_text(
                'add_test(bounds "/build/tests/buffer_bounds_tests")\n', encoding="utf-8")
            self.assertEqual(check_test_registration.registered(build), {"buffer_bounds_tests"})

    def test_literal_prefixes_cannot_expose_commands_inside_arguments(self):
        prefixes = (r"\#", r"\(", '"prefix" ', '"" ', '"line\nprefix" ', "[[prefix]] ",
                    "[=[prefix]=] ", "[===[line\nprefix]===] ")
        for prefix in prefixes:
            with self.subTest(prefix=prefix):
                content = (f"set(example\n{prefix}add_test(ghost /build/tests/missing_tests)\n)\n"
                           'add_test("real" "/build/tests/real_tests")\n')
                self.assertEqual(self.registered(content), {"real_tests"})

    def test_commands_inside_literals_do_not_register_tests(self):
        for opening, closing in (('"', '"'), ("[[", "]]"), ("[=[", "]=]"), ("[===[", "]===]")):
            with self.subTest(opening=opening):
                content = (f"set(example {opening}\nadd_test(ghost /build/tests/missing_tests)\n{closing})\n"
                           'add_test("real" "/build/tests/real_tests")\n')
                self.assertEqual(self.registered(content), {"real_tests"})

    def test_properties_inside_literals_do_not_disable_a_real_test(self):
        for opening, closing in (('"', '"'), ("[[", "]]"), ("[=[", "]=]"), ("[===[", "]===]")):
            with self.subTest(opening=opening):
                content = ('add_test("real" "/build/tests/real_tests")\n'
                           f"set(example {opening}\nset_tests_properties(real PROPERTIES DISABLED TRUE)\n{closing})\n")
                self.assertEqual(self.registered(content), {"real_tests"})

    def test_subdirectories_inside_literals_are_not_followed(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            ignored = build / "ignored"
            ignored.mkdir()
            (ignored / "CTestTestfile.cmake").write_text(
                'add_test("ghost" "/build/tests/missing_tests")\n', encoding="utf-8")
            for opening, closing in (('"', '"'), ("[[", "]]"), ("[=[", "]=]"), ("[===[", "]===]")):
                with self.subTest(opening=opening):
                    content = (f"set(example {opening}\nsubdirs(ignored)\n{closing})\n"
                               'add_test("real" "/build/tests/real_tests")\n')
                    (build / "CTestTestfile.cmake").write_text(content, encoding="utf-8")
                    self.assertEqual(check_test_registration.registered(build), {"real_tests"})

    def test_disabled_property_is_local_to_each_configuration(self):
        for configurations in (("OFF", "TRUE"), ("TRUE", "OFF"), ("TRUE", "TRUE")):
            with self.subTest(configurations=configurations):
                content = 'if(CTEST_CONFIGURATION_TYPE STREQUAL "Debug")\n'
                for index, disabled in enumerate(configurations):
                    if index:
                        content += 'else()\n'
                    content += f'add_test("bounds" "/build/tests/config{index}/buffer_bounds_tests")\n'
                    content += f'set_tests_properties("bounds" PROPERTIES DISABLED {disabled})\n'
                content += 'endif()\n'
                names = self.registered(content)
                self.assertEqual("buffer_bounds_tests" in names, "OFF" in configurations)


    def registered(self, content):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CTestTestfile.cmake").write_text(content, encoding="utf-8")
            return check_test_registration.registered(build)

    def test_bracket_comments_do_not_register_tests(self):
        for equals in ("", "=", "==="):
            with self.subTest(equals=equals):
                content = (f"#[{equals}[\nadd_test(ghost /build/tests/missing_tests)\n]{equals}]\n"
                           'add_test("real" "/build/tests/real_tests")\n')
                self.assertEqual(self.registered(content), {"real_tests"})

    def test_bracket_comment_does_not_disable_a_registered_test(self):
        content = ('add_test("real" "/build/tests/real_tests")\n'
                   '#[=[\nset_tests_properties("real" PROPERTIES DISABLED TRUE)\n]=]\n')
        self.assertEqual(self.registered(content), {"real_tests"})

    def test_bracket_comment_inside_arguments_does_not_end_the_command(self):
        content = ('add_test("real" #[=[ )\nadd_test(ghost /build/tests/missing_tests)\n]=]\n'
                   '"/build/tests/real_tests")\n')
        self.assertEqual(self.registered(content), {"real_tests"})

    def test_comment_markers_inside_literals_remain_arguments(self):
        content = ('add_test("real" "/build/tests/real_tests" "#[[" [=[# comment]=] "\\\\#")\n')
        self.assertEqual(list(check_test_registration.commands(content)),
                         [("add_test", ["real", "/build/tests/real_tests", "#[[", "# comment", "\\#"])])

    def test_test_name_does_not_register_executable(self):
        names = self.registered('add_test("missing_tests" "/usr/bin/python3" "unrelated.py")\n')
        self.assertNotIn("missing_tests", names)

    def test_direct_executable(self):
        names = self.registered('add_test("bounds" "/build/tests/buffer_bounds_tests")\n')
        self.assertIn("buffer_bounds_tests", names)
        self.assertNotIn("bounds", names)

    def test_wrapped_executable(self):
        names = self.registered('add_test("bounds" "/usr/bin/python3" "runner.py" "/build/tests/buffer_bounds_tests")\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_windows_executable_and_test_name(self):
        names = self.registered('add_test("missing_tests.exe" "C:/Program Files/Python/python.exe" "runner.py" "C:/build/tests/buffer_bounds_tests.exe")\n')
        self.assertIn("buffer_bounds_tests", names)
        self.assertNotIn("missing_tests", names)

    def test_hyphenated_executable_is_discovered(self):
        output = "tests/buffer-bounds_tests: CXX_EXECUTABLE_LINKER\n"
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CMakeCache.txt").write_text("CMAKE_MAKE_PROGRAM:FILEPATH=/configured/ninja\nCMAKE_GENERATOR:INTERNAL=Ninja\n", encoding="utf-8")
            with patch.object(check_test_registration.subprocess, "run", side_effect=lambda command, **kwargs: SimpleNamespace(
                    stdout="Build file encoding: UTF-8\n" if command[-1] == "wincodepage" else output)) as run:
                self.assertEqual(check_test_registration.built(build), {"buffer-bounds_tests"})
            self.assertEqual(run.call_args[0][0][0], "/configured/ninja")

    def test_nested_test_output_is_discovered(self):
        output = "tests/gpu/buffer_bounds_tests: CXX_EXECUTABLE_LINKER\n"
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CMakeCache.txt").write_text("CMAKE_MAKE_PROGRAM:FILEPATH=/configured/ninja\nCMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n", encoding="utf-8")
            with patch.object(check_test_registration.subprocess, "run", side_effect=lambda command, **kwargs: SimpleNamespace(
                    stdout="Build file encoding: UTF-8\n" if command[-1] == "wincodepage" else output)) as run:
                self.assertEqual(check_test_registration.built(build), {"buffer_bounds_tests"})
            self.assertEqual(run.call_args[0][0][0], "/configured/ninja")

    def test_quoted_cmake_path_is_unescaped(self):
        names = self.registered(r'add_test("bounds" "/build/tests/a\"b_tests")' + '\n')
        self.assertIn('a"b_tests', names)

    def test_bracket_quoted_command_is_recognized(self):
        names = self.registered('add_test([=[bounds]=] [=[/build/tests/buffer_bounds_tests]=])\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_multiline_test_command_is_recognized(self):
        names = self.registered('add_test("bounds"\n    "/build/tests/buffer_bounds_tests"\n)\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_multi_config_branches_register_the_executable(self):
        names = self.registered(
            'if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")\n'
            '  add_test([=[bounds]=] "/build/tests/Debug/buffer_bounds_tests")\n'
            '  set_tests_properties([=[bounds]=] PROPERTIES  TIMEOUT "20")\n'
            'elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")\n'
            '  add_test([=[bounds]=] "/build/tests/Release/buffer_bounds_tests")\n'
            '  set_tests_properties([=[bounds]=] PROPERTIES  TIMEOUT "20")\n'
            'else()\n'
            '  add_test([=[bounds]=] NOT_AVAILABLE)\n'
            'endif()\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_disabled_test_is_not_counted_as_run(self):
        names = self.registered('add_test("bounds" "/build/tests/buffer_bounds_tests")\nset_tests_properties("bounds" PROPERTIES DISABLED TRUE)\n')
        self.assertNotIn("buffer_bounds_tests", names)

    def test_option_value_does_not_register_executable(self):
        names = self.registered('add_test("bounds" "/usr/bin/python3" "runner.py" "--report=/build/tests/missing_tests")\n')
        self.assertNotIn("missing_tests", names)

    def test_reenabled_test_is_counted_as_run(self):
        names = self.registered('add_test("bounds" "/build/tests/buffer_bounds_tests")\nset_tests_properties("bounds" PROPERTIES DISABLED TRUE)\nset_tests_properties("bounds" PROPERTIES DISABLED OFF)\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_disabled_wrapper_does_not_register_its_argument(self):
        names = self.registered('add_test("bounds" "/usr/bin/python3" "runner.py" "/build/tests/buffer_bounds_tests")\nset_tests_properties([=[bounds]=] PROPERTIES DISABLED "ON")\n')
        self.assertNotIn("buffer_bounds_tests", names)

    def test_command_text_inside_an_argument_is_not_another_test(self):
        names = self.registered('add_test("bounds" "/usr/bin/python3" [=[\nadd_test("fake" "/build/tests/missing_tests")\n]=])\n')
        self.assertNotIn("missing_tests", names)

    def test_windows_escaped_path_is_decoded(self):
        names = self.registered(r'add_test("bounds" "C:\\build\\tests\\buffer_bounds_tests.exe")' + '\n')
        self.assertIn("buffer_bounds_tests", names)

    def test_unreferenced_registration_directory_does_not_count(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CTestTestfile.cmake").write_text("", encoding="utf-8")
            stale = build / "removed"
            stale.mkdir()
            (stale / "CTestTestfile.cmake").write_text(
                'add_test("bounds" "/build/tests/buffer_bounds_tests")\n', encoding="utf-8")
            self.assertNotIn("buffer_bounds_tests", check_test_registration.registered(build))
            (build / "CTestTestfile.cmake").unlink()
            self.assertNotIn("buffer_bounds_tests", check_test_registration.registered(build))

    def test_only_reachable_nested_directories_are_registered(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            active = build / "active tests"
            nested = active / "nested"
            removed = active / "removed"
            nested.mkdir(parents=True)
            removed.mkdir()
            (build / "CTestTestfile.cmake").write_text('subdirs("active tests")\n', encoding="utf-8")
            (active / "CTestTestfile.cmake").write_text('subdirs([=[nested]=])\n', encoding="utf-8")
            for path, name in ((nested, "active_tests"), (removed, "removed_tests")):
                (path / "CTestTestfile.cmake").write_text(
                    f'add_test("check" "/build/tests/{name}")\n', encoding="utf-8")
            self.assertEqual(check_test_registration.registered(build), {"active_tests"})

    def test_external_registration_directory_is_followed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, external = root / "build", root / "external"
            build.mkdir()
            external.mkdir()
            (build / "CTestTestfile.cmake").write_text(
                f'subdirs("{external.as_posix()}")\n', encoding="utf-8")
            (external / "CTestTestfile.cmake").write_text(
                'add_test("bounds" "/build/tests/buffer_bounds_tests")\n', encoding="utf-8")
            self.assertEqual(check_test_registration.registered(build), {"buffer_bounds_tests"})
    def test_unsupported_generator_never_runs_build_tool(self):
        for generator in ("Unix Makefiles", "Visual Studio 17 2022", ""):
            with self.subTest(generator=generator), tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                (build / "CMakeCache.txt").write_text("CMAKE_MAKE_PROGRAM:FILEPATH=make\nCMAKE_GENERATOR:INTERNAL=" + generator + "\n", encoding="utf-8")
                with patch.object(check_test_registration.subprocess, "run") as run:
                    with self.assertRaisesRegex(ValueError, "requires a Ninja generator"):
                        check_test_registration.built(build)
                    run.assert_not_called()

    def test_missing_make_program_never_runs_build_tool(self):
        for program in (None, "", "CMAKE_MAKE_PROGRAM-NOTFOUND"):
            with self.subTest(program=program), tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                cache = "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                if program is not None:
                    cache += "CMAKE_MAKE_PROGRAM:FILEPATH=" + program + "\n"
                (build / "CMakeCache.txt").write_text(cache, encoding="utf-8")
                with patch.object(check_test_registration.subprocess, "run") as run:
                    with self.assertRaises(FileNotFoundError):
                        check_test_registration.built(build)
                    run.assert_not_called()

    @unittest.skipIf(shutil.which("ninja") is None, "requires a local Ninja to exercise its real output encoding")
    def test_ninja_output_encoding_with_a_cp1252_default(self):
        program = shutil.which("ninja")
        actual_run = check_test_registration.subprocess.run
        for encoding in (("utf-8", "mbcs") if sys.platform == "win32" else ("utf-8",)):
            with self.subTest(encoding=encoding), tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                name = "caf\u00e9_tests".encode(encoding, errors="ignore").decode(encoding)
                (build / "CMakeCache.txt").write_text(
                    f"CMAKE_MAKE_PROGRAM:FILEPATH={program}\nCMAKE_GENERATOR:INTERNAL=Ninja\n", encoding="utf-8")
                (build / "build.ninja").write_text(
                    "rule CXX_EXECUTABLE_LINKER\n  command = true\n"
                    f"build tests/{name}: CXX_EXECUTABLE_LINKER\n", encoding=encoding)

                def run(command, **kwargs):
                    if command[-1] == "wincodepage":
                        return SimpleNamespace(stdout="Build file encoding: " + ("ANSI" if encoding == "mbcs" else "UTF-8"))
                    kwargs.setdefault("encoding", "cp1252")
                    return actual_run(command, **kwargs)

                with patch.object(check_test_registration.subprocess, "run", side_effect=run):
                    self.assertEqual(check_test_registration.built(build), {name})

    def test_unknown_windows_ninja_encoding_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "CMakeCache.txt").write_text(
                "CMAKE_MAKE_PROGRAM:FILEPATH=/configured/ninja\nCMAKE_GENERATOR:INTERNAL=Ninja\n", encoding="utf-8")
            with patch.object(check_test_registration.sys, "platform", "win32"), patch.object(
                    check_test_registration.subprocess, "run", return_value=SimpleNamespace(stdout="unknown")) as run:
                with self.assertRaisesRegex(ValueError, "unsupported Ninja build-file encoding"):
                    check_test_registration.built(build)
                run.assert_called_once()

    def run_tool(self, build):
        return subprocess.run([sys.executable, "-I", str(Path(check_test_registration.__file__)), str(build)],
                              capture_output=True, text=True, timeout=30)

    def test_missing_build_directory_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.run_tool(Path(directory) / "no build")
        self.assertEqual(result.returncode, 2)
        self.assertIn("error:", result.stderr)
        self.assertIn("CMakeCache.txt", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_unsupported_generator_is_reported_without_a_traceback(self):
        with tempfile.TemporaryDirectory() as directory:
            (Path(directory) / "CMakeCache.txt").write_text("CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n", encoding="utf-8")
            result = self.run_tool(directory)
        self.assertEqual(result.returncode, 2)
        self.assertIn("requires a Ninja generator", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

class ScriptRegistrationTests(unittest.TestCase):
    def unrun(self, files):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            check_test_registration.subprocess.run(["git", "init", "-q", str(root)], check=True)
            for name, content in files.items():
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text(content, encoding="utf-8")
            return check_test_registration.unrun_scripts(root)[1]

    def test_script_registered_by_a_nested_add_test_runs(self):
        self.assertEqual(self.unrun({
            "core/relinker/CMakeLists.txt": "if(WIN32)\n    add_test(NAME a COMMAND ${Python3_EXECUTABLE}\n        ${CMAKE_CURRENT_SOURCE_DIR}/relinker/tests/test_a.py)\nendif()\n",
            "core/relinker/relinker/tests/test_a.py": ""}), [])

    def test_script_in_bracket_argument_with_initial_newline_is_registered(self):
        self.assertEqual(self.unrun({
            "CMakeLists.txt": "add_test(NAME a COMMAND python [=[\ntools/tests/test_a.py]=])\n",
            "tools/tests/test_a.py": ""}), [])

    def test_unregistered_script_is_reported(self):
        self.assertEqual(self.unrun({
            "CMakeLists.txt": "add_test(NAME a COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_a.py)\n",
            "tools/tests/test_a.py": "", "tools/tests/test_b.py": "", "tools/tests/helper.py": ""}), ["tools/tests/test_b.py"])

    def test_commented_add_test_does_not_run_a_script(self):
        self.assertEqual(self.unrun({
            "CMakeLists.txt": "#add_test(NAME a COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/test_a.py)\n#[[\nadd_test(NAME a COMMAND python test_a.py)\n]]\n",
            "test_a.py": ""}), ["test_a.py"])

    def test_registration_names_its_directory(self):
        self.assertEqual(self.unrun({
            "CMakeLists.txt": "add_test(NAME a COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/a/tests/test_a.py)\n",
            "a/tests/test_a.py": "", "b/tests/test_a.py": ""}), ["b/tests/test_a.py"])

    def test_workflow_run_counts_and_a_paths_filter_does_not(self):
        self.assertEqual(self.unrun({
            ".github/workflows/pages.yml": "jobs:\n  web:\n    steps:\n      - run: python3 tools/test_web.py --build build-wasm\n",
            ".github/workflows/web.yml": "on:\n  pull_request:\n    paths:\n      - tools/test_paths.py\n",
            "tools/test_web.py": "", "tools/test_paths.py": ""}), ["tools/test_paths.py"])

    def test_workflow_script_inside_a_longer_name_does_not_run(self):
        self.assertEqual(self.unrun({
            ".github/workflows/build.yml": "jobs:\n  t:\n    steps:\n      - run: python3 tools/check_test_registration.py build\n"
                                           "      - run: python3 tools/run_test_a.py\n",
            "tools/check_test_registration.py": "", "tools/run_test_a.py": "",
            "tools/tests/test_registration.py": "", "tools/tests/test_a.py": ""}),
            ["tools/tests/test_a.py", "tools/tests/test_registration.py"])

    def test_workflow_script_with_a_longer_suffix_does_not_run(self):
        self.assertEqual(self.unrun({
            ".github/workflows/ci.yml": "jobs:\n  t:\n    steps:\n      - run: python3 tools/tests/test_a.pyc\n"
                                        "      - run: python3 tools/tests/test_b.py.bak\n"
                                        "      - run: python3 \"./tools/tests/test_c.py\" --quick\n",
            "tools/tests/test_a.py": "", "tools/tests/test_b.py": "", "tools/tests/test_c.py": ""}),
            ["tools/tests/test_a.py", "tools/tests/test_b.py"])

    def test_commented_workflow_line_does_not_run_a_script(self):
        self.assertEqual(self.unrun({
            ".github/workflows/ci.yml": "# python3 tools/tests/test_f.py\njobs:\n  t:\n    steps:\n      # python3 tools/tests/test_a.py\n"
                                        "      - run: |\n          #python3 tools/tests/test_b.py\n"
                                        "          \t# python3 tools/tests/test_c.py\n"
                                        "          echo done\t# python3 tools/tests/test_d.py\n"
                                        "          python3 tools/tests/test_e.py\n",
            "tools/tests/test_a.py": "", "tools/tests/test_b.py": "", "tools/tests/test_c.py": "",
            "tools/tests/test_d.py": "", "tools/tests/test_e.py": "", "tools/tests/test_f.py": ""}),
            ["tools/tests/test_a.py", "tools/tests/test_b.py", "tools/tests/test_c.py", "tools/tests/test_d.py",
             "tools/tests/test_f.py"])


class CheckRegistrationCliTests(unittest.TestCase):
    def test_cache_with_utf8_bom_is_read(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            cache = build / "CMakeCache.txt"
            cache.write_text("\ufeffCMAKE_MAKE_PROGRAM:FILEPATH=ninja\nCMAKE_GENERATOR:INTERNAL=Ninja\n", encoding="utf-8")
            output = "tests/dummy_tests: EXECUTABLE\n"
            with patch.object(check_test_registration.subprocess, "run", side_effect=lambda command, **kwargs: SimpleNamespace(
                    stdout="Build file encoding: UTF-8\n" if command[-1] == "wincodepage" else output)):
                names = check_test_registration.built(build)
                self.assertEqual(names, {"dummy_tests"})

    def test_main_scripts_with_explicit_argv(self):
        with patch.object(check_test_registration, "unrun_scripts", return_value=(["test_a.py"], [])):
            with contextlib.redirect_stdout(io.StringIO()):
                code = check_test_registration.main(["--scripts"])
            self.assertEqual(code, 0)

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
        with patch.object(check_test_registration.sys, "stdout", out), patch.object(check_test_registration.sys, "stderr", err):
            try:
                check_test_registration.main(["--invalid-argument"])
            except SystemExit:
                pass
        self.assertEqual(out.encoding, "utf-8")
        self.assertEqual(out.errors, "backslashreplace")
        self.assertEqual(err.encoding, "utf-8")
        self.assertEqual(err.errors, "backslashreplace")


if __name__ == "__main__":
    unittest.main()
