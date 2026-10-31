from pathlib import Path
import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_test_registration


class UnbuiltExportTests(unittest.TestCase):
    def tree(self, files):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        for name, content in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
        return root

    def unbuilt(self, root, built):
        return check_test_registration.unbuilt_exports(root, {(root / name).resolve() for name in built})

    def test_a_source_no_target_compiles_is_reported(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Export.cpp": 'extern "C" {\nint APS5_VABI sceExampleOpen(void) {\n    return 0;\n}\n}\n',
            "core/libs/prx/libSceExample/src/Built.cpp": "int APS5_VABI sceExampleClose(int handle) { return handle; }\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/src/Built.cpp"]), [Path("core/libs/prx/libSceExample/Export.cpp")])

    def test_multiline_parameters_and_qualifiers_still_count_as_definitions(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Built.cpp": "",
            "core/libs/prx/libSceExample/Wide.cpp": "[[noreturn]] void APS5_VABI sceExampleAbort(int code,\n        const char* reason) noexcept {\n}\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]), [Path("core/libs/prx/libSceExample/Wide.cpp")])

    def test_declarations_tests_and_sources_without_exports_are_not_reported(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Built.cpp": "",
            "core/libs/prx/libSceExample/Declared.cpp": 'extern "C" int APS5_VABI sceExampleOpen(void);\nusing Callback = int (APS5_VABI*)(int);\n',
            "core/libs/prx/libSceExample/Plain.cpp": "static int helper() { return 1; }\n",
            "core/libs/prx/libSceExample/tests/Guest.cpp": "int APS5_VABI sceExampleOpen(void) { return 0; }\n",
            "core/libs/prx/libSceExample/src/test/Fixture.cpp": "int APS5_VABI sceExampleFixture(void) { return 0; }\n",
            "core/libs/tests/GuestExample.cpp": "int APS5_VABI sceExampleOpen(void) { return 0; }\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]), [])

    def test_a_source_included_by_a_compiled_source_counts_as_built(self):
        root = self.tree({
            "core/libs/prx/libc/src/exception/Exports.cpp": '#include "Unwind.cpp"\n  #  include "Runtime.cpp"\n',
            "core/libs/prx/libc/src/exception/Unwind.cpp": '#include "Personality.cpp"\nint APS5_VABI _Unwind_Resume(void* exception) { return 0; }\n',
            "core/libs/prx/libc/src/exception/Runtime.cpp": "void APS5_VABI __cxa_rethrow() {}\n",
            "core/libs/prx/libc/src/exception/Personality.cpp": "int APS5_VABI __gxx_personality_v0(int version) { return version; }\n",
            "core/libs/prx/libc/src/exception/Orphan.cpp": "int APS5_VABI __cxa_orphan(void) { return 0; }\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libc/src/exception/Exports.cpp"]), [Path("core/libs/prx/libc/src/exception/Orphan.cpp")])

    def test_function_try_blocks_still_count_as_definitions(self):
        for suffix in (" try", " noexcept try", "\n        noexcept\n        try"):
            with self.subTest(suffix=suffix):
                root = self.tree({
                    "core/libs/prx/libSceExample/Built.cpp": "",
                    "core/libs/prx/libSceExample/Try.cpp": (
                        "int APS5_VABI sceExampleTry(int value)" + suffix
                        + " { return value; } catch (...) { return -1; }\n"
                    ),
                })
                self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]),
                                 [Path("core/libs/prx/libSceExample/Try.cpp")])

    def test_a_build_without_library_sources_is_not_checked(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Export.cpp": "int APS5_VABI sceExampleOpen(void) { return 0; }\n",
            "core/relinker/Main.cpp": "int main() { return 0; }\n",
        })
        self.assertIsNone(self.unbuilt(root, ["core/relinker/Main.cpp"]))

    def test_compiled_sources_come_from_the_compile_commands_of_ninja(self):
        root = self.tree({"core/libs/prx/libSceExample/Export.cpp": "", "core/libs/prx/libSceExample/Other.cpp": ""})
        database = [
            {"directory": str(root / "build"), "file": "../core/libs/prx/libSceExample/Export.cpp",
             "command": "/usr/bin/c++ -O2 -o Export.cpp.o -c ../core/libs/prx/libSceExample/Export.cpp"},
            {"directory": str(root / "build"), "file": str(root / "core/libs/prx/libSceExample/Other.cpp"),
             "arguments": ["clang-cl", "/nologo", "/c", str(root / "core/libs/prx/libSceExample/Other.cpp")]},
            {"directory": str(root / "build"), "file": "libSceExample.prx", "command": "/usr/bin/c++ -shared -o libSceExample.prx Export.cpp.o"},
            {"directory": str(root / "build"), "file": "../core/libs/prx/libSceExample/Generator.cpp",
             "command": "python3 generate.py ../core/libs/prx/libSceExample/Generator.cpp"},
        ]

        def run(command, *, capture_output, text, encoding, check):
            self.assertEqual(command, ["ninja", "-C", "build", "-t", "compdb"])
            return subprocess.CompletedProcess(command, 0, json.dumps(database), "")

        with patch.object(check_test_registration, "ninja", return_value=("ninja", "utf-8")), \
                patch.object(check_test_registration.subprocess, "run", side_effect=run):
            sources = check_test_registration.compiled("build")
        self.assertEqual(sources, {(root / "core/libs/prx/libSceExample/Export.cpp").resolve(),
                                   (root / "core/libs/prx/libSceExample/Other.cpp").resolve()})

    def test_main_fails_and_names_each_unbuilt_source(self):
        unbuilt = [Path("core/libs/prx/libSceExample/Export.cpp")]
        for result, expected, line in ((unbuilt, 1, "error: core/libs/prx/libSceExample/Export.cpp defines APS5_VABI functions but no target compiles it"),
                                       ([], 0, "every core/libs/prx source that defines an APS5_VABI function is compiled"),
                                       (None, 0, "no core/libs/prx source is compiled in this build; library sources were not checked")):
            with self.subTest(result=result), patch.object(check_test_registration, "built", return_value={"example_tests"}), \
                    patch.object(check_test_registration, "registered", return_value={"example_tests"}), \
                    patch.object(check_test_registration, "compiled", return_value=set()), \
                    patch.object(check_test_registration, "unbuilt_exports", return_value=result), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(check_test_registration.main(["build"]), expected)
            self.assertIn(line, output.getvalue())


if __name__ == "__main__":
    unittest.main()
