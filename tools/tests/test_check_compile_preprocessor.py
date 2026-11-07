import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_compile


class CheckCompilePreprocessorTests(unittest.TestCase):
    def test_forwarded_dependencies_preserve_outputs_and_compiler_errors(self):
        self.check_forwarded_dependencies(shutil.which("clang") or shutil.which("gcc"), ("-MD", "-MMD"))

    def test_gcc_dependency_aliases_preserve_outputs_and_compiler_errors(self):
        compiler = shutil.which("gcc")
        if compiler is not None:
            version = subprocess.run([compiler, "--version"], capture_output=True, text=True)
            if "clang" in version.stdout.lower():
                self.skipTest("GCC dependency aliases are not supported by Clang")
        self.check_forwarded_dependencies(compiler, ("--write-dependencies", "--write-user-dependencies"))

    def check_forwarded_dependencies(self, compiler, options):
        if compiler is None:
            self.skipTest("needs a C compiler")
        with tempfile.TemporaryDirectory(prefix="syntax check ") as folder:
            root = Path(folder).resolve()
            source = root / "unit source.c"
            dependency = root / "unit deps.d"
            object_file = root / "unit.o"
            valid = "#ifndef REQUIRED\n#error lost_preprocessor_definition\n#endif\nint value;\n"
            for option in options:
                for field in ("arguments", "command"):
                    with self.subTest(option=option, field=field):
                        source.write_text(valid, encoding="utf-8")
                        forwarded = [f"-Wp,{option},{dependency}", "-Wp,-DREQUIRED,-MP,-MT,custom_target,-UUNUSED"]
                        arguments = [compiler, "-Werror", "-c", str(source), *forwarded, "-o", str(object_file)]
                        original = subprocess.run(arguments, cwd=root, capture_output=True, text=True)
                        self.assertEqual(original.returncode, 0, original.stderr)
                        preserved = {path: path.read_bytes() for path in (dependency, object_file)}
                        command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                                   " ".join(shlex.quote(argument) for argument in arguments))
                        entry = {"directory": str(root), "file": str(source),
                                 field: arguments if field == "arguments" else command}
                        for text, failures in ((valid, 0), (valid + "#error syntax_check_must_fail\n", 1)):
                            source.write_text(text, encoding="utf-8")
                            checked, failed, diagnostics = check_compile.check_source(
                                (source, source.name, root, [entry]))
                            self.assertEqual((checked, failed), (1, failures), diagnostics)
                            if failures:
                                self.assertIn("syntax_check_must_fail", diagnostics)
                            for path, content in preserved.items():
                                self.assertEqual(path.read_bytes(), content, str(path))
                            self.assertEqual(set(root.iterdir()), {source, dependency, object_file})
                        dependency.unlink()
                        object_file.unlink()
                        source.write_text(valid, encoding="utf-8")
                        self.assertEqual(check_compile.check_source((source, source.name, root, [entry]))[:2], (1, 0))
                        self.assertEqual(set(root.iterdir()), {source})

    def test_dependency_only_arguments_are_removed(self):
        source = Path("unit.c").resolve()
        flags = ["-Wp,-MD,unit.d", "-Wp,-MMD,unit.d,-MP", "-Wp,-MF,unit.d,-MT,target,-MQ,quoted",
                 "-Wp,-MFunit.d,-MTtarget,-MQquoted", "-Wp,-M,-MM,-MG,-MP",
                 "-Wp,--write-dependencies,unit.d,--dependencies,--user-dependencies,--print-missing-file-dependencies",
                 "-Wp,--write-user-dependencies,unit.d"]
        for flag in flags:
            with self.subTest(flag=flag):
                self.assertEqual(check_compile.syntax_arguments(["cc", flag, "-c", str(source)], Path.cwd(), source),
                                 ["cc", "-fsyntax-only", str(source)])

    def test_other_preprocessor_arguments_are_preserved(self):
        source = Path("unit.c").resolve()
        for flag, expected in (("-Wp,-DNAME=value,-MD,unit.d,-I,include files", "-Wp,-DNAME=value,-I,include files"),
                               ("-Wp,-include,-MD,-MMD,unit.d", "-Wp,-include,-MD"),
                               ("-Wp,--include,-MD,--write-dependencies,unit.d", "-Wp,--include,-MD")):
            with self.subTest(flag=flag):
                self.assertEqual(check_compile.syntax_arguments(["cc", flag, str(source)], Path.cwd(), source),
                                 ["cc", "-fsyntax-only", expected, str(source)])

    def test_unsupported_options_and_response_operands_are_still_rejected(self):
        source = Path("unit.c").resolve()
        for flag in ("-Wp,-unknown-option", "-Wp,-MD,unit.d,-E", "-Wp,-MD,unit.d,-I,@operand.rsp",
                     "-Wp,-MD,@operand.rsp"):
            with self.subTest(flag=flag):
                with self.assertRaisesRegex(ValueError, "unsupported forwarded compiler option"):
                    check_compile.syntax_arguments(["cc", flag, str(source)], Path.cwd(), source)

    def test_missing_dependency_values_are_rejected(self):
        source = Path("unit.c").resolve()
        for option in ("-MD", "-MMD", "-MF", "-MT", "-MQ", "--write-dependencies", "--write-user-dependencies"):
            with self.subTest(option=option):
                with self.assertRaisesRegex(ValueError, "missing value"):
                    check_compile.syntax_arguments(["cc", f"-Wp,{option}", str(source)], Path.cwd(), source)


if __name__ == "__main__":
    unittest.main()
