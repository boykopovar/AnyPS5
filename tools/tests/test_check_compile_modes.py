import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/check_compile.py"


class CheckCompileModeTests(unittest.TestCase):
    def compilers(self, names=("gcc", "clang")):
        values = list(dict.fromkeys(value for value in map(shutil.which, names) if value))
        if not values:
            self.skipTest("needs GCC or Clang")
        return values

    def run_tool(self, root, build, source, arguments, field):
        if not (root / ".git").exists():
            subprocess.run(["git", "init", "-q", str(root)], capture_output=True, check=True)
        command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                   " ".join(shlex.quote(value) for value in arguments))
        entry = {"directory": str(build), "file": str(source),
                 field: arguments if field == "arguments" else command}
        (build / "compile_commands.json").write_text(json.dumps([entry]), encoding="utf-8")
        before = {path: path.read_bytes() for path in root.rglob("*") if path.is_file()}
        result = subprocess.run([sys.executable, "-I", str(TOOL), "--build", str(build), str(source)],
                                cwd=root, capture_output=True, text=True, encoding="utf-8", check=False)
        self.assertEqual({path for path in root.rglob("*") if path.is_file()}, set(before), result.stderr)
        for path, data in before.items():
            self.assertEqual(path.read_bytes(), data, path.name)
        self.assertNotIn("Traceback", result.stderr)
        return result

    def check_modes(self, variants, dependency=False, configure=None, invalid=None, marker="syntax_must_be_checked", compiler_names=("gcc", "clang"), wrap=False, trailing=False):
        for compiler in self.compilers(compiler_names):
            for flags in variants:
                for field in ("arguments", "command"):
                    with self.subTest(compiler=compiler, flags=flags, field=field), tempfile.TemporaryDirectory(prefix="syntax mode ") as name:
                        root = Path(name).resolve()
                        build = root / "build"
                        build.mkdir()
                        source = root / "unit.c"
                        object_file, dependency_file = build / "unit.o", build / "unit.d"
                        prefix = "#if REQUIRED != 42\n#error definition was lost\n#endif\n"
                        suffix = ""
                        extra = list(flags)
                        if configure is not None:
                            additions, opening, suffix = configure(root, build, source)
                            extra += additions
                            prefix = opening + prefix
                        valid = prefix + "int main(void) { return 0; }\n" + suffix
                        invalid_text = prefix + (invalid or "int syntax_must_be_checked = ;\n") + suffix
                        compile_flags = [] if any(flag in flags for flag in ("-E", "--preprocess", "-M", "--dependencies", "-MM", "--user-dependencies")) else ["-c"]
                        driver = [compiler]
                        if wrap:
                            wrapper = root / "wrapper.py"
                            wrapper.write_text("import subprocess, sys\n"
                                               "if sys.argv[2:].count('-fsyntax-only') != 1: sys.exit(78)\n"
                                               "sys.exit(subprocess.run(sys.argv[1:]).returncode)\n", encoding="utf-8")
                            driver = [sys.executable, str(wrapper), compiler]
                        arguments = [*driver, "-D", "REQUIRED=42", *([] if trailing else extra),
                                     *compile_flags, str(source), *(extra if trailing else []), "-o", str(object_file)]
                        if dependency:
                            arguments += ["-MF", str(dependency_file)]
                        source.write_text(valid, encoding="utf-8")
                        control = subprocess.run(arguments, cwd=build, capture_output=True, text=True, check=False)
                        self.assertEqual(control.returncode, 0, control.stderr)
                        object_file.write_bytes(b"existing object output\n")
                        dependency_file.write_bytes(b"existing dependency output\n")
                        for text, expected in ((valid, 0), (invalid_text, 1)):
                            source.write_text(text, encoding="utf-8")
                            result = self.run_tool(root, build, source, arguments, field)
                            self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                            if expected:
                                self.assertIn(marker, result.stderr)
                            else:
                                self.assertIn("1 translation units compile", result.stdout)

    def test_preprocessing_modes_reach_syntax_errors(self):
        self.check_modes((("-E",), ("--preprocess",)))

    def test_dependency_modes_reach_syntax_errors(self):
        self.check_modes((("-M",), ("--dependencies",)), dependency=True)

    def test_user_dependency_modes_reach_syntax_errors(self):
        self.check_modes((("-MM",), ("--user-dependencies",)), dependency=True)

    def test_generated_header_mode_does_not_hide_missing_headers(self):
        self.check_modes((("-M", "-MG"), ("--dependencies", "--print-missing-file-dependencies")),
                         dependency=True, invalid='#include "missing_generated_header.h"\n',
                         marker="missing_generated_header.h")

    def test_preprocessor_output_companions_do_not_block_syntax_checks(self):
        self.check_modes((("-Werror", "-E", "-P"), ("-Werror", "--preprocess", "--no-line-commands")))

    def test_macro_dump_modes_still_check_syntax(self):
        self.check_modes((("-Werror", "-E", "-dM"), ("-Werror", "-E", "-dD")))

    def test_include_dump_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-dI"),))

    def test_real_compiler_wrapper_keeps_one_syntax_mode(self):
        self.check_modes((("-fsyntax-only", "-E"),), wrap=True)

    def test_trailing_preprocessing_mode_still_checks_syntax(self):
        self.check_modes((("-E",),), trailing=True)

    def test_comment_output_modes_still_check_syntax(self):
        self.check_modes((("-Werror", "-E", "-C"), ("-Werror", "-E", "-CC")))

    def test_joined_definitions_before_preprocessing_modes_are_preserved(self):
        self.check_modes((("-DANOTHER_MACRO", "-E"),))

    def test_joined_undefinitions_before_preprocessing_modes_are_preserved(self):
        self.check_modes((("-UUNUSED_MACRO", "-E"),))

    def test_optimization_and_warning_options_before_preprocessing_modes_are_preserved(self):
        self.check_modes((("-O2", "-Wall", "-E"),))

    def test_gcc_negative_syntax_mode_cannot_enable_output(self):
        compiler = self.compilers(("gcc",))[0]
        version = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=False)
        if "clang" in (version.stdout + version.stderr).lower():
            self.skipTest("GCC's negative syntax mode is unsupported by Clang")
        self.check_modes((("-fno-syntax-only",),), compiler_names=("gcc",))

    def test_explicit_cpp_language_is_preserved(self):
        self.check_modes((("-E", "-x", "c++"),))

    def test_clang_end_of_options_preserves_mode_named_inputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("needs Clang")
        for field in ("arguments", "command"):
            with self.subTest(field=field), tempfile.TemporaryDirectory(prefix="syntax positional ") as name:
                root = Path(name).resolve()
                build = root / "build"
                build.mkdir()
                source = root / "unit.c"
                source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
                other = build / "-E"
                arguments = [compiler, "-x", "c", "--", "-E", str(source)]
                for text, expected in (("int extra;\n", 0), ("int positional_syntax_error = ;\n", 1)):
                    other.write_text(text, encoding="utf-8")
                    result = self.run_tool(root, build, source, arguments, field)
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                    if expected:
                        self.assertIn("positional_syntax_error", result.stderr)

    def check_forwarded_options(self, variants, response_text=None):
        compiler = self.compilers(("clang",))[0]
        for flags in variants:
            for field in ("arguments", "command"):
                for text in ("int value;\n", "int forwarded_syntax_error = ;\n"):
                    with self.subTest(flags=flags, field=field, text=text), tempfile.TemporaryDirectory(prefix="syntax forwarded ") as name:
                        root = Path(name).resolve()
                        build = root / "build"
                        build.mkdir()
                        source = root / "unit.c"
                        if response_text is not None:
                            (build / "operand.rsp").write_text(response_text, encoding="utf-8")
                        source.write_text(text, encoding="utf-8")
                        object_file, dependency_file = build / "unit.o", build / "unit.d"
                        object_file.write_bytes(b"existing object output\n")
                        dependency_file.write_bytes(b"existing dependency output\n")
                        arguments = [compiler, *flags, "-c", str(source), "-o", str(object_file), "-MF", str(dependency_file)]
                        result = self.run_tool(root, build, source, arguments, field)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertIn("unsupported forwarded compiler option", result.stderr)

    def test_forwarded_clang_actions_are_rejected(self):
        self.check_forwarded_options((("-Xclang", "-E"), ("-Xclang=-E",), ("-Xclang", "-emit-obj")))

    def test_forwarded_preprocessor_actions_are_rejected(self):
        self.check_forwarded_options((("-Xpreprocessor", "-E"), ("-Xpreprocessor", "-M")))

    def test_comma_forwarded_preprocessor_actions_are_rejected(self):
        self.check_forwarded_options((("-Wp,-E",), ("-Wp,-E,-P",)))

    def test_forwarded_response_operands_cannot_hide_actions(self):
        self.check_forwarded_options((("-Wp,-I,@operand.rsp",),), response_text="somewhere -E")

    def test_include_directory_values_can_look_like_modes(self):
        def configure(root, build, source):
            directory = build / "-E"
            directory.mkdir()
            (directory / "only_here.h").write_text("#define FROM_INCLUDE 1\n", encoding="utf-8")
            return ["-I", "-E"], '#include "only_here.h"\n#ifndef FROM_INCLUDE\n#error include lost\n#endif\n', ""
        self.check_modes(((),), configure=configure)

    def test_forwarded_macro_options_are_preserved(self):
        self.check_modes((("-Wp,-DOTHER=1,-UUNUSED",),))

    def test_forwarded_include_operands_can_look_like_modes(self):
        def configure(root, build, source):
            directory = build / "-E"
            directory.mkdir()
            (directory / "forwarded.h").write_text("#define FROM_INCLUDE 1\n", encoding="utf-8")
            return ["-Wp,-I,-E"], '#include "forwarded.h"\n#ifndef FROM_INCLUDE\n#error include lost\n#endif\n', ""
        self.check_modes(((),), configure=configure)

    def test_library_directory_values_can_look_like_modes(self):
        self.check_modes((("-L", "-E"),))

    def test_forced_include_values_can_look_like_modes(self):
        def configure(root, build, source):
            (build / "-M").write_text("#define FROM_INCLUDE 1\n", encoding="utf-8")
            return ["-include", "-M"], "#ifndef FROM_INCLUDE\n#error include lost\n#endif\n", ""
        self.check_modes(((),), configure=configure)

    def test_source_path_used_as_an_option_value_is_preserved(self):
        def configure(root, build, source):
            return ["-include", str(source)], "#ifndef INCLUDED_ONCE\n#define INCLUDED_ONCE\n", "#endif\n"
        self.check_modes(((),), configure=configure)

    def test_missing_path_values_fail_before_a_syntax_flag_is_injected(self):
        self.check_missing_values(("-I", "-L", "-include"))

    def test_missing_language_and_output_values_fail_before_a_syntax_flag_is_injected(self):
        self.check_missing_values(("-x", "-o", "-MF"))

    def check_missing_values(self, options):
        for compiler in self.compilers():
            for option in options:
                for field in ("arguments", "command"):
                    with self.subTest(compiler=compiler, option=option, field=field), tempfile.TemporaryDirectory(prefix="syntax missing ") as name:
                        root = Path(name).resolve()
                        build = root / "build"
                        build.mkdir()
                        source = root / "unit.c"
                        source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
                        result = self.run_tool(root, build, source, [compiler, option], field)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertIn("missing", result.stderr.lower())

    def test_unknown_options_still_fail(self):
        for compiler in self.compilers():
            for field in ("arguments", "command"):
                with self.subTest(compiler=compiler, field=field), tempfile.TemporaryDirectory(prefix="syntax unknown ") as name:
                    root = Path(name).resolve()
                    build = root / "build"
                    build.mkdir()
                    source = root / "unit.c"
                    source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
                    option = "-funknown-syntax-mode-test"
                    result = self.run_tool(root, build, source, [compiler, "-E", option, "-c", str(source)], field)
                    self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                    self.assertIn(option, result.stderr)

    def test_unknown_operand_counts_are_rejected_before_mode_rewriting(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("needs Clang")
        for field in ("arguments", "command"):
            with self.subTest(field=field), tempfile.TemporaryDirectory(prefix="syntax arity ") as name:
                root = Path(name).resolve()
                build = root / "build"
                build.mkdir()
                source = root / "unit.c"
                source.write_text("int main(void) { return 0; }\n", encoding="utf-8")
                arguments = [compiler, "-fdebug-compilation-dir", "-E", "-c", str(source), "-o", str(build / "unit.o")]
                control = subprocess.run(arguments, cwd=build, capture_output=True, text=True, check=False)
                self.assertEqual(control.returncode, 0, control.stderr)
                result = self.run_tool(root, build, source, arguments, field)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("unknown operand count", result.stderr)


if __name__ == "__main__":
    unittest.main()
