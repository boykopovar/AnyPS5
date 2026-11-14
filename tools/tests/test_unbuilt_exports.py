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

    def unbuilt(self, root, built, skipped=()):
        return check_test_registration.unbuilt_exports(root, {(root / name).resolve() for name in built}, skipped)

    def test_a_source_no_target_compiles_is_reported(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Export.cpp": 'extern "C" {\nint APS5_VABI sceExampleOpen(void) {\n    return 0;\n}\n}\n',
            "core/libs/prx/libSceExample/src/Built.cpp": "int APS5_VABI sceExampleClose(int handle) { return handle; }\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/src/Built.cpp"]), [Path("core/libs/prx/libSceExample/Export.cpp")])

    def test_a_sanitizer_build_skips_the_address_sanitizer_library(self):
        for flag in ("ANYPS5_ENABLE_ASAN", "ANYPS5_ENABLE_TSAN"):
            with self.subTest(flag=flag):
                root = self.tree({
                    "build/CMakeCache.txt": f"{flag}:BOOL=ON\n",
                    "core/libs/prx/libSceAddressSanitizer/Export.cpp": "int APS5_VABI sceAddressSanitizerReport(void) { return 0; }\n",
                    "core/libs/prx/libSceExample/Built.cpp": "",
                })
                skipped = check_test_registration.skipped_libraries(root / "build")
                self.assertEqual(skipped, {"libSceAddressSanitizer"})
                self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"], skipped), [])

    def test_a_normal_build_still_reports_the_address_sanitizer_library(self):
        root = self.tree({
            "build/CMakeCache.txt": "ANYPS5_ENABLE_ASAN:BOOL=OFF\nANYPS5_ENABLE_TSAN:BOOL=OFF\n",
            "core/libs/prx/libSceAddressSanitizer/Export.cpp": "int APS5_VABI sceAddressSanitizerReport(void) { return 0; }\n",
            "core/libs/prx/libSceExample/Built.cpp": "",
        })
        skipped = check_test_registration.skipped_libraries(root / "build")
        self.assertEqual(skipped, set())
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"], skipped), [Path("core/libs/prx/libSceAddressSanitizer/Export.cpp")])

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

    def test_documentation_includes_do_not_compile_an_export(self):
        snippets = (
            '/* example\n#include "Forgotten.cpp"\n*/\n',
            'const char* example = R"(\n#include "Forgotten.cpp"\n)";\n',
            'const char* example = u8R"example(\n#include "Forgotten.cpp"\n)example";\n',
            '// example \\\n#include "Forgotten.cpp"\n',
            'const char* example = "#include \\"Forgotten.cpp\\"";\n',
        )
        for snippet in snippets:
            with self.subTest(snippet=snippet):
                root = self.tree({
                    "core/libs/prx/libSceExample/Built.cpp": snippet + "int built() { return 0; }\n",
                    "core/libs/prx/libSceExample/Forgotten.cpp": "int APS5_VABI forgotten() { return 7; }\n",
                })
                self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]),
                                 [Path("core/libs/prx/libSceExample/Forgotten.cpp")])

    def test_real_spliced_and_commented_includes_are_transitive(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Built.cpp": "int n = 1'000;\n# /* directive */ incl\\\nude /* operand */ \"Included.cpp\"\nchar c = 'x';\n",
            "core/libs/prx/libSceExample/Included.cpp": '#  include "Nested.cpp"\nint APS5_VABI included() { return 1; }\n',
            "core/libs/prx/libSceExample/Nested.cpp": "int APS5_VABI nested() { return 2; }\n",
            "core/libs/prx/libSceExample/Orphan.cpp": "int APS5_VABI orphan() { return 3; }\n",
        })
        self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]),
                         [Path("core/libs/prx/libSceExample/Orphan.cpp")])

    def test_comment_and_literal_examples_do_not_define_exports(self):
        examples = ('/* int APS5_VABI example() { return 0; } */',
                    '// int APS5_VABI example() { return 0; }',
                    'const char* text = R"example(int APS5_VABI example() { return 0; })example";',
                    'const char* text = "int APS5_VABI example() { return 0; }";')
        for example in examples:
            with self.subTest(example=example):
                root = self.tree({
                    "core/libs/prx/libSceExample/Built.cpp": "",
                    "core/libs/prx/libSceExample/Example.cpp": example + "\n",
                })
                self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]), [])
                file = root / "core/libs/prx/libSceExample/Example.cpp"
                file.write_text(example + "\nint APS5_VABI real_export() { return 1; }\n", encoding="utf-8")
                self.assertEqual(self.unbuilt(root, ["core/libs/prx/libSceExample/Built.cpp"]),
                                 [Path("core/libs/prx/libSceExample/Example.cpp")])

    def test_main_reports_exports_hidden_behind_documentation_includes(self):
        root = self.tree({
            "core/libs/prx/libSceExample/Built.cpp": '/* example\n#include "Forgotten.cpp"\n*/\n',
            "core/libs/prx/libSceExample/Forgotten.cpp": "int APS5_VABI forgotten() { return 7; }\n",
        })
        for arguments in (["build"], ["build", "--json"]):
            with self.subTest(arguments=arguments), \
                    patch.object(check_test_registration, "__file__", str(root / "tools/check_test_registration.py")), \
                    patch.object(check_test_registration, "built", return_value={"covered_tests"}), \
                    patch.object(check_test_registration, "registered", return_value={"covered_tests"}), \
                    patch.object(check_test_registration, "compiled", return_value={(root / "core/libs/prx/libSceExample/Built.cpp").resolve()}), \
                    patch.object(check_test_registration, "skipped_libraries", return_value=set()), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(check_test_registration.main(arguments), 1)
            if "--json" in arguments:
                self.assertEqual(json.loads(output.getvalue())["unbuilt_exports"],
                                 ["core/libs/prx/libSceExample/Forgotten.cpp"])
            else:
                self.assertIn("core/libs/prx/libSceExample/Forgotten.cpp defines APS5_VABI functions but no target compiles it", output.getvalue())

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
                    patch.object(check_test_registration, "skipped_libraries", return_value=set()), \
                    patch.object(check_test_registration, "unbuilt_exports", return_value=result), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(check_test_registration.main(["build"]), expected)
            self.assertIn(line, output.getvalue())

    def test_main_notes_the_library_a_sanitizer_build_skips(self):
        with patch.object(check_test_registration, "built", return_value={"example_tests"}), \
                patch.object(check_test_registration, "registered", return_value={"example_tests"}), \
                patch.object(check_test_registration, "compiled", return_value=set()), \
                patch.object(check_test_registration, "skipped_libraries", return_value={"libSceAddressSanitizer"}), \
                patch.object(check_test_registration, "unbuilt_exports", return_value=[]), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(check_test_registration.main(["build"]), 0)
        self.assertIn("note: libSceAddressSanitizer is not built in this ASan/TSan configuration", output.getvalue())


if __name__ == "__main__":
    unittest.main()
