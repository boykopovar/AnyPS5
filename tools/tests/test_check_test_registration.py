import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_test_registration


class CheckTestRegistrationTests(unittest.TestCase):
    def test_cpp_source_blanks_comments_and_keeps_line_breaks(self):
        text = "int a; // note\n/* block */ int b;\n"
        masked = check_test_registration.cpp_source(text)
        self.assertIn("int a;", masked)
        self.assertIn("int b;", masked)
        self.assertNotIn("note", masked)
        self.assertNotIn("block", masked)
        self.assertEqual(masked.count("\n"), text.count("\n"))

    def test_cpp_source_masks_strings_unless_kept(self):
        text = 'const char* s = "ANYPS5_X";\n'
        self.assertNotIn("ANYPS5_X", check_test_registration.cpp_source(text))
        self.assertIn("ANYPS5_X", check_test_registration.cpp_source(text, keep_strings=True))

    def test_is_disabled_accepts_on_spellings_only(self):
        for value in ("1", "ON", "on", "Yes", "true", "Y"):
            self.assertTrue(check_test_registration.is_disabled(value), value)
        for value in ("0", "OFF", "", "NO", None):
            self.assertFalse(check_test_registration.is_disabled(value), value)

    def test_disabled_value_replace_and_unresolved_variables(self):
        self.assertEqual(check_test_registration.disabled_value((None, False), "replace", "ON"), ("ON", True))
        self.assertEqual(check_test_registration.disabled_value((None, False), "replace", "${SOME_VAR}"), (None, False))


class MetadataScriptRegistrationTests(unittest.TestCase):
    def unrun(self, files):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            for name, content in files.items():
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text(content, encoding="utf-8")
            return check_test_registration.unrun_scripts(root)[1]

    def metadata_files(self):
        reader = Path(check_test_registration.__file__).resolve().parents[1] / "cmake/ToolTestRegistration.cmake"
        return {"CMakeLists.txt": 'include(cmake/ToolTestRegistration.cmake)\n'
                                  'register_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\n',
                "cmake/ToolTestRegistration.cmake": reader.read_text(encoding="utf-8"),
                "tools/tests/registrations/probe.json": json.dumps({"version": 1, "tests": [
                    {"name": "probe", "script": "test_probe.py", "arguments": [], "timeout": 10}]}),
                "tools/tests/test_probe.py": ""}

    def test_metadata_registers_only_its_declared_script(self):
        files = self.metadata_files()
        files["tools/tests/test_orphan.py"] = ""
        data = json.loads(files["tools/tests/registrations/probe.json"])
        data["tests"][0]["arguments"] = ["test_orphan.py"]
        files["tools/tests/registrations/probe.json"] = json.dumps(data)
        self.assertEqual(self.unrun(files), ["tools/tests/test_orphan.py"])

    def test_arbitrary_json_does_not_register_a_script(self):
        for name in ("probe.json", "tools/tests/probe.json", "tools/tests/registrations/nested/probe.json"):
            with self.subTest(name=name):
                files = self.metadata_files()
                files[name] = files.pop("tools/tests/registrations/probe.json")
                self.assertEqual(self.unrun(files), ["tools/tests/test_probe.py"])

    def test_malformed_recognized_metadata_is_rejected(self):
        for content in ("{", '{"version": 1, "version": 1, "tests": []}',
                        '{"version": 1, "tests": [], "extra": true}'):
            with self.subTest(content=content):
                files = self.metadata_files()
                files["tools/tests/registrations/probe.json"] = content
                with self.assertRaises(ValueError):
                    self.unrun(files)

    def test_metadata_with_an_unknown_test_field_is_rejected(self):
        files = self.metadata_files()
        data = json.loads(files["tools/tests/registrations/probe.json"])
        data["tests"][0]["disabled"] = False
        files["tools/tests/registrations/probe.json"] = json.dumps(data)
        with self.assertRaises(ValueError):
            self.unrun(files)

    def test_metadata_naming_a_missing_script_is_rejected(self):
        files = self.metadata_files()
        del files["tools/tests/test_probe.py"]
        with self.assertRaises(ValueError):
            self.unrun(files)

    def test_metadata_duplicate_names_across_manifests_are_rejected(self):
        files = self.metadata_files()
        files["tools/tests/registrations/duplicate.json"] = files["tools/tests/registrations/probe.json"]
        with self.assertRaises(ValueError):
            self.unrun(files)

    def test_disconnected_metadata_does_not_register_a_script(self):
        for hook in ("", 'include(cmake/ToolTestRegistration.cmake)\n',
                     'register_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\n',
                     'include(cmake/ToolTestRegistration.cmake)\n'
                     '# register_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\n',
                     'include(cmake/ToolTestRegistration.cmake)\n'
                     'function(unused)\nregister_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\nendfunction()\n',
                     'function(unused)\ninclude(cmake/ToolTestRegistration.cmake)\nendfunction()\n'
                     'register_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\n',
                     'include(cmake/ToolTestRegistration.cmake)\nregister_python_tool_tests(other)\n'):
            with self.subTest(hook=hook):
                files = self.metadata_files()
                files["CMakeLists.txt"] = hook
                self.assertEqual(self.unrun(files), ["tools/tests/test_probe.py"])

    def test_metadata_without_its_reader_does_not_register_a_script(self):
        files = self.metadata_files()
        del files["cmake/ToolTestRegistration.cmake"]
        self.assertEqual(self.unrun(files), ["tools/tests/test_probe.py"])

    def test_metadata_hook_literal_conditions_preserve_possible_coverage(self):
        hook = self.metadata_files()["CMakeLists.txt"]
        include, call = hook.splitlines(keepends=True)
        for cmake, reachable in (("if(FALSE)\n" + hook + "endif()\n", False),
                                 (include + "if(FALSE)\n" + call + "endif()\n", False),
                                 ("if(FALSE)\n" + include + "endif()\n" + call, False),
                                 ("if(TRUE)\nelse()\n" + hook + "endif()\n", False),
                                 ("if(FALSE)\nelse()\n" + hook + "endif()\n", True),
                                 ("if(FALSE)\nelseif(TRUE)\n" + hook + "endif()\n", True),
                                 ("if(TRUE)\nelseif(TRUE)\n" + hook + "endif()\n", False),
                                 ("if(NOT TRUE)\n" + hook + "endif()\n", False),
                                 ('if(NOT "TRUE")\n' + hook + "endif()\n", False),
                                 ("if(WIN32)\n" + include + "else()\n" + call + "endif()\n", False),
                                 ("if(WIN32)\n" + hook + "endif()\n", True)):
            with self.subTest(cmake=cmake):
                files = self.metadata_files()
                files["CMakeLists.txt"] = cmake
                self.assertEqual(self.unrun(files), [] if reachable else ["tools/tests/test_probe.py"])

    def test_metadata_preserves_disabled_registration_handling(self):
        for properties, unrun in (("DISABLED TRUE", ["tools/tests/test_probe.py"]), ("DISABLED FALSE", [])):
            with self.subTest(properties=properties):
                files = self.metadata_files()
                files["CMakeLists.txt"] += f"set_tests_properties(probe PROPERTIES {properties})\n"
                self.assertEqual(self.unrun(files), unrun)

    def test_ignored_metadata_matches_the_reader_filesystem_glob(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            files = self.metadata_files()
            files[".gitignore"] = "tools/tests/registrations/\n"
            for name, content in files.items():
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text(content, encoding="utf-8")
            self.assertNotIn("tools/tests/registrations/probe.json", check_test_registration.listed(root))
            self.assertEqual(check_test_registration.unrun_scripts(root)[1], [])

    def test_deleted_cached_metadata_does_not_register_a_script(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "-q", str(root)], check=True)
            for name, content in self.metadata_files().items():
                (root / name).parent.mkdir(parents=True, exist_ok=True)
                (root / name).write_text(content, encoding="utf-8")
            subprocess.run(["git", "-C", str(root), "add", "."], check=True, capture_output=True)
            (root / "tools/tests/registrations/probe.json").unlink()
            self.assertIn("tools/tests/registrations/probe.json", check_test_registration.listed(root))
            self.assertEqual(check_test_registration.unrun_scripts(root)[1], ["tools/tests/test_probe.py"])


if __name__ == "__main__":
    unittest.main()
