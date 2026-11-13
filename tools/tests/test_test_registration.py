import json
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


class RegistrationJsonTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "CMake and Ninja are required")
    def test_real_cmake_executable_report(self):
        import json
        import shutil
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests").mkdir()
            (root / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(fixture CXX)\nenable_testing()\nadd_subdirectory(tests)\n')
            (root / "tests/CMakeLists.txt").write_text('add_executable(covered main.cpp)\nadd_executable(lost main.cpp)\nadd_test(NAME covered COMMAND covered)\n')
            (root / "tests/main.cpp").write_text('int main() { return 0; }\n')
            build = root / "build"
            subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja"], check=True, capture_output=True)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(check_test_registration.main([str(build), "--json"]), 1)
            data = json.loads(output.getvalue())
            self.assertEqual(data["registered"], ["covered"])
            self.assertEqual(data["new_unregistered"], ["lost"])
            with patch.object(check_test_registration, "KNOWN_UNRUN", {"lost"}), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(check_test_registration.main([str(build), "--json"]), 0)

    def test_real_git_script_report(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", directory], check=True)
            (root / "tools").mkdir()
            (root / "tools/test_a.py").write_text('pass\n')
            (root / "tools/test_b.py").write_text('pass\n')
            (root / "CMakeLists.txt").write_text('add_test(NAME a COMMAND python tools/test_a.py)\n')
            output = io.StringIO()
            with patch.object(check_test_registration, "__file__", str(root / "tools/tool.py")), contextlib.redirect_stdout(output):
                self.assertEqual(check_test_registration.main(["--scripts", "--json"]), 1)
            data = json.loads(output.getvalue())
            self.assertEqual(data["registered"], ["tools/test_a.py"])
            self.assertEqual(data, {"mode": "scripts", "tests": ["tools/test_a.py", "tools/test_b.py"],
                                    "registered": ["tools/test_a.py"], "unregistered": ["tools/test_b.py"]})

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "CMake and Ninja are required")
    def test_real_cmake_report_matches_every_text_mode_decision(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests").mkdir()
            (root / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.20)\nproject(fixture CXX)\nenable_testing()\nadd_subdirectory(tests)\n')
            (root / "tests/CMakeLists.txt").write_text(
                "".join(f"add_executable({name} main.cpp)\n" for name in ("covered", "lost", "kept", "fixed"))
                + "add_test(NAME covered COMMAND covered)\nadd_test(NAME fixed COMMAND fixed)\n"
                + "add_test(NAME external COMMAND ${CMAKE_COMMAND} -E true)\n")
            (root / "tests/main.cpp").write_text('int main() { return 0; }\n')
            build = root / "build"
            subprocess.run(["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja"], check=True, capture_output=True)
            cases = ((set(), 1, [], ["kept", "lost"]), ({"kept", "lost"}, 0, [], []), ({"kept", "lost", "fixed"}, 1, ["fixed"], []))
            for known, code, stale, fresh in cases:
                with self.subTest(known=sorted(known)), patch.object(check_test_registration, "KNOWN_UNRUN", known):
                    output = io.StringIO()
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(check_test_registration.main([str(build)]), code)
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(check_test_registration.main([str(build), "--json"]), code)
                    self.assertEqual(json.loads(output.getvalue()), {
                        "mode": "executables", "tests": ["covered", "fixed", "kept", "lost"],
                        "registered": ["covered", "fixed"], "unregistered": ["kept", "lost"],
                        "known_unrun": sorted({"kept", "lost"} & known), "stale_exceptions": stale,
                        "new_unregistered": fresh, "unbuilt_exports": None})
            library = root / "core/libs/prx/libProbe"
            library.mkdir(parents=True)
            (library / "built.cpp").write_text("int built() { return 0; }\n")
            (library / "omitted.cpp").write_text("#define APS5_VABI\nint APS5_VABI omitted() { return 0; }\n")
            cmake = root / "CMakeLists.txt"
            with cmake.open("a") as stream:
                stream.write("add_library(probe STATIC core/libs/prx/libProbe/built.cpp)\n")
            subprocess.run(["cmake", "-S", str(root), "-B", str(build)], check=True, capture_output=True)
            with patch.object(check_test_registration, "__file__", str(root / "tools/tool.py")), \
                    patch.object(check_test_registration, "KNOWN_UNRUN", {"kept", "lost"}):
                text, output = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(text):
                    self.assertEqual(check_test_registration.main([str(build)]), 1)
                with contextlib.redirect_stdout(output):
                    self.assertEqual(check_test_registration.main([str(build), "--json"]), 1)
                omitted = "core/libs/prx/libProbe/omitted.cpp"
                self.assertIn(omitted, text.getvalue())
                self.assertEqual(json.loads(output.getvalue())["unbuilt_exports"], [omitted])
            (root / "tests/CMakeLists.txt").write_text("")
            subprocess.run(["cmake", "-S", str(root), "-B", str(build)], check=True, capture_output=True)
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(check_test_registration.main([str(build), "--json"]), 1)
            self.assertEqual(json.loads(output.getvalue())["tests"], [])


class RegistrationTests(unittest.TestCase):
    def test_truncated_registration_commands_are_rejected(self):
        for tail in ('set_tests_properties(bounds PROPERTIES DISABLED TRUE',
                     'set_tests_properties(bounds PROPERTIES DISABLED TRUE # truncated',
                     'subdirs(nested', 'add_test(other /build/tests/other_tests'):
            with self.subTest(tail=tail), tempfile.TemporaryDirectory() as directory:
                build = Path(directory)
                (build / 'CTestTestfile.cmake').write_text(
                    'add_test(bounds /build/tests/buffer_bounds_tests)\n' + tail, encoding='utf-8')
                with self.assertRaisesRegex(ValueError, 'unterminated'):
                    check_test_registration.registered(build)

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for the argument oracle")
    def test_quoted_semicolons_match_real_cmake_arguments(self):
        values = ["left" + "\\" * count + ";right" for count in range(9)]
        values += [r"\;", r"a\;b\;c", r"a;\;b", "a\\\n\\;b", "a\\\r\n\\;b", "\u00e9\\;tail"]
        commands = [f'add_test("case{index}" "{value}")\n' for index, value in enumerate(values)]
        commands += ['add_test("bracket" [=[a\\;b]=])\n']
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "oracle.cmake"
            script.write_bytes(('cmake_minimum_required(VERSION 3.20)\n'
                                'function(add_test name value)\n'
                                'string(HEX "${value}" encoded)\n'
                                'file(WRITE "${CMAKE_CURRENT_LIST_DIR}/${name}.hex" "${encoded}")\n'
                                'endfunction()\n' + "".join(commands)).encode("utf-8"))
            subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, check=True)
            for content in commands:
                command, arguments = list(check_test_registration.commands(content))[0]
                with self.subTest(name=arguments[0]):
                    actual = bytes.fromhex((root / f"{arguments[0]}.hex").read_text(encoding="ascii")).decode("utf-8")
                    self.assertEqual((command, arguments), ("add_test", [arguments[0], actual]))

    @unittest.skipUnless(shutil.which("cmake"), "CMake is required for the argument oracle")
    def test_quoted_continuations_follow_cmake(self):
        for ending, backslashes in (("\n", 1), ("\r\n", 1), ("\n", 2), ("\n", 3), ("\n", 4)):
            with self.subTest(ending=repr(ending), backslashes=backslashes), tempfile.TemporaryDirectory() as directory:
                content = 'add_test("real" "tools/tests/test_' + '\\' * backslashes + ending + 'bounds.py")\n'
                script = Path(directory) / "oracle.cmake"
                script.write_bytes(('function(add_test)\n'
                                    'file(WRITE "${CMAKE_CURRENT_LIST_DIR}/argument.txt" "${ARGV1}")\n'
                                    'endfunction()\n' + content).encode())
                subprocess.run(["cmake", "-P", str(script)], capture_output=True, text=True, check=True)
                actual = (Path(directory) / "argument.txt").read_text()
                expected = "tools/tests/test_" + '\\' * (backslashes // 2) + ("\n" if backslashes % 2 == 0 else "") + "bounds.py"
                self.assertEqual(actual, expected)
                self.assertEqual(list(check_test_registration.commands(content)), [("add_test", ["real", actual])])
                if backslashes == 1:
                    (Path(directory) / "CMakeLists.txt").write_bytes(content.encode())
                    self.assertEqual(check_test_registration.run_scripts(Path(directory), ["CMakeLists.txt"]), {actual})

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest") and shutil.which("ninja"),
                         "CMake, CTest and Ninja are required")
    def test_bom_bearing_included_cmake_matches_cmake_and_ctest_oracle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\n'
                'project(oracle NONE)\n'
                'enable_testing()\n'
                f'set(Python3_EXECUTABLE [=[{sys.executable}]=])\n'
                'include(RegisterTests.cmake)\n',
                encoding="utf-8")
            (root / "RegisterTests.cmake").write_bytes(
                'add_test(NAME example COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_example.py")\n'
                .encode("utf-8-sig"))
            (root / "tools/tests").mkdir(parents=True)
            (root / "tools/tests/test_example.py").write_text("import sys\nsys.exit(0)\n", encoding="utf-8")
            subprocess.run(["cmake", "-S", str(root), "-B", str(root / "build"), "-G", "Ninja"],
                           capture_output=True, text=True, check=True)
            ctest = subprocess.run(["ctest", "--test-dir", str(root / "build"), "-V"],
                                   capture_output=True, text=True, check=True)
            self.assertIn("1/1 Test #1: example", ctest.stdout)
            self.assertEqual(
                check_test_registration.run_scripts(root, ["CMakeLists.txt", "RegisterTests.cmake"]),
                {"tools/tests/test_example.py"})

    def test_commands_recognizes_first_command_with_initial_utf8_bom(self):
        cases = (
            ('\ufeffadd_test("case" "\ufeffvalue" [=[\ufeffbracket]=])\n',
             [("add_test", ["case", "\ufeffvalue", "\ufeffbracket"])]),
            ('\ufeffsubdirs("nested" [=[\ufeffpath]=])\n',
             [("subdirs", ["nested", "\ufeffpath"])]),
            ('\ufeffset_tests_properties("case" PROPERTIES DISABLED TRUE)\n',
             [("set_tests_properties", ["case", "PROPERTIES", "DISABLED", "TRUE"])]),
        )
        for content, expected in cases:
            with self.subTest(command=expected[0][0]):
                self.assertEqual(list(check_test_registration.commands(content)), expected)

    def test_commands_preserves_utf8_bom_inside_literals(self):
        content = 'add_test("case" "\ufeffvalue" [=[\ufeffbracket]=])\n'
        self.assertEqual(list(check_test_registration.commands(content)),
                         [("add_test", ["case", "\ufeffvalue", "\ufeffbracket"])])

    def test_literal_escaped_newline_and_bracket_continuation_are_preserved(self):
        quoted = r'add_test("real" "a\nb")' + '\n'
        bracket = 'add_test("real" [=[a' + '\\' + '\n' + 'b]=])\n'
        self.assertEqual(list(check_test_registration.commands(quoted)), [("add_test", ["real", "a\nb"])])
        self.assertEqual(list(check_test_registration.commands(bracket)), [("add_test", ["real", "a\\\nb"])])

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

    def test_looping_subdirectory_is_skipped(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory).resolve()
            loop = build / "first"
            (build / "nested").mkdir()
            (build / "CTestTestfile.cmake").write_text('subdirs("nested" "first")\n', encoding="utf-8")
            (build / "nested/CTestTestfile.cmake").write_text(
                'add_test(bounds "/build/tests/buffer_bounds_tests")\n', encoding="utf-8")
            original_resolve = Path.resolve

            def resolve(path):
                if path == loop:
                    raise RuntimeError("Symlink loop")
                return original_resolve(path)

            for root, expected in ((build, {"buffer_bounds_tests"}), (loop, set())):
                with self.subTest(root=root), patch.object(Path, "resolve", autospec=True, side_effect=resolve) as resolving:
                    self.assertEqual(check_test_registration.registered(root), expected)
                    resolving.assert_any_call(loop)

    def test_real_looping_subdirectory_is_skipped_where_supported(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "nested").mkdir()
            try:
                os.symlink("second", build / "first", target_is_directory=True)
                os.symlink("first", build / "second", target_is_directory=True)
            except NotImplementedError as error:
                self.skipTest(f"symbolic links are unavailable: {error}")
            except OSError as error:
                if os.name == "nt" and getattr(error, "winerror", None) == 1314:
                    self.skipTest(f"symbolic link privilege is unavailable: {error}")
                raise
            (build / "CTestTestfile.cmake").write_text('subdirs("nested" "first")\n', encoding="utf-8")
            (build / "nested/CTestTestfile.cmake").write_text(
                'add_test(bounds "/build/tests/buffer_bounds_tests")\n', encoding="utf-8")
            self.assertEqual(check_test_registration.registered(build), {"buffer_bounds_tests"})
            self.assertEqual(check_test_registration.registered(build / "first"), set())

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
                              capture_output=True, encoding="utf-8", timeout=30)

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

    def test_cmake_test_names_do_not_register_scripts(self):
        for command in ('add_test(NAME test_a.py COMMAND python helper.py)',
                        'add_test(test_a.py python helper.py)'):
            with self.subTest(command=command):
                self.assertEqual(self.unrun({"CMakeLists.txt": command, "test_a.py": ""}), ["test_a.py"])

    def test_cmake_test_options_do_not_register_scripts(self):
        for option in ("CONFIGURATIONS", "WORKING_DIRECTORY", "BUILD_DEPENDS"):
            for command in (f'add_test(NAME a {option} test_a.py COMMAND python helper.py)',
                            f'add_test(NAME a COMMAND python helper.py {option} test_a.py)'):
                with self.subTest(command=command):
                    self.assertEqual(self.unrun({"CMakeLists.txt": command, "test_a.py": ""}), ["test_a.py"])

    def test_cmake_command_scripts_remain_registered(self):
        commands = ('add_test(a python test_a.py)',
                    'add_test(NAME a CONFIGURATIONS Debug COMMAND python test_a.py)',
                    'add_test(NAME a WORKING_DIRECTORY work COMMAND python test_a.py)',
                    'add_test(NAME a COMMAND python test_a.py CONFIGURATIONS Debug)',
                    'add_test(NAME a COMMAND python test_a.py COMMAND_EXPAND_LISTS)',
                    'add_test(NAME a COMMAND test_a.py WORKING_DIRECTORY work)')
        for command in commands:
            with self.subTest(command=command):
                self.assertEqual(self.unrun({"CMakeLists.txt": command, "test_a.py": ""}), [])

    def test_script_in_bracket_argument_with_initial_newline_is_registered(self):
        self.assertEqual(self.unrun({
            "CMakeLists.txt": "add_test(NAME a COMMAND python [=[\ntools/tests/test_a.py]=])\n",
            "tools/tests/test_a.py": ""}), [])

    def test_script_registered_by_bom_bearing_cmake_module_runs(self):
        self.assertEqual(self.unrun({
            "cmake/RegisterTests.cmake": "\ufeffadd_test(NAME example COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_example.py)\n",
            "tools/tests/test_example.py": ""}), [])

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

    def test_workflow_metadata_does_not_register_scripts(self):
        script = "tools/tests/test_python_probe.py"
        job = "jobs:\n  t:\n    runs-on: ubuntu-latest\n    steps:\n"
        echo = "      - run: echo done\n"
        workflows = (
            f"name: python3 {script}\n" + job + echo,
            f"env:\n  DESCRIPTION: python3 {script}\n" + job + echo,
            job + f"      - name: python3 {script}\n        run: echo done\n",
            job + f"      - env:\n          DESCRIPTION: python3 {script}\n        run: echo done\n",
            job + f"      - env:\n          run: python3 {script}\n        run: echo done\n",
            job + f"      - uses: ./{script}\n",
            job.replace("    steps:\n", "    defaults:\n      run:\n        shell: bash\n"
                        f"        working-directory: {script}\n    steps:\n") + echo,
            f"env:\n  DESCRIPTION: |\n    jobs:\n      fake:\n        steps:\n          - run: python3 {script}\n" + job + echo,
        )
        for workflow in workflows:
            with self.subTest(workflow=workflow):
                self.assertEqual(self.unrun({".github/workflows/ci.yml": workflow, script: ""}), [script])

    def test_workflow_run_scalars_with_defaults_register_scripts(self):
        header = "jobs:\n  t:\n    defaults:\n      run:\n        shell: bash\n    steps:\n"
        steps = ("      - run: python3 tools/tests/test_inline.py\n"
                 "      - name: named step\n        shell: bash\n        run: |\n"
                 "          python3 \"./tools/tests/test_block.py\"\n"
                 "      - shell: bash\n        run: 'python tools/tests/test_named.py'\n")
        for body in (steps, steps.replace("      - ", "    - ").replace("        run:", "      run:")
                     .replace("        shell:", "      shell:").replace("          python", "        python")):
            source = header + body
            for quote in ("", "'", '"'):
                workflow = source
                for key in ("jobs", "t", "steps", "run"):
                    workflow = workflow.replace(f"{key}:", f"{quote}{key}{quote}:")
                with self.subTest(workflow=workflow):
                    self.assertEqual(self.unrun({".github/workflows/ci.yml": workflow,
                                                "tools/tests/test_inline.py": "", "tools/tests/test_block.py": "",
                                                "tools/tests/test_named.py": ""}), [])

    def test_workflow_run_block_stops_at_sibling_metadata(self):
        workflow = ("jobs:\n  t:\n    steps:\n      - run: |\n"
                    "          python3 tools/tests/test_actual.py\n"
                    "        name: python3 tools/tests/test_name.py\n"
                    "        env:\n          DESCRIPTION: python3 tools/tests/test_env.py\n"
                    "      - name: python3 tools/tests/test_later.py\n        run: echo done\n")
        self.assertEqual(self.unrun({".github/workflows/ci.yml": workflow,
                                    "tools/tests/test_actual.py": "", "tools/tests/test_name.py": "",
                                    "tools/tests/test_env.py": "", "tools/tests/test_later.py": ""}),
                         ["tools/tests/test_env.py", "tools/tests/test_later.py", "tools/tests/test_name.py"])


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ctest") and shutil.which("ninja"),
                     "CMake, CTest and Ninja are required")
class DisabledScriptRegistrationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.source = self.root / "source"
        self.script = "tools/tests/test_probe.py"
        self.registration = ('add_test(NAME probe COMMAND "${Python3_EXECUTABLE}" '
                             '"${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_probe.py")\n')
        (self.source / "tools/tests").mkdir(parents=True)
        (self.source / self.script).write_text('print("registration-probe-ran")\n', encoding="utf-8")
        self.tool = self.source / "tools/check_test_registration.py"
        self.tool.write_bytes(Path(check_test_registration.__file__).read_bytes())
        subprocess.run(["git", "init", "-q", str(self.source)], check=True, capture_output=True)

    def run_project(self, registrations, workflow=None):
        prefix = ('cmake_minimum_required(VERSION 3.20)\nproject(registration NONE)\nenable_testing()\n'
                  f'set(Python3_EXECUTABLE [=[{sys.executable}]=])\n')
        (self.source / "CMakeLists.txt").write_text(prefix + registrations, encoding="utf-8")
        if workflow is not None:
            path = self.source / ".github/workflows/check.yml"
            path.parent.mkdir(parents=True)
            path.write_text(workflow, encoding="utf-8")
        build = self.root / "build"
        subprocess.run(["cmake", "-S", str(self.source), "-B", str(build), "-G", "Ninja"],
                       check=True, capture_output=True, text=True)
        ctest = subprocess.run(["ctest", "--test-dir", str(build), "-V"],
                               check=True, capture_output=True, text=True)
        audit = subprocess.run([sys.executable, "-B", str(self.tool), "--scripts"],
                               capture_output=True, text=True)
        return ctest.stdout, audit

    def test_cli_rejects_a_script_only_registered_as_disabled(self):
        output, audit = self.run_project(
            self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n')
        self.assertIn("Disabled", output)
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)
        self.assertIn(f"error: {self.script} is a test script that no add_test or workflow runs", audit.stdout)

    def test_set_property_disabled_matches_ctest(self):
        cases = (("set_property(TEST probe PROPERTY DISABLED TRUE)", True),
                 ("set_property(TEST probe PROPERTY DISABLED ON)\nset_property(TEST probe PROPERTY DISABLED)", False),
                 ("set_property(TEST probe PROPERTY DISABLED TRUE FALSE)", False),
                 ("set_property(TEST probe APPEND PROPERTY DISABLED TRUE)", True),
                 ("set_tests_properties(probe PROPERTIES DISABLED ON)\nset_property(TEST probe APPEND PROPERTY DISABLED FALSE)", False),
                 ("set_tests_properties(probe PROPERTIES DISABLED ON)\nset_property(TEST probe APPEND PROPERTY DISABLED)", True),
                 ("set_property(TEST probe PROPERTY DISABLED TR)\nset_property(TEST probe APPEND_STRING PROPERTY DISABLED UE)", True),
                 ("set_property(TEST probe APPEND_STRING PROPERTY DISABLED T RUE)", False))
        for properties, disabled in cases:
            with self.subTest(properties=properties):
                output, audit = self.run_project(self.registration + properties + "\n")
                self.assertEqual("registration-probe-ran" not in output, disabled, output)
                self.assertEqual(audit.returncode, int(disabled), audit.stdout + audit.stderr)
                if disabled:
                    self.assertIn("Not Run (Disabled)", output)
                    self.assertIn(self.script, audit.stdout)

    def test_set_property_disables_multiple_modern_and_legacy_registrations(self):
        output, audit = self.run_project(
            self.registration +
            'add_test(legacy "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_probe.py")\n'
            'set_property(TEST probe legacy PROPERTY DISABLED TRUE)\n')
        self.assertEqual(output.count("Not Run (Disabled)"), 2)
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_conditional_set_property_preserves_possible_coverage(self):
        for properties in ('if(FALSE)\nset_property(TEST probe PROPERTY DISABLED TRUE)\nendif()\n',
                           'set_property(TEST probe PROPERTY DISABLED TR)\n'
                           'if(TRUE)\nset_property(TEST probe PROPERTY DISABLED ON)\nendif()\n'
                           'set_property(TEST probe APPEND_STRING PROPERTY DISABLED UE)\n'):
            with self.subTest(properties=properties):
                output, audit = self.run_project(self.registration + properties)
                self.assertIn("registration-probe-ran", output)
                self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_conditional_true_spellings_preserve_definite_disabled_state(self):
        for command in ('set_property(TEST probe PROPERTY DISABLED TRUE)',
                        'set_tests_properties(probe PROPERTIES DISABLED TRUE)'):
            for append in ('', 'set_property(TEST probe APPEND_STRING PROPERTY DISABLED UE)\n'):
                with self.subTest(command=command, append=append):
                    output, audit = self.run_project(
                        self.registration + 'set_tests_properties(probe PROPERTIES DISABLED ON)\n'
                        f'if(FALSE)\n{command}\nendif()\n' + append)
                    disabled = not append
                    self.assertEqual('registration-probe-ran' not in output, disabled, output)
                    self.assertEqual(audit.returncode, int(disabled), audit.stdout + audit.stderr)

    def test_set_property_in_shared_and_alternative_scopes_matches_ctest(self):
        disabled = self.registration + 'set_property(TEST probe PROPERTY DISABLED TRUE)\n'
        output, audit = self.run_project('if(TRUE)\n' + disabled + 'else()\n' + disabled + 'endif()\n')
        self.assertIn("Disabled", output)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_unrelated_set_properties_do_not_disable_a_registration(self):
        for command in ('set_property(DIRECTORY PROPERTY DISABLED TRUE)',
                        'set_property(TEST probe PROPERTY LABELS DISABLED TRUE)',
                        'set_property(TEST PROPERTY DISABLED TRUE)'):
            with self.subTest(command=command):
                output, audit = self.run_project(self.registration + command + "\n")
                self.assertIn("registration-probe-ran", output)
                self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_cross_directory_properties_do_not_disable_local_tests(self):
        content = self.registration + 'set_property(TEST probe DIRECTORY other PROPERTY DISABLED TRUE)\n'
        (self.source / 'CMakeLists.txt').write_text(content, encoding='utf-8')
        self.assertEqual(check_test_registration.run_scripts(self.source, ['CMakeLists.txt']), {self.script})

    def test_conditional_disable_does_not_hide_an_enabled_registration(self):
        output, audit = self.run_project(
            self.registration +
            'if(NOT (TRUE))\nset_tests_properties(probe PROPERTIES DISABLED TRUE)\nendif()\n')
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_disabled_registration_inside_a_shared_condition_is_rejected(self):
        output, audit = self.run_project(
            'if(TRUE)\n' + self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED ON)\nendif()\n')
        self.assertIn("Disabled", output)
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_disabled_alternative_registrations_do_not_reenable_each_other(self):
        disabled = self.registration + 'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n'
        output, audit = self.run_project('if(TRUE)\n' + disabled + 'else()\n' + disabled + 'endif()\n')
        self.assertIn("Disabled", output)
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_reenabled_registration_counts_as_run(self):
        output, audit = self.run_project(
            self.registration +
            'set_tests_properties(probe PROPERTIES TIMEOUT 10 DISABLED TRUE)\n'
            'set_tests_properties(probe PROPERTIES DISABLED OFF)\n')
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_conditional_enable_preserves_possible_coverage(self):
        output, audit = self.run_project(
            self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n'
            'if(TRUE)\nset_tests_properties(probe PROPERTIES DISABLED OFF)\nendif()\n')
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_later_loop_iteration_can_reenable_a_registration(self):
        output, audit = self.run_project(
            'foreach(toggle IN ITEMS TRUE FALSE)\nif(toggle)\n' + self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED TRUE)\nelse()\n'
            'set_tests_properties(probe PROPERTIES DISABLED OFF)\nendif()\nendforeach()\n')
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_repeated_callable_scopes_preserve_possible_reenable(self):
        for kind in ("function", "macro"):
            with self.subTest(kind=kind):
                output, audit = self.run_project(
                    f'{kind}(configure_probe toggle)\nif(${{toggle}})\n' + self.registration +
                    'set_tests_properties(probe PROPERTIES DISABLED TRUE)\nelse()\n'
                    'set_tests_properties(probe PROPERTIES DISABLED OFF)\nendif()\n' +
                    f'end{kind}()\nconfigure_probe(TRUE)\nconfigure_probe(FALSE)\n')
                self.assertIn("registration-probe-ran", output)
                self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_later_while_iteration_can_reenable_a_registration(self):
        output, audit = self.run_project(
            'set(toggle TRUE)\nwhile(TRUE)\nif(toggle)\n' + self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED TRUE)\nelse()\n'
            'set_tests_properties(probe PROPERTIES DISABLED OFF)\nendif()\n'
            'if(toggle)\nset(toggle FALSE)\nelse()\nbreak()\nendif()\nendwhile()\n')
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_conditional_disable_preserves_a_previously_disabled_test(self):
        for condition in ("TRUE", "FALSE"):
            with self.subTest(condition=condition):
                output, audit = self.run_project(
                    self.registration + 'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n' +
                    f'if({condition})\nset_tests_properties(probe PROPERTIES DISABLED TRUE)\nendif()\n')
                self.assertIn("Disabled", output)
                self.assertNotIn("registration-probe-ran", output)
                self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_another_enabled_registration_of_the_same_script_counts(self):
        output, audit = self.run_project(
            self.registration +
            'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n' +
            self.registration.replace("NAME probe", "NAME enabled"))
        self.assertIn("Disabled", output)
        self.assertIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)

    def test_disabling_multiple_modern_and_legacy_registrations_is_reported(self):
        output, audit = self.run_project(
            self.registration +
            'add_test(legacy "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/tests/test_probe.py")\n'
            'set_tests_properties(probe legacy PROPERTIES DISABLED "YES")\n')
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(output.count("Not Run (Disabled)"), 2)
        self.assertEqual(audit.returncode, 1, audit.stdout + audit.stderr)

    def test_workflow_coverage_counts_when_ctest_registration_is_disabled(self):
        output, audit = self.run_project(
            self.registration + 'set_tests_properties(probe PROPERTIES DISABLED TRUE)\n',
            'jobs:\n  check:\n    steps:\n      - run: python tools/tests/test_probe.py\n')
        self.assertIn("Disabled", output)
        self.assertNotIn("registration-probe-ran", output)
        self.assertEqual(audit.returncode, 0, audit.stdout + audit.stderr)


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

    def test_scripts_outside_a_git_checkout_is_a_usage_error(self):
        error = subprocess.CalledProcessError(128, ["git", "ls-files"], stderr="fatal: not a git repository")
        for output in ([], ["--json"]):
            with self.subTest(output=output), patch.object(check_test_registration, "unrun_scripts", side_effect=error):
                with contextlib.redirect_stderr(io.StringIO()) as errors, self.assertRaises(SystemExit) as caught:
                    check_test_registration.main(["--scripts", *output])
                self.assertEqual(caught.exception.code, 2)
                self.assertIn("run it in a git checkout", errors.getvalue())
                self.assertNotIn("Traceback", errors.getvalue())

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
