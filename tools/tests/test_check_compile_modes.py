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
    def check_driver_queries(self, variants, compiler_names=("gcc", "clang")):
        for compiler in self.compilers(compiler_names):
            if compiler_names == ("gcc",):
                identity = subprocess.run([compiler, "--version"], capture_output=True, text=True, check=False)
                if "clang" in (identity.stdout + identity.stderr).lower():
                    self.skipTest("requires GNU GCC driver information options")
            for flags in variants:
                for field in ("arguments", "command"):
                    with self.subTest(compiler=compiler, flags=flags, field=field), tempfile.TemporaryDirectory(prefix="driver query ") as name:
                        root = Path(name).resolve()
                        build = root / "build"
                        build.mkdir()
                        source = root / "unit.c"
                        source.write_text("int syntax_must_be_checked = ;\n", encoding="utf-8")
                        ordinary = [compiler, "-c", str(source), "-o", str(build / "unit.o")]
                        control = subprocess.run(ordinary, cwd=build, capture_output=True, text=True, check=False)
                        self.assertNotEqual(control.returncode, 0)
                        self.assertIn("syntax_must_be_checked", control.stderr)
                        arguments = [compiler, *flags, *ordinary[1:]]
                        bypass = subprocess.run(arguments, cwd=build, capture_output=True, text=True, check=False)
                        self.assertEqual(bypass.returncode, 0, bypass.stderr)
                        (build / "unit.o").write_bytes(b"preserved object")
                        result = self.run_tool(root, build, source, arguments, field)
                        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                        self.assertIn("bypasses compilation", result.stderr)
                        self.assertNotIn("translation units compile", result.stdout)

    def test_version_and_dry_run_modes_cannot_report_success(self):
        self.check_driver_queries((("--version",), ("-###",)))

    def test_driver_help_cannot_report_success(self):
        self.check_driver_queries((("--help",),))

    def test_clang_driver_only_and_hidden_queries_cannot_report_success(self):
        if shutil.which("clang") is None:
            self.skipTest("needs Clang")
        self.check_driver_queries((("-fdriver-only",), ("--help-hidden",),
                                   ("-ccc-print-phases",), ("-ccc-print-bindings",)), compiler_names=("clang",))

    def test_clang_print_trace_still_checks_syntax(self):
        if shutil.which("clang") is None:
            self.skipTest("needs Clang")
        self.check_modes((("-print-ivar-layout",),), compiler_names=("clang",))

    def test_clang_supported_cpu_queries_cannot_report_success(self):
        self.check_driver_queries((("--print-supported-cpus",), ("-print-supported-cpus",)), compiler_names=("clang",))

    def test_clang_cpu_help_queries_cannot_report_success(self):
        self.check_driver_queries((("-mcpu=help",), ("-mtune=help",)), compiler_names=("clang",))

    def test_clang_target_cpu_options_still_check_syntax(self):
        self.check_modes((("--target=aarch64", "-mcpu=generic", "-mtune=generic"),), compiler_names=("clang",))

    def test_gcc_driver_queries_cannot_report_success(self):
        self.check_driver_queries((("-dumpversion",), ("-dumpfullversion",), ("-dumpmachine",)), compiler_names=("gcc",))

    def test_gcc_specs_and_target_help_cannot_report_success(self):
        self.check_driver_queries((("-dumpspecs",), ("--target-help",)), compiler_names=("gcc",))

    def test_gcc_print_queries_cannot_report_success(self):
        self.check_driver_queries((("-print-search-dirs",), ("-print-file-name=libgcc.a",)), compiler_names=("gcc",))

    def test_gcc_program_and_category_help_cannot_report_success(self):
        self.check_driver_queries((("-print-prog-name=cc1",), ("--help=common",)), compiler_names=("gcc",))

    def check_query_named_include_operands(self, names):
        def configure(root, build, source):
            for name in names:
                (build / name).write_text("#define HEADER_PRESENT 1\n", encoding="utf-8")
            return [], "#ifndef HEADER_PRESENT\n#error include operand was lost\n#endif\n", ""
        self.check_modes(tuple(("-include", name) for name in names), configure=configure)

    def test_query_named_include_operand_still_compiles(self):
        self.check_query_named_include_operands(("--version",))

    def test_supported_cpu_query_named_include_operands_still_compile(self):
        self.check_query_named_include_operands(("--print-supported-cpus", "-print-supported-cpus"))

    def test_cpu_help_query_named_include_operands_still_compile(self):
        self.check_query_named_include_operands(("-mcpu=help", "-mtune=help"))

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

    def test_short_preprocessing_mode_reaches_syntax_errors(self):
        self.check_modes((("-E",),))

    def test_long_preprocessing_mode_reaches_syntax_errors(self):
        self.check_modes((("--preprocess",),))

    def test_short_dependency_mode_reaches_syntax_errors(self):
        self.check_modes((("-M",),), dependency=True)

    def test_long_dependency_mode_reaches_syntax_errors(self):
        self.check_modes((("--dependencies",),), dependency=True)

    def test_short_user_dependency_mode_reaches_syntax_errors(self):
        self.check_modes((("-MM",),), dependency=True)

    def test_long_user_dependency_mode_reaches_syntax_errors(self):
        self.check_modes((("--user-dependencies",),), dependency=True)

    def check_generated_header_mode(self, flags):
        self.check_modes((flags,), dependency=True, invalid='#include "missing_generated_header.h"\n',
                         marker="missing_generated_header.h")

    def test_short_generated_header_mode_does_not_hide_missing_headers(self):
        self.check_generated_header_mode(("-M", "-MG"))

    def test_long_generated_header_mode_does_not_hide_missing_headers(self):
        self.check_generated_header_mode(("--dependencies", "--print-missing-file-dependencies"))

    def test_short_preprocessor_output_companions_do_not_block_syntax_checks(self):
        self.check_modes((("-Werror", "-E", "-P"),))

    def test_long_preprocessor_output_companions_do_not_block_syntax_checks(self):
        self.check_modes((("-Werror", "--preprocess", "--no-line-commands"),))

    def test_macro_only_dump_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-dM"),))

    def test_macro_and_output_dump_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-dD"),))

    def test_include_dump_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-dI"),))

    def test_real_compiler_wrapper_keeps_one_syntax_mode(self):
        self.check_modes((("-fsyntax-only", "-E"),), wrap=True)

    def test_trailing_preprocessing_mode_still_checks_syntax(self):
        self.check_modes((("-E",),), trailing=True)

    def test_comment_output_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-C"),))

    def test_macro_comment_output_mode_still_checks_syntax(self):
        self.check_modes((("-Werror", "-E", "-CC"),))

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
        def configure(root, build, source):
            return ["-L", "-E"], "", ""
        self.check_modes(((),), configure=configure)

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


    def test_nested_response_modes_preserve_mode_named_include_directories(self):
        def configure(root, build, source):
            directory = build / "-E"
            directory.mkdir()
            (directory / "nested_header.h").write_text("#define FROM_RESPONSE 1\n", encoding="utf-8")
            (build / "inner.rsp").write_text("-I -E", encoding="utf-8")
            response = build / "outer flags.rsp"
            response.write_text("@inner.rsp -E -P", encoding="utf-8")
            return ["@" + str(response)], '#include "nested_header.h"\n#ifndef FROM_RESPONSE\n#error include lost\n#endif\n', ""
        self.check_modes(((),), configure=configure)


if __name__ == "__main__":
    unittest.main()
