import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "cmake"))
import tool_test_registration


class RegistrationFixture:
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tool metadata ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.scripts = self.root / "tools/tests"
        self.manifests = self.scripts / "registrations"
        self.manifests.mkdir(parents=True)
        self.script = self.scripts / "test_example.py"
        self.script.write_text("import json, pathlib, sys\npathlib.Path(__file__).with_suffix('.args').write_text(json.dumps(sys.argv[1:]), encoding='utf-8')\n", encoding="utf-8")

    def record(self, **changes):
        return dict({"name": "example", "script": self.script.name, "arguments": ["ExampleTests"], "timeout": 30}, **changes)

    def manifest(self, tests=None, name="example.json"):
        path = self.manifests / name
        path.write_text(json.dumps({"version": 1, "tests": tests if tests is not None else [self.record()]}), encoding="utf-8")
        return path


class ToolTestRegistrationTests(RegistrationFixture, unittest.TestCase):
    def test_manifest_order_preserves_each_record_order_and_literals(self):
        literals = ["class with spaces", 'double"quote', "single'quote", "Unicode \u00e9 \u6f22\u5b57", "dollar$plain"]
        first = self.record(name="z_first", arguments=literals, timeout=0.5)
        second = self.record(name="a_second", arguments=[], timeout=1)
        last = self.record(name="last")
        self.manifest([last], "z.json")
        self.manifest([first, second], "a.json")
        self.assertEqual(tool_test_registration.load_registrations(self.root), [first, second, last])

    def test_absent_directory_and_no_files_have_no_tests(self):
        self.assertEqual(tool_test_registration.load_registrations(self.root), [])
        self.manifests.rmdir()
        self.assertEqual(tool_test_registration.load_registrations(self.root), [])

    def test_exact_root_schema_and_version(self):
        valid = {"version": 1, "tests": [self.record()]}
        invalid = [None, [], "value", 1, {}, {"version": 1}, {"tests": [self.record()]},
                   dict(valid, version=True), dict(valid, version=1.0), dict(valid, version=2),
                   dict(valid, extra="ignored"), dict(valid, tests=None), dict(valid, tests={}),
                   dict(valid, tests=[])]
        path = self.manifest()
        for value in invalid:
            with self.subTest(value=value):
                path.write_text(json.dumps(value), encoding="utf-8")
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_records_require_only_the_four_supported_fields(self):
        invalid = [None, [], "value", 1]
        for key in self.record():
            item = self.record()
            del item[key]
            invalid.append(item)
        for key, value in (("command", ["native_tool"]), ("properties", {"DISABLED": True}),
                           ("platform", "Windows"), ("executable", "native_tool")):
            invalid.append(dict(self.record(), **{key: value}))
        for item in invalid:
            with self.subTest(record=item):
                self.manifest([item])
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_names_are_stable_literal_identifiers(self):
        for value in (None, 7, True, "", "has space", "has-hyphen", "2first", "a;b", "${name}", "$<CONFIG>", "\u00e9"):
            with self.subTest(name=value):
                self.manifest([self.record(name=value)])
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_timeouts_are_finite_positive_numbers_within_policy(self):
        for value in (None, True, False, "30", [], 0, -1, 30.01, float("nan"), float("inf"), float("-inf")):
            with self.subTest(timeout=value):
                self.manifest([self.record(timeout=value)])
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_arguments_reject_scalar_and_evaluated_or_split_values(self):
        invalid = [None, "ExampleTests", {}, 1, [None], [True], [1], [[]], [""],
                   ["a;b"], ["a\\b"], ["[value]"], ["${variable}"], ["$<CONFIG>"],
                   ["line\nbreak"], ["tab\tvalue"], ["null\x00value"], ["delete\x7fvalue"],
                   ["next\u0085line"], ["surrogate\ud800"]]
        for value in invalid:
            with self.subTest(arguments=value):
                self.manifest([self.record(arguments=value)])
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_script_must_be_an_existing_flat_python_test(self):
        for value in (None, 1, "", "test_missing.py", "../test_example.py", "sub/test_example.py",
                      str(self.script.resolve()), "run.py", "test_a-b.py", "test_example.py;other"):
            with self.subTest(script=value):
                self.manifest([self.record(script=value)])
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)
        self.script.unlink()
        self.script.mkdir()
        self.manifest()
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)

    def test_duplicate_names_across_and_within_manifests_are_rejected(self):
        self.manifest([self.record(), self.record()])
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)
        self.manifest()
        self.manifest(name="other.json")
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)

    def test_duplicate_json_keys_and_nonstandard_numbers_are_rejected(self):
        path = self.manifest()
        record = json.dumps(self.record())
        values = ['{"version":1,"version":1,"tests":[' + record + ']}',
                  '{"version":1,"tests":[],"tests":[' + record + ']}',
                  '{"version":1,"tests":[' + record[:-1] + ',"timeout":20}]}',
                  '{"version":1,"tests":[' + record.replace('30', 'NaN') + ']}',
                  '{"version":1,"tests":[' + record.replace('30', 'Infinity') + ']}',
                  '{"version":1,"tests":[', "not json"]
        for value in values:
            with self.subTest(source=value):
                path.write_text(value, encoding="utf-8")
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root)

    def test_explicit_paths_cannot_bypass_the_manifest_directory(self):
        recognized = self.manifest()
        outside = self.root / "outside.json"
        outside.write_bytes(recognized.read_bytes())
        nested = self.manifests / "nested"
        nested.mkdir()
        nested_path = nested / "other.json"
        nested_path.write_bytes(recognized.read_bytes())
        for path in (outside, nested_path, self.manifests / "missing.json", self.script):
            with self.subTest(path=path):
                with self.assertRaises(ValueError):
                    tool_test_registration.load_registrations(self.root, [path])
        self.assertEqual(tool_test_registration.load_registrations(self.root, [recognized]), [self.record()])

    def test_symlink_scripts_and_manifests_cannot_escape_the_root(self):
        outside = self.root / "outside.py"
        outside.write_text("raise SystemExit(99)\n", encoding="utf-8")
        self.script.unlink()
        try:
            self.script.symlink_to(outside)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        path = self.manifest()
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)
        self.script.unlink()
        self.script.write_text("pass\n", encoding="utf-8")
        outside_manifest = self.root / "outside.json"
        path.replace(outside_manifest)
        path.symlink_to(outside_manifest)
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)

    def test_linked_manifest_directory_cannot_escape_the_root(self):
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "example.json").write_text(json.dumps({"version": 1, "tests": [self.record()]}), encoding="utf-8")
        self.manifests.rmdir()
        if os.name == "nt":
            result = subprocess.run(["cmd", "/d", "/c", "mklink", "/J", str(self.manifests), str(outside)],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.manifests.symlink_to(outside, target_is_directory=True)
        with self.assertRaises(ValueError):
            tool_test_registration.load_registrations(self.root)


class ToolTestRegistrationCliTests(RegistrationFixture, unittest.TestCase):
    def test_cli_emits_only_the_canonical_validated_inventory(self):
        item = self.record(arguments=["two words", 'quote"', "\u6f22\u5b57"])
        self.manifest([item])
        result = subprocess.run([sys.executable, str(ROOT / "cmake/tool_test_registration.py"), str(self.root)],
                                capture_output=True, text=True, encoding="utf-8", timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(json.loads(result.stdout), {"version": 1, "tests": [item]})
        self.assertEqual(len(result.stdout.splitlines()), 1)

    def test_cli_invalid_metadata_has_no_partial_inventory_and_exits_two(self):
        self.manifest([self.record()], "a_valid.json")
        invalid = self.manifest([self.record(name="another", timeout=True)], "z_invalid.json")
        result = subprocess.run([sys.executable, str(ROOT / "cmake/tool_test_registration.py"), str(self.root)],
                                capture_output=True, text=True, encoding="utf-8", timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, "")
        self.assertIn(invalid.name, result.stderr)


class CMakeFixture(RegistrationFixture):
    def setUp(self):
        super().setUp()
        if not shutil.which("cmake") or not shutil.which("ninja") or not shutil.which("ctest"):
            self.skipTest("CMake, Ninja and CTest are required")
        (self.root / "cmake").mkdir()
        for name in ("ToolTestRegistration.cmake", "tool_test_registration.py"):
            shutil.copyfile(ROOT / "cmake" / name, self.root / "cmake" / name)
        self.build = self.root / "build"
        self.project = (
            'cmake_minimum_required(VERSION 3.20)\nproject(metadata NONE)\nenable_testing()\n'
            'set(Python3_EXECUTABLE "' + Path(sys.executable).as_posix() + '")\n'
            'include(cmake/ToolTestRegistration.cmake)\n'
        )
        self.call = 'register_python_tool_tests("${CMAKE_CURRENT_SOURCE_DIR}")\n'

    def configure(self, source=None, success=True):
        if source is not None:
            (self.root / "CMakeLists.txt").write_text(self.project + source, encoding="utf-8")
        result = subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.build), "-G", "Ninja"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=15)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def inventory(self):
        result = subprocess.run(["ctest", "--test-dir", str(self.build), "--show-only=json-v1"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)["tests"]


class ToolTestRegistrationCMakeTests(CMakeFixture, unittest.TestCase):
    def test_real_registration_preserves_order_timeout_and_exact_argv(self):
        arguments = ["class with spaces", 'double"quote', "single'quote", "\u00e9\u6f22\u5b57", "dollar$plain"]
        self.manifest([self.record(name="z_first", arguments=arguments, timeout=12.5),
                       self.record(name="a_second", arguments=[], timeout=1)])
        self.configure(self.call)
        tests = self.inventory()
        self.assertEqual([test["name"] for test in tests], ["z_first", "a_second"])
        self.assertEqual(Path(tests[0]["command"][0]).resolve(), Path(sys.executable).resolve())
        self.assertEqual(Path(tests[0]["command"][1]).resolve(), self.script.resolve())
        self.assertEqual(tests[0]["command"][2:], arguments)
        properties = {item["name"]: item["value"] for item in tests[0]["properties"]}
        self.assertEqual(properties["TIMEOUT"], 12.5)
        result = subprocess.run(["ctest", "--test-dir", str(self.build), "-R", "^z_first$", "--output-on-failure"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(json.loads(self.script.with_suffix(".args").read_text(encoding="utf-8")), arguments)

    def test_legacy_name_collision_before_or_after_the_hook_is_rejected(self):
        self.manifest()
        legacy = 'add_test(NAME example COMMAND "${CMAKE_COMMAND}" -E true)\n'
        for source in (legacy + self.call, self.call + legacy):
            with self.subTest(source=source):
                result = self.configure(source, success=False)
                self.assertIn("example", result.stdout + result.stderr)

    def test_subdirectory_name_collisions_before_and_after_the_hook_are_rejected(self):
        self.manifest()
        child = self.root / "child"
        child.mkdir()
        (child / "CMakeLists.txt").write_text('add_test(NAME example COMMAND "${CMAKE_COMMAND}" -E true)\n', encoding="utf-8")
        for source in ('add_subdirectory(child)\n' + self.call, self.call + 'add_subdirectory(child)\n'):
            with self.subTest(source=source):
                result = self.configure(source, success=False)
                self.assertIn("example", result.stdout + result.stderr)

    def test_unrelated_subdirectory_test_is_preserved(self):
        self.manifest()
        child = self.root / "child"
        child.mkdir()
        (child / "CMakeLists.txt").write_text('add_test(NAME native_child COMMAND "${CMAKE_COMMAND}" -E true)\n', encoding="utf-8")
        self.configure(self.call + 'add_subdirectory(child)\n')
        self.assertEqual([test["name"] for test in self.inventory()], ["example", "native_child"])

    def test_invalid_manifest_fails_configuration(self):
        self.manifest([self.record(arguments=["${danger}"])])
        result = self.configure(self.call, success=False)
        self.assertIn("example.json", result.stdout + result.stderr)

    def test_include_alone_does_not_evaluate_metadata(self):
        self.manifest([self.record(timeout=False)])
        self.configure('add_test(NAME native_control COMMAND "${CMAKE_COMMAND}" -E true)\n')
        self.assertEqual([test["name"] for test in self.inventory()], ["native_control"])


class ToolTestRegistrationRegenerationTests(CMakeFixture, unittest.TestCase):
    def test_add_edit_and_remove_manifests_regenerate_without_cmake_edits(self):
        path = self.manifest([self.record(name="first", timeout=2)])
        self.configure(self.call)
        original = (self.root / "CMakeLists.txt").read_bytes()
        self.assertEqual([test["name"] for test in self.inventory()], ["first"])
        second = self.manifest([self.record(name="second", timeout=3)], "second.json")
        mutations = ((None, ["first", "second"], 2),
                     ([self.record(name="changed", timeout=4)], ["changed", "second"], 4),
                     ([], ["changed"], 4))
        for changed, expected, timeout in mutations:
            with self.subTest(expected=expected):
                if changed == []:
                    second.unlink()
                elif changed is not None:
                    path.write_text(json.dumps({"version": 1, "tests": changed}), encoding="utf-8")
                result = subprocess.run(["cmake", "--build", str(self.build)], capture_output=True, text=True,
                                        encoding="utf-8", errors="replace", timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                tests = self.inventory()
                self.assertEqual([test["name"] for test in tests], expected)
                properties = {item["name"]: item["value"] for item in tests[0]["properties"]}
                self.assertEqual(properties["TIMEOUT"], timeout)
                self.assertEqual((self.root / "CMakeLists.txt").read_bytes(), original)

    def test_validator_edits_are_configure_dependencies(self):
        self.manifest()
        self.configure(self.call)
        (self.root / "cmake/tool_test_registration.py").write_text("raise SystemExit(2)\n", encoding="utf-8")
        result = subprocess.run(["cmake", "--build", str(self.build)], capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=15)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
