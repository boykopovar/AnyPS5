import argparse
import json
import os
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_compile

TOOL = Path(__file__).resolve().parents[1] / "check_compile.py"


def is_app_execution_alias(program):
    return getattr(os.lstat(program), "st_reparse_tag", 0) == getattr(stat, "IO_REPARSE_TAG_APPEXECLINK", None)


def spawnable_python():
    for program, options in ((sys.executable, ()), (shutil.which("py"), ("-3",))):
        if program and Path(program).is_file() and not is_app_execution_alias(program):
            return [program, *options]
    return None


class CheckCompileFixture(unittest.TestCase):
    def setUp(self):
        holder = tempfile.TemporaryDirectory()
        self.addCleanup(holder.cleanup)
        self.root = Path(holder.name).resolve()
        subprocess.run(["git", "init", "-q", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "config", "user.email", "tests@example.com"], check=True)
        subprocess.run(["git", "-C", str(self.root), "config", "user.name", "tests"], check=True)
        self.build = self.root / "build"
        self.build.mkdir()

    def tool(self, *arguments, env=None):
        environment = dict(os.environ)
        environment.update(env or {})
        return subprocess.run([sys.executable, "-I", str(TOOL), *arguments], cwd=self.root,
                              capture_output=True, text=True, encoding="utf-8", env=environment)

    def source(self, relative, text="int x;\n"):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def compiler(self, body, name="compiler.py"):
        script = self.root / name
        script.write_text(body, encoding="utf-8")
        return script

    def entry(self, relative, command):
        return {"directory": str(self.build), "command": command, "file": str(self.root / relative)}

    def manifest(self, entries):
        (self.build / "compile_commands.json").write_text(json.dumps(entries), encoding="utf-8")

    def failing(self, relative, message=None):
        source = self.source(relative)
        message = message if message is not None else f"{relative}:1:1: error: no\n"
        script = self.compiler("import sys\nsys.stderr.write(%r)\nsys.exit(1)\n" % message)
        self.manifest([self.entry(relative, f'"{sys.executable}" "{script}" "{source}"')])
        return source


class CheckCompileTests(CheckCompileFixture):
    def test_a_translation_unit_that_does_not_compile_fails_with_the_compiler_output(self):
        self.failing("unit.cpp")
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 1)
        self.assertIn("unit.cpp:1:1: error: no", result.stderr)
        self.assertIn("1 of 1 translation units did not compile", result.stderr)

    def test_a_translation_unit_that_compiles_passes(self):
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"')])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)


class CheckCompileDatabaseNulTests(unittest.TestCase):
    def test_rejects_embedded_nul_in_command_fields(self):
        import check_compile
        for record in ({"directory": "build", "file": "unit.c", "command": "cc\0 unit.c"},
                       {"directory": "build", "file": "unit.c", "arguments": ["cc", "unit\0.c"]}):
            with self.subTest(record=record):
                with self.assertRaisesRegex(ValueError, "NUL"):
                    check_compile.compilation_database(json.dumps([record]), "compile_commands.json")


class CheckCompileRecordedCommandTests(CheckCompileFixture):
    def test_the_command_is_rewritten_to_syntax_only_without_object_or_dependency_files(self):
        source = self.source("unit.cpp")
        script = self.compiler(
            "import json\nimport sys\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\n")
        seen = self.root / "arguments.json"
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{seen}" -c -o unit.o '
                                                f'-MD -MT unit.o -MF unit.o.d "{source}"')])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(seen.read_text(encoding="utf-8")), ["-fsyntax-only", str(source)])

    def test_compile_commands_accepts_utf8_bom(self):
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        entry = self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"')
        (self.build / "compile_commands.json").write_text("\ufeff" + json.dumps([entry]), encoding="utf-8")
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)

    def test_every_configuration_is_checked_and_each_source_is_counted_once(self):
        source = self.source("unit.cpp")
        script = self.compiler(
            "import json\nimport sys\nfrom pathlib import Path\n"
            "with Path(sys.argv[1]).open('a') as log:\n"
            "    log.write(json.dumps([str(Path.cwd()), sys.argv[2:]]) + '\\n')\n"
            "if sys.argv[2] == '1':\n"
            "    sys.stderr.write(sys.argv[3] + ': configuration failed\\n')\n"
            "sys.exit(int(sys.argv[2]))\n")
        seen = self.root / "invocations.jsonl"
        second = self.build / "second"
        second.mkdir()
        for statuses in ((0, 0), (0, 1), (1, 0), (1, 1)):
            with self.subTest(statuses=statuses):
                seen.write_text("", encoding="utf-8")
                first = self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{seen}" '
                                              f'{statuses[0]} first -c -o first.o "{source}"')
                other = {"directory": str(second), "file": os.path.relpath(source, second),
                         "arguments": [sys.executable, str(script), str(seen),
                                       str(statuses[1]), "second", "-c", "-o", "second.o",
                                       os.path.relpath(source, second)]}
                self.manifest([first, other])
                result = self.tool("unit.cpp")
                self.assertEqual(result.returncode, int(any(statuses)), result.stderr)
                invocations = [json.loads(line) for line in seen.read_text(encoding="utf-8").splitlines()]
                self.assertEqual(invocations, [
                    [str(self.build), [str(statuses[0]), "first", "-fsyntax-only", str(source)]],
                    [str(second), [str(statuses[1]), "second", "-fsyntax-only", str(source)]],
                ])
                if any(statuses):
                    self.assertIn("1 of 1 translation units did not compile", result.stderr)
                else:
                    self.assertIn("1 translation units compile", result.stdout)
                for label, status in zip(("first", "second"), statuses):
                    self.assertEqual(f"{label}: configuration failed" in result.stderr, bool(status))

    def test_a_later_configuration_with_a_real_compiler_error_fails(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check configuration-dependent compilation")
        source = self.source("unit.c", "#ifdef SECOND_TARGET\n#error second target must fail\n#endif\nint x;\n")
        self.manifest([
            {"directory": str(self.build), "file": str(source),
             "arguments": [compiler, *flags, "-c", str(source)]}
            for flags in ([], ["-DSECOND_TARGET"])
        ])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn("second target must fail", result.stderr)
        self.assertIn("1 of 1 translation units did not compile", result.stderr)

    @unittest.skipUnless(os.name == "nt", "Windows command-line quoting")
    def test_windows_paths_and_quoted_defines_survive_the_syntax_check(self):
        source = self.source("unit.cpp")
        script = self.compiler(
            "import json\nimport sys\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\n")
        seen = self.root / "arguments.json"
        flags = ['-DNAME=quoted "value"', '-IC:\\sdk files\\include\\']
        arguments = [sys.executable, str(script), str(seen), *flags,
                     "-c", "-o", "unit.o", "-MD", "-MF", "unit.o.d", str(source)]
        self.manifest([self.entry("unit.cpp", subprocess.list2cmdline(arguments))])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(seen.read_text(encoding="utf-8")),
                         ["-fsyntax-only", *flags, str(source)])

    def test_nothing_checked_is_a_failure(self):
        self.manifest([])
        result = self.tool("orphan.cpp")
        self.assertEqual(result.returncode, 1)
        self.assertIn("orphan.cpp: no compile command in", result.stderr)
        self.assertIn("no translation unit to check in", result.stderr)

    def test_a_translation_unit_outside_the_build_is_reported_but_the_others_decide(self):
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"')])
        result = self.tool("unit.cpp", "orphan.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)
        self.assertIn("orphan.cpp: no compile command in", result.stderr)

    def test_an_absolute_path_names_the_same_translation_unit(self):
        source = self.failing("unit.cpp")
        result = self.tool(str(source))
        self.assertEqual(result.returncode, 1)
        self.assertIn("unit.cpp:1:1: error: no", result.stderr)

    def test_a_relative_source_argument_in_the_command_is_replaced(self):
        source = self.source("unit.cpp")
        script = self.compiler(
            "import json\nimport sys\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\n")
        seen = self.root / "arguments.json"
        relative = os.path.relpath(source, self.build)
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{seen}" "{relative}"')])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(seen.read_text(encoding="utf-8")), ["-fsyntax-only", str(source)])

    def test_a_build_directory_that_is_not_a_child_of_the_checkout_is_matched(self):
        self.build = self.root / "out" / "build"
        self.build.mkdir(parents=True)
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"')])
        result = self.tool("--build", str(self.build), "unit.cpp")
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)

    def test_a_build_directory_beside_the_checkout_is_matched(self):
        repo = self.root / "AnyPS5"
        repo.mkdir()
        subprocess.run(["git", "init", "-q", str(repo)], check=True)
        sibling = self.root / "sibling-build"
        sibling.mkdir()
        source = repo / "src" / "unit.cpp"
        source.parent.mkdir()
        source.write_text("int x;\n", encoding="utf-8")
        script = self.compiler("import sys\nsys.exit(0)\n")
        entry = {"directory": str(sibling), "command": f'"{sys.executable}" "{script}" "{source}"',
                 "file": str(source)}
        (sibling / "compile_commands.json").write_text(json.dumps([entry]), encoding="utf-8")
        result = subprocess.run([sys.executable, "-I", str(TOOL), "--build", str(sibling), "src/unit.cpp"],
                                cwd=repo, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("1 translation units compile", result.stdout)

    def test_a_generated_translation_unit_outside_the_checkout_is_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory).resolve() / "generated unit.cpp"
            source.write_text("int x;\n", encoding="utf-8")
            seen = self.root / "arguments.json"
            script = self.compiler(
                "import json\nimport sys\nfrom pathlib import Path\n"
                "Path(sys.argv[1]).write_text(json.dumps(sys.argv[3:]))\n"
                "if sys.argv[2] == '1': sys.stderr.write('external source: error: no\\n')\n"
                "sys.exit(int(sys.argv[2]))\n")
            for relative in (False, True):
                for status in (0, 1):
                    with self.subTest(relative=relative, status=status):
                        self.manifest([{
                            "directory": str(self.build),
                            "file": os.path.relpath(source, self.build) if relative else str(source),
                            "arguments": [sys.executable, str(script), str(seen), str(status),
                                          "-c", "-o", "generated.o", str(source)],
                        }])
                        path = os.path.relpath(source, self.root) if relative else str(source)
                        result = self.tool(path)
                        self.assertNotIn("no compile command", result.stderr)
                        self.assertEqual(result.returncode, status, result.stdout + result.stderr)
                        self.assertEqual(json.loads(seen.read_text(encoding="utf-8")),
                                         ["-fsyntax-only", str(source)])
                        if status:
                            self.assertIn("external source: error: no", result.stderr)
                            self.assertIn("1 of 1 translation units did not compile", result.stderr)
                        else:
                            self.assertIn("1 translation units compile", result.stdout)


class CheckCompilePathSelectionTests(CheckCompileFixture):
    def test_a_changed_file_with_a_non_ascii_name_is_checked(self):
        self.failing("caf\u00e9.cpp")
        subprocess.run(["git", "-C", str(self.root), "add", "caf\u00e9.cpp"], check=True)
        subprocess.run(["git", "-C", str(self.root), "commit", "-qm", "add"], check=True)
        self.source("caf\u00e9.cpp", "int y;\n")
        result = self.tool("--base", "HEAD")
        self.assertEqual(result.returncode, 1)
        self.assertIn("error: no", result.stderr)

    def test_a_longer_path_does_not_shadow_the_translation_unit(self):
        source = self.source("src/foo.cpp")
        other = self.source("plugins/x/src/foo.cpp")
        passes = self.compiler("import sys\nsys.exit(0)\n", name="passes.py")
        fails = self.compiler(
            "import sys\nsys.stderr.write('src/foo.cpp:1:1: error: no\\n')\nsys.exit(1)\n", name="fails.py")
        self.manifest([
            self.entry("plugins/x/src/foo.cpp", f'"{sys.executable}" "{passes}" "{other}"'),
            self.entry("src/foo.cpp", f'"{sys.executable}" "{fails}" "{source}"'),
        ])
        result = self.tool("src/foo.cpp")
        self.assertEqual(result.returncode, 1)
        self.assertIn("src/foo.cpp:1:1: error: no", result.stderr)

    def test_symbolic_link_loops_are_reported_without_a_traceback(self):
        loop = self.root / "loop.cpp"
        other = self.root / "other.cpp"
        try:
            loop.symlink_to(other)
            other.symlink_to(loop)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"'),
                       self.entry("other.cpp", f'"{sys.executable}" "{script}" "{other}"')])
        result = self.tool("unit.cpp", "loop.cpp")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("loop.cpp", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        looping_build = self.root / "looping-build"
        looping_build.symlink_to(looping_build)
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"'),
                       {"directory": str(looping_build), "file": "other.cpp", "command": "cc other.cpp"},
                       {"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), str(loop), str(source), "--", str(loop)]}])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)
        self.assertNotIn("Traceback", result.stderr)
        response = self.build / "flags.rsp"
        response.symlink_to(response)
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), "@flags.rsp", str(source)]}])
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("flags.rsp", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_dry_run_symbolic_link_loops_are_reported_without_a_traceback(self):
        loop = self.root / "loop.cpp"
        other = self.root / "other.cpp"
        try:
            loop.symlink_to(other)
            other.symlink_to(loop)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        source = self.source("unit.cpp")
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\nsys.exit(1)\n")
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{invoked}" -c -o unit.o "{source}"'),
                       self.entry("other.cpp", f'"{sys.executable}" "{script}" "{invoked}" "{other}"')])
        control = self.tool("--dry-run", "unit.cpp")
        self.assertEqual(control.returncode, 0, control.stdout + control.stderr)
        self.assertEqual(control.stderr, "")
        self.assertEqual(json.loads(control.stdout), {
            "translation_units": [{
                "source": str(source),
                "commands": [{
                    "directory": str(self.build),
                    "arguments": [sys.executable, str(script), str(invoked), "-fsyntax-only", str(source)],
                }],
            }],
        })
        self.assertFalse(invoked.exists())
        for arguments in (("unit.cpp", "loop.cpp"), ("loop.cpp",)):
            with self.subTest(arguments=arguments):
                result = self.tool("--dry-run", *arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("loop.cpp", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(invoked.exists())
        subprocess.run(["git", "-C", str(self.root), "add", "unit.cpp"], check=True)
        subprocess.run(["git", "-C", str(self.root), "commit", "-qm", "add"], check=True)
        result = self.tool("--dry-run", "--base", "HEAD")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("loop.cpp", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(invoked.exists())
        looping_build = self.root / "looping-build"
        looping_build.symlink_to(looping_build)
        self.manifest([self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{invoked}" "{source}"'),
                       {"directory": str(looping_build), "file": "other.cpp", "command": "cc other.cpp"},
                       {"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), str(invoked), str(loop), str(source), "--", str(loop)]}])
        result = self.tool("--dry-run", "unit.cpp")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        plan = json.loads(result.stdout)
        self.assertEqual(len(plan["translation_units"]), 1)
        self.assertEqual(plan["translation_units"][0]["source"], str(source))
        self.assertEqual(len(plan["translation_units"][0]["commands"]), 2)
        self.assertFalse(invoked.exists())
        response = self.build / "flags.rsp"
        response.write_text('"-DREQUIRED=1" "-c" "-o" "unit.o"', encoding="utf-8")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), str(invoked), "@flags.rsp", str(source)]}])
        result = self.tool("--dry-run", "unit.cpp")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(json.loads(result.stdout), {
            "translation_units": [{
                "source": str(source),
                "commands": [{
                    "directory": str(self.build),
                    "arguments": [sys.executable, str(script), str(invoked), "-fsyntax-only", "-DREQUIRED=1", str(source)],
                }],
            }],
        })
        self.assertFalse(invoked.exists())
        response.unlink()
        response.symlink_to(response)
        result = self.tool("--dry-run", "unit.cpp")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("flags.rsp", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(invoked.exists())
        dangling = self.root / "dangling.cpp"
        dangling.symlink_to(self.root / "missing.cpp")
        for missing_arg, resolved in (("orphan.cpp", self.root / "orphan.cpp"),
                                      ("dangling.cpp", self.root / "missing.cpp")):
            with self.subTest(missing_arg=missing_arg):
                result = self.tool("--dry-run", missing_arg)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertEqual(result.stderr, "")
                self.assertEqual(json.loads(result.stdout), {
                    "translation_units": [{"source": str(resolved), "commands": []}],
                })

    def test_syntax_arguments_does_not_call_path_resolve_on_the_source(self):
        loop = self.root / "loop.cpp"
        other = self.root / "other.cpp"
        try:
            loop.symlink_to(other)
            other.symlink_to(loop)
        except (NotImplementedError, OSError) as error:
            self.skipTest(f"symbolic links are unavailable: {error}")
        check_compile.syntax_arguments([sys.executable, "loop.cpp"], self.build, loop)

    def test_a_new_untracked_translation_unit_is_checked(self):
        self.source("base.cpp")
        subprocess.run(["git", "-C", str(self.root), "add", "base.cpp"], check=True)
        subprocess.run(["git", "-C", str(self.root), "commit", "-qm", "add"], check=True)
        self.failing("brandnew.cpp")
        result = self.tool("--base", "HEAD")
        self.assertEqual(result.returncode, 1)
        self.assertIn("brandnew.cpp:1:1: error: no", result.stderr)

    def test_a_missing_build_directory_is_rejected(self):
        result = self.tool("--build", "nowhere", "unit.cpp")
        self.assertEqual(result.returncode, 2)
        self.assertIn("not a directory", result.stderr)

    def test_the_changed_translation_units_are_taken_from_the_base(self):
        self.failing("changed.cpp")
        subprocess.run(["git", "-C", str(self.root), "add", "changed.cpp"], check=True)
        subprocess.run(["git", "-C", str(self.root), "commit", "-qm", "add"], check=True)
        self.source("changed.cpp", "int y;\n")
        result = self.tool("--base", "HEAD")
        self.assertEqual(result.returncode, 1)
        self.assertIn("changed.cpp:1:1: error: no", result.stderr)

    def test_files_changed_only_on_the_base_since_the_branch_point_are_not_checked(self):
        script = self.compiler("import sys\nsys.stderr.write(sys.argv[-1] + ': error: no\\n')\nsys.exit(1)\n")
        self.manifest([self.entry(name, f'"{sys.executable}" "{script}" "{self.root / name}"')
                       for name in ("mine.cpp", "other.cpp")])
        git = ["git", "-C", str(self.root)]
        self.source("other.cpp")
        subprocess.run([*git, "add", "other.cpp"], check=True)
        subprocess.run([*git, "commit", "-qm", "add"], check=True)
        subprocess.run([*git, "branch", "-M", "main"], check=True)
        subprocess.run([*git, "checkout", "-qb", "side"], check=True)
        self.source("mine.cpp")
        subprocess.run([*git, "add", "mine.cpp"], check=True)
        subprocess.run([*git, "commit", "-qm", "mine"], check=True)
        subprocess.run([*git, "checkout", "-q", "main"], check=True)
        self.source("other.cpp", "int y;\n")
        subprocess.run([*git, "commit", "-qam", "other"], check=True)
        subprocess.run([*git, "checkout", "-q", "side"], check=True)
        result = self.tool("--base", "main")
        self.assertEqual(result.returncode, 1)
        self.assertIn("mine.cpp: error: no", result.stderr)
        self.assertNotIn("other.cpp", result.stderr)

    def test_no_changed_translation_unit_is_not_a_failure(self):
        self.source("clean.cpp")
        subprocess.run(["git", "-C", str(self.root), "add", "clean.cpp"], check=True)
        subprocess.run(["git", "-C", str(self.root), "commit", "-qm", "add"], check=True)
        self.manifest([])
        result = self.tool("--base", "HEAD")
        self.assertEqual(result.returncode, 0)
        self.assertIn("no changed translation unit", result.stdout)

    def test_the_commands_come_from_ninja_when_the_manifest_is_absent(self):
        source = self.source("unit.cpp")
        script = self.compiler("import sys\nsys.exit(0)\n")
        entry = self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{source}"')
        bin = self.root / "bin"
        bin.mkdir()
        stub = bin / "ninja_stub.py"
        stub.write_text("import json\nimport sys\n"
                        "if sys.argv[1:] == ['-t', 'wincodepage']:\n    print('Build file encoding: UTF-8')\n"
                        "else:\n    print(json.dumps(%r))\n" % [entry], encoding="utf-8")
        if os.name == "nt":
            (bin / "ninja.cmd").write_text('@"%NINJA_STUB_PYTHON%" "%~dp0ninja_stub.py" %*\n', encoding="ascii")
        else:
            ninja = bin / "ninja"
            ninja.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{stub}"\n', encoding="utf-8")
            ninja.chmod(0o755)
        result = self.tool("unit.cpp", env={"PATH": f"{bin}{os.pathsep}{os.environ['PATH']}",
                                            "NINJA_STUB_PYTHON": sys.executable})
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)

    def test_argument_arrays_preserve_tokens_and_take_precedence_over_command(self):
        source = self.source("unit with spaces.cpp")
        script = self.compiler(
            "import json\nimport sys\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\n")
        seen = self.root / "arguments.json"
        flags = ['-DNAME=quoted "value"', '-IC:\\sdk files\\include\\']
        arguments = [sys.executable, str(script), str(seen), *flags,
                     "-c", "-o", "unit.o", "-MD", "-MT", "unit.o", "-MF", "unit.o.d",
                     os.path.relpath(source, self.build)]
        entry = {"directory": str(self.build), "file": str(source), "arguments": arguments}
        for command in (None, "nonexistent-compiler -c unit.cpp"):
            with self.subTest(command=command):
                if command is not None:
                    entry["command"] = command
                self.manifest([entry])
                result = self.tool(source.name)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(json.loads(seen.read_text(encoding="utf-8")),
                                 ["-fsyntax-only", *flags, str(source)])

    def test_a_cmake_env_launcher_keeps_its_environment_and_the_compiler_command_is_rewritten(self):
        cmake = shutil.which("cmake")
        if cmake is None:
            self.skipTest("cmake is not on PATH")
        source = self.source("unit.cpp")
        script = self.compiler(
            "import json\nimport os\nimport sys\nfrom pathlib import Path\n"
            "Path(sys.argv[1]).write_text(json.dumps([sys.argv[2:], os.environ.get('CHECK_COMPILE_SET'), "
            "os.environ.get('CHECK_COMPILE_UNSET')]))\n")
        seen = self.root / "arguments.json"
        self.manifest([self.entry("unit.cpp", f'"{cmake}" -E env CHECK_COMPILE_SET=pch_defines,time_macros '
                                                f'--unset=CHECK_COMPILE_UNSET "{sys.executable}" "{script}" "{seen}" '
                                                f'-include unit.hpp -c -o unit.o "{source}"')])
        result = self.tool("unit.cpp", env={"CHECK_COMPILE_UNSET": "1"})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(seen.read_text(encoding="utf-8")),
                         [["-fsyntax-only", "-include", "unit.hpp", str(source)], "pch_defines,time_macros", None])

    def test_parallel_checks_overlap_but_diagnostics_keep_source_order(self):
        barrier = self.root / "barrier"
        barrier.mkdir()
        compiler = self.compiler(
            "import sys, time\nfrom pathlib import Path\n"
            "root, name = Path(sys.argv[1]), sys.argv[2]\n"
            "(root / name).touch()\n"
            "deadline = time.monotonic() + 5\n"
            "while not all((root / value).exists() for value in ('first', 'second')):\n"
            "    if time.monotonic() > deadline: sys.exit('checks did not overlap')\n"
            "    time.sleep(0.01)\n"
            "if name == 'second': (root / 'second.done').touch()\n"
            "while name == 'first' and not (root / 'second.done').exists():\n"
            "    if time.monotonic() > deadline: sys.exit('second did not finish')\n"
            "    time.sleep(0.01)\n"
            "sys.exit(name + ': diagnostic')\n")
        entries = []
        for name in ("first", "second"):
            source = self.source(name + ".c")
            entries.append(self.entry(name + ".c", f'"{sys.executable}" "{compiler}" "{barrier}" {name} "{source}"'))
        self.manifest(entries)
        result = self.tool("--jobs", "2", "first.c", "second.c")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("2 of 2 translation units did not compile", result.stderr)
        self.assertLess(result.stderr.index("first: diagnostic"), result.stderr.index("second: diagnostic"))
        self.assertNotIn("did not overlap", result.stderr)
        for jobs in ("0", "-1"):
            with self.subTest(jobs=jobs):
                result = self.tool("--jobs", jobs, "first.c")
                self.assertEqual(result.returncode, 2)
                self.assertIn("positive integer", result.stderr)


class CheckCompileDuplicateCompilationDatabaseKeysTests(unittest.TestCase):
    def test_rejects_duplicate_keys_in_compilation_records(self):
        import check_compile
        for content in ('[{"directory":"a","directory":"b","file":"a.c","command":"cc a.c"}]',
                        '[{"directory":"a","file":"a.c","command":"cc a.c","command":"cc b.c"}]'):
            with self.subTest(content=content):
                with self.assertRaisesRegex(ValueError, "duplicate"):
                    check_compile.compilation_database(content, "compile_commands.json")


class CheckCompileDatabaseTests(CheckCompileFixture):
    def test_invalid_argument_arrays_are_rejected_without_a_traceback(self):
        source = self.source("unit.cpp")
        for arguments in (None, [], "compiler -c unit.cpp", ["compiler", 3], [""]):
            with self.subTest(arguments=arguments):
                self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
                result = self.tool(source.name)
                self.assertEqual(result.returncode, 2)
                self.assertIn("arguments must be a nonempty list of strings with an executable", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_invalid_compilation_databases_are_rejected_without_a_traceback(self):
        for database in ({}, {"file": "unit.cpp"}, None, 42, "commands"):
            with self.subTest(database=database):
                self.manifest(database)
                result = self.tool("unit.cpp")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("compile_commands.json: expected an array of command objects", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_excessively_nested_compilation_databases_are_rejected_without_a_traceback(self):
        depth = sys.getrecursionlimit() + 64
        while True:
            text = "[" * depth + "]" * depth
            try:
                json.loads(text)
            except RecursionError:
                break
            depth *= 2
            if depth > 10_000_000:
                self.fail("the JSON decoder accepts every probed nesting depth")
        (self.build / "compile_commands.json").write_text(text, encoding="utf-8")
        result = self.tool("unit.cpp")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertNotIn("expected an array of command objects", result.stderr)

    def test_a_recursion_error_outside_the_database_parser_is_not_a_usage_error(self):
        source = self.source("unit.cpp")
        with patch.object(check_compile, "repository", return_value=self.root):
            with patch.object(check_compile, "check", side_effect=RecursionError("unrelated")):
                with self.assertRaises(RecursionError):
                    check_compile.main(["--build", str(self.build), str(source)])

    def test_non_object_entries_are_rejected_before_any_compiler_runs(self):
        self.check_invalid_entries(lambda valid: [(value, "expected a command object")
                                                  for value in (None, [], 42, "command")])

    def test_invalid_directories_are_rejected_before_any_compiler_runs(self):
        self.check_invalid_locations("directory")

    def test_invalid_files_are_rejected_before_any_compiler_runs(self):
        self.check_invalid_locations("file")

    def test_invalid_commands_are_rejected_before_any_compiler_runs(self):
        def cases(valid):
            return [({key: value for key, value in valid.items() if key != "command"},
                     "expected arguments or command"),
                    *(({**valid, "command": value}, "command must be a nonempty string")
                      for value in (None, [], 42, "", " \t")),
                    ({**valid, "arguments": []},
                     "arguments must be a nonempty list of strings with an executable")]
        self.check_invalid_entries(cases)

    def check_invalid_locations(self, field):
        def cases(valid):
            missing = dict(valid)
            del missing[field]
            return [(missing, f"missing {field}"),
                    *(({**valid, field: value}, f"{field} must be a nonempty string")
                      for value in (None, [], 42, ""))]
        self.check_invalid_entries(cases)

    def check_invalid_entries(self, cases):
        source = self.source("unit.cpp")
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\n")
        valid = self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{invoked}" "{source}"')
        for invalid, message in cases(valid):
            with self.subTest(invalid=invalid):
                self.manifest([valid, invalid])
                result = self.tool("--jobs", "2", "unit.cpp")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn(f"compile_commands.json: entry 1: {message}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(invoked.exists(), result.stdout + result.stderr)

    def test_invalid_ninja_databases_are_validated_before_compilation(self):
        source = self.source("unit.cpp")
        cases = (({}, "expected an array of command objects"),
                 ([self.entry("unit.cpp", "compiler"), None], "entry 1: expected a command object"))
        for database, message in cases:
            with self.subTest(database=database), \
                    patch.object(check_compile.shutil, "which", return_value="ninja"), \
                    patch.object(check_compile, "ninja_encoding", return_value="utf-8"), \
                    patch.object(check_compile, "capture", return_value=subprocess.CompletedProcess(
                        [], 0, json.dumps(database), "")), \
                    patch.object(check_compile.subprocess, "run") as compiler:
                with self.assertRaisesRegex(ValueError, "ninja -t compdb: " + message):
                    check_compile.check(self.root, self.build, [source])
                compiler.assert_not_called()

    def test_index_groups_entries_by_checkout_relative_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "checkout"
            build = Path(directory) / "build"
            (root / "src").mkdir(parents=True)
            build.mkdir()
            database = [
                {"directory": str(root), "file": "src/a.cpp", "command": "cc -c src/a.cpp"},
                {"directory": str(root / "src"), "file": "a.cpp", "command": "cc -DX -c a.cpp"},
                {"directory": str(root), "file": "src/b.cpp", "command": "cc -c src/b.cpp"},
            ]
            (build / "compile_commands.json").write_text(json.dumps(database), encoding="utf-8")
            commands = check_compile.index(build, root)
            self.assertEqual(sorted(commands), ["src/a.cpp", "src/b.cpp"])
            self.assertEqual(commands["src/a.cpp"], database[:2])
            self.assertEqual(commands["src/b.cpp"], database[2:])


class CheckCompileOutputTests(CheckCompileFixture):
    def test_dependency_options_do_not_make_syntax_checks_fail_or_rewrite_outputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check unused dependency options with -Werror")
        source = self.source("unit.c")
        object_file = self.build / "unit.o"
        dependency_file = self.build / "unit.d"
        for option in ("-MF", "-MT", "-MQ"):
            for joined in (False, True):
                with self.subTest(option=option, joined=joined):
                    source.write_text("int x;\n", encoding="utf-8")
                    value = str(dependency_file) if option == "-MF" else "target with spaces.o"
                    flags = [option + value] if joined else [option, value]
                    if option != "-MF":
                        flags += ["-MF", str(dependency_file)]
                    arguments = [compiler, "-Werror", "-MMD", *flags,
                                 "-c", str(source), "-o", str(object_file)]
                    original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
                    self.assertEqual(original.returncode, 0, original.stderr)
                    outputs = {path: path.read_bytes() for path in (object_file, dependency_file)}
                    self.manifest([{"directory": str(self.build), "file": str(source),
                                    "arguments": arguments}])
                    result = self.tool("unit.c")
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn("1 translation units compile", result.stdout)
                    for path, content in outputs.items():
                        self.assertEqual(path.read_bytes(), content)
                    source.write_text("#error syntax_check_must_fail\n", encoding="utf-8")
                    result = self.tool("unit.c")
                    self.assertEqual(result.returncode, 1, result.stdout)
                    self.assertIn("syntax_check_must_fail", result.stderr)
                    for path, content in outputs.items():
                        self.assertEqual(path.read_bytes(), content)

    def test_ninja_response_file_is_expanded_before_syntax_rewriting(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if compiler is None or shutil.which("ninja") is None:
            self.skipTest("needs Ninja and a C compiler")
        source = self.source("unit.c", "#ifndef REQUIRED\n#error missing definition\n#endif\nint x;\n")
        command = subprocess.list2cmdline([compiler]) if os.name == "nt" else '"' + compiler + '"'
        (self.build / "build.ninja").write_text(
            "rule cc\n  command = " + command + " @$out.rsp\n"
            "  rspfile = $out.rsp\n  rspfile_content = -DREQUIRED -c $in -o $out -MD -MF $out.d\n"
            "build order: phony\nbuild unit.o: cc ../unit.c || order\n", encoding="utf-8")
        self.assertFalse((self.build / "unit.o.rsp").exists())
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)
        self.assertFalse((self.build / "unit.o").exists())
        self.assertFalse((self.build / "unit.o.d").exists())
        self.assertFalse((self.build / "unit.o.rsp").exists())

    def test_ninja_commands_in_a_non_ascii_build_directory_are_found(self):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if compiler is None or shutil.which("ninja") is None:
            self.skipTest("needs Ninja and a C compiler")
        name = "ação"
        build = self.root / name
        if os.name == "nt":
            try:
                str(build).encode("mbcs", "strict")
            except UnicodeEncodeError:
                self.skipTest("the ANSI code page cannot represent the build directory")
        build.mkdir()
        self.source("unit.c")
        command = subprocess.list2cmdline([compiler]) if os.name == "nt" else '"' + compiler + '"'
        (build / "build.ninja").write_text(
            "rule cc\n  command = " + command + " -c $in -o $out\nbuild unit.o: cc ../unit.c\n", encoding="utf-8")
        result = self.tool("--build", name, "unit.c")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)

    def test_windows_ninja_output_is_decoded_in_its_declared_build_file_encoding(self):
        cases = (("Build file encoding: UTF-8\n", 0, "utf-8"), ("Build file encoding: ANSI\n", 0, "mbcs"),
                 ("Build file encoding: OEM\n", 0, None), ("", 1, None))
        for declared, status, expected in cases:
            with self.subTest(declared=declared, status=status):
                result = subprocess.CompletedProcess([], status, declared, "ninja: error: unknown tool" if status else "")
                with patch.object(check_compile.sys, "platform", "win32"), \
                        patch.object(check_compile, "capture", return_value=result):
                    if expected is None:
                        with self.assertRaises(ValueError):
                            check_compile.ninja_encoding("ninja")
                    else:
                        self.assertEqual(check_compile.ninja_encoding("ninja"), expected)
        with patch.object(check_compile.sys, "platform", "linux"), \
                patch.object(check_compile, "capture", side_effect=AssertionError("queried Ninja")):
            self.assertEqual(check_compile.ninja_encoding("ninja"), "utf-8")

    def test_syntax_check_preserves_existing_clang_compilation_fragment(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check compilation-fragment side effects")
        source = self.source("unit.c")
        fragment = self.build / "fragment.json"
        for option in (f'-MJ "{fragment}"', f'-MJ"{fragment}"'):
            with self.subTest(option=option):
                fragment.write_bytes(b"previous compilation fragment")
                self.manifest([self.entry("unit.c", f'"{compiler}" -c {option} "{source}"')])
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(fragment.read_bytes(), b"previous compilation fragment")

    def test_saved_temporaries_do_not_rewrite_files_or_hide_compiler_errors(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if compiler is None:
            self.skipTest("needs a C compiler")
        source = self.source("unit.c")
        for option, field in (("-save-temps", "arguments"), ("--save-temps", "command"),
                              ("-save-temps=cwd", "arguments"), ("-save-temps=obj", "command")):
            with self.subTest(option=option, field=field):
                source.write_text("#ifndef REQUIRED\n#error missing definition\n#endif\nint value;\n", encoding="utf-8")
                arguments = [compiler, "-Werror", "-DREQUIRED", option, "-c", str(source),
                             "-o", str(self.build / "unit.o")]
                original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
                self.assertEqual(original.returncode, 0, original.stderr)
                for name in ("a-unit", "a-unit.i", "unit.i"):
                    (self.build / name).write_bytes(b"previous build output")
                command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                           " ".join(shlex.quote(argument) for argument in arguments))
                self.manifest([{"directory": str(self.build), "file": str(source),
                                field: arguments if field == "arguments" else command}])
                preserved = {path: path.read_bytes() for path in self.build.iterdir()}
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("1 translation units compile", result.stdout)
                for path, content in preserved.items():
                    self.assertEqual(path.read_bytes(), content, str(path))
                self.assertEqual(set(self.build.iterdir()), set(preserved))
                source.write_text("#error syntax_check_must_fail\n", encoding="utf-8")
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn("syntax_check_must_fail", result.stderr)
                for path, content in preserved.items():
                    self.assertEqual(path.read_bytes(), content, str(path))
                self.assertEqual(set(self.build.iterdir()), set(preserved))

    def test_unmodeled_saved_temporary_modes_are_rejected_before_the_compiler_runs(self):
        source = self.source("unit.c")
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\n")
        for option in ("-save-temps=invalid", "-save-temps=diag", "--save-temps=diag",
                       "-save-temps=CWD", "-save-temps="):
            with self.subTest(option=option):
                self.manifest([{"directory": str(self.build), "file": str(source),
                                "arguments": [sys.executable, str(script), str(invoked), option,
                                              "-c", str(source), "-o", str(self.build / "unit.o")]}])
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn(f"unsupported saved-temporary mode in {option}", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(invoked.exists(), result.stdout + result.stderr)
                self.assertFalse((self.build / "unit.o").exists(), result.stdout + result.stderr)


class CheckCompileResponseTests(CheckCompileFixture):
    def test_response_forwarded_dependencies_preserve_outputs(self):
        compilers = list(dict.fromkeys(compiler for compiler in (shutil.which("gcc"), shutil.which("clang")) if compiler))
        if not compilers:
            self.skipTest("needs GCC or Clang")
        source = self.source("unit.c", "#ifndef REQUIRED\n#error missing definition\n#endif\nint value;\n")
        response = self.build / "flags.rsp"
        outputs = [self.build / name for name in ("unit.o", "dependency with spaces.d")]
        response.write_text(" ".join('"' + value + '"' for value in
                                    ("-c", source.as_posix(), "-o", outputs[0].as_posix(),
                                     "-Wp,-D,REQUIRED=1", "-Wp,-MD," + outputs[1].as_posix())), encoding="utf-8")
        for compiler in compilers:
            arguments = [compiler, "@" + str(response)]
            original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True, check=False)
            self.assertEqual(original.returncode, 0, original.stderr)
            outputs[1].write_bytes(b"existing dependency output\n")
            preserved = {path: path.read_bytes() for path in (source, response, *outputs)}
            for field in ("arguments", "command"):
                for jobs in ("1", "2"):
                    with self.subTest(compiler=compiler, field=field, jobs=jobs):
                        command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                                   " ".join(shlex.quote(argument) for argument in arguments))
                        self.manifest([{"directory": str(self.build), "file": str(source),
                                        field: arguments if field == "arguments" else command}])
                        result = self.tool("--jobs", jobs, "unit.c")
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        for path, data in preserved.items():
                            self.assertEqual(path.read_bytes(), data, path.name)

    @unittest.skipUnless(os.name == "nt", "Windows GCC response-file text mode")
    def test_windows_gcc_response_text_mode_is_not_reinterpreted(self):
        compiler = shutil.which("gcc")
        if compiler is None:
            self.skipTest("needs GCC's driver wrapper")
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        recorder = self.compiler("import json, sys\nprint(json.dumps(sys.argv[1:]))\n")
        interpreter = spawnable_python()
        if interpreter is None:
            self.skipTest("needs a Python interpreter that is not an App Execution Alias")
        wrapper = ",".join([*interpreter, str(recorder)])
        arguments = [compiler, "-wrapper", wrapper,
                     "-c", str(source), "-o", str(self.build / "unit.o"), "@flags.rsp"]
        cases = (
            ('"-DVALUE=A\nB"', ["VALUE=A\nB"], False),
            ('"-DVALUE=A\rB"', ["VALUE=A\rB"], False),
            ('"-DVALUE=A\r\nB"', ["VALUE=A\nB"], True),
            ("'-DVALUE=A\r\nB'", ["VALUE=A\nB"], True),
            ("-DVALUE=first\x1a -DAFTER=second", ["VALUE=first"], True),
            ("-DVALUE=first\\\x1a -DAFTER=second", ["VALUE=first"], True),
        )
        for content, expected, rejected in cases:
            with self.subTest(content=content):
                response.write_bytes(content.encode("utf-8"))
                original = subprocess.run(arguments, cwd=self.build, capture_output=True,
                                          text=True, encoding="utf-8", check=False)
                self.assertEqual(original.returncode, 0, original.stderr)
                commands = [json.loads(line) for line in original.stdout.splitlines() if line.startswith("[")]
                definitions = [command[index + 1] for command in commands
                               for index, argument in enumerate(command[:-1]) if argument == "-D"]
                self.assertEqual(definitions, expected)
                if rejected:
                    with self.assertRaisesRegex(ValueError, "response-file"):
                        check_compile.expand_response_files(["@flags.rsp"], self.build)
                else:
                    self.assertEqual(check_compile.expand_response_files(["@flags.rsp"], self.build),
                                     ["-D" + value for value in expected])

    def test_response_files_contain_options_without_an_executable_name(self):
        response = self.build / "flags.rsp"
        cases = (
            ("", []),
            (" \t\r\n", []),
            ("  -DA=7 -DB=11", ["-DA=7", "-DB=11"]),
            ("-DA=7\n-DB=11\n", ["-DA=7", "-DB=11"]),
            ("-DA=7\r\n-DB=11\r\n", ["-DA=7", "-DB=11"]),
            (r'-DNAME=\"hello\" -DB=11', ['-DNAME="hello"', "-DB=11"]),
            ('"-DNAME=hello world" -DB=11', ["-DNAME=hello world", "-DB=11"]),
        )
        for content, expected in cases:
            with self.subTest(content=content):
                response.write_bytes(content.encode("utf-8"))
                self.assertEqual(check_compile.expand_response_files(["@flags.rsp"], self.build), expected)

    def test_empty_response_options_compile_without_changing_build_outputs(self):
        self.check_response_option_outputs((
            ("", "int x;\n"),
            (" \t\r\n", "int x;\n"),
        ))

    def test_response_definitions_compile_without_changing_build_outputs(self):
        self.check_response_option_outputs((
            ("  -DA=7 -DB=11", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
            ("-DA=7\n-DB=11\n", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
        ))

    def test_crlf_and_quoted_response_definitions_compile_without_changing_build_outputs(self):
        self.check_response_option_outputs((
            ("-DA=7\r\n-DB=11\r\n", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
            (r'-DNAME=\"hello\" -DB=11', '_Static_assert(sizeof(NAME) == 6 && B == 11, "quoted definition");\n'),
        ))

    def check_response_option_outputs(self, cases):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if compiler is None:
            self.skipTest("needs a GNU-compatible C compiler")
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        output_options = self.build / "output flags.rsp"
        outputs = [self.build / name for name in ("unit.o", "unit.d")]
        output_options.write_text(" ".join('"' + value + '"' for value in
                                          ("-c", source.as_posix(), "-MMD", "-MF",
                                           outputs[1].as_posix(), "-o", outputs[0].as_posix())), encoding="utf-8")
        arguments = [compiler, "@" + str(response), "@" + str(output_options)]
        for content, text in cases:
            source.write_text(text, encoding="utf-8")
            response.write_bytes(content.encode("utf-8"))
            original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True, check=False)
            self.assertEqual(original.returncode, 0, original.stderr)
            preserved = {path: path.read_bytes() for path in (source, response, output_options, *outputs)}
            for field in ("arguments", "command"):
                for jobs in ("1", "2"):
                    with self.subTest(content=content, field=field, jobs=jobs):
                        command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                                   " ".join(shlex.quote(argument) for argument in arguments))
                        self.manifest([{"directory": str(self.build), "file": str(source),
                                        field: arguments if field == "arguments" else command}])
                        result = self.tool("--jobs", jobs, "unit.c")
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn("1 translation units compile", result.stdout)
                        for path, data in preserved.items():
                            self.assertEqual(path.read_bytes(), data, path.name)

    def test_ambiguous_response_options_fail_before_the_compiler_runs(self):
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\n")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), str(invoked), "@flags.rsp"]}])
        for content in (r"'-DC=C:\includes'", r'"-DC=C:\includes"', r'"-DHOME=\$HOME"',
                        "-DA=\\\n7", '""', "\ufeff-DA=7", "\v-DA=7", "-DA=\0",
                        '"-DA=A\r\nB"', "'-DA=A\r\nB'", "-DA=7\x1a -DB=11", "-DA=7\\\x1a -DB=11"):
            with self.subTest(content=content):
                response.write_bytes(content.encode("utf-8"))
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("response-file", result.stderr)
                self.assertNotIn("Traceback", result.stderr)
                self.assertFalse(invoked.exists())

    def test_cl_mode_response_files_are_rejected_before_execution(self):
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        response.write_text("-DA=7", encoding="utf-8")
        for arguments in (["clang-cl", "@flags.rsp"], ["cl.exe", "@flags.rsp"],
                          ["clang", "--driver-mode=cl", "@flags.rsp"],
                          ["clang", "--driver-mode=cl", "--rsp-quoting=windows", "@flags.rsp"]):
            with self.subTest(arguments=arguments):
                self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("cl-mode compiler response files are unsupported", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_windows_quoted_response_files_follow_the_clang_tokenizer(self):
        response = self.build / "flags.rsp"
        cases = (
            (r'-DA=a\\"b c"', ["-DA=a\\b c"]),
            (r'-DA=a\\\"b', ['-DA=a\\"b']),
            (r"-DA=c:\dir\ -DB=x\\y", ["-DA=c:\\dir\\", "-DB=x\\\\y"]),
            (r'"-DA=p""q"', ['-DA=p"q']),
            (r'"-DA=p"""q"', ['-DA=p"q']),
            (r'-DA=p""q', ["-DA=pq"]),
            (r'-D "" -DB', ["-D", "", "-DB"]),
            ('""', [""]),
            ("-DA=x\vy -DB=x\fy", ["-DA=x\vy", "-DB=x\fy"]),
            ('-DA "-DB=x y', ["-DA", "-DB=x y"]),
            ("-DA\r\n-DB\t-DC\n", ["-DA", "-DB", "-DC"]),
            ('"-DA=x\r\ny"', ["-DA=x\r\ny"]),
            ("-DA='x y'", ["-DA='x", "y'"]),
            ("\\", ["\\"]),
            ('"\\"', ['"']),
            ("", []),
            (" \t\r\n", []),
        )
        for content, expected in cases:
            with self.subTest(content=content):
                response.write_bytes(content.encode("utf-8"))
                self.assertEqual(check_compile.expand_response_files(["@flags.rsp"], self.build, windows=True), expected)

    def test_windows_quoted_response_files_decode_byte_order_marks(self):
        response = self.build / "flags.rsp"
        cases = (
            (b"\xef\xbb\xbf-DA=c:\\dir", ["-DA=c:\\dir"]),
            (b"\xff\xfe" + '-DA="x y" -DB=\u00e9'.encode("utf-16-le"), ["-DA=x y", "-DB=\u00e9"]),
            (b"\xfe\xff" + '-DA="x y"'.encode("utf-16-be"), ["-DA=x y"]),
        )
        for content, expected in cases:
            with self.subTest(content=content):
                response.write_bytes(content)
                self.assertEqual(check_compile.expand_response_files(["@flags.rsp"], self.build, windows=True), expected)
        for content in (b"-DA=\0", b"\xff\xfe-", b"-DA=\xff", "-DNOBOM".encode("utf-16-le")):
            with self.subTest(content=content):
                response.write_bytes(content)
                with self.assertRaises(ValueError):
                    check_compile.expand_response_files(["@flags.rsp"], self.build, windows=True)

    def test_windows_quoting_applies_to_nested_response_files_relative_to_the_build_directory(self):
        (self.build / "sub").mkdir()
        (self.build / "sub" / "outer.rsp").write_text("@inner.rsp -DA=c:\\outer", encoding="utf-8")
        (self.build / "sub" / "inner.rsp").write_text("-DWRONG", encoding="utf-8")
        (self.build / "inner.rsp").write_text('"-DB=x y" -DC=c:\\inner', encoding="utf-8")
        self.assertEqual(check_compile.expand_response_files(["@sub/outer.rsp"], self.build, windows=True),
                         ["-DB=x y", "-DC=c:\\inner", "-DA=c:\\outer"])

    def test_the_last_top_level_rsp_quoting_option_selects_the_tokenizer(self):
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        recorded = self.root / "recorded.json"
        script = self.compiler("import json, sys\nfrom pathlib import Path\n"
                               f"Path({str(recorded)!r}).write_text(json.dumps(sys.argv[1:]))\n")
        cases = (
            ("-DA=c:\\dir", ["--rsp-quoting=windows"], "-DA=c:\\dir"),
            ("-DA=c:\\dir", ["--rsp-quoting=windows", "--rsp-quoting=posix"], "-DA=c:dir"),
            ("-DA=c:\\dir", ["--rsp-quoting=posix", "--rsp-quoting=windows"], "-DA=c:\\dir"),
            ("-DA=c:\\dir", ["@flags.rsp", "--rsp-quoting=windows"], "-DA=c:\\dir"),
            ("--rsp-quoting=windows -DA=c:\\dir", [], "-DA=c:dir"),
        )
        for content, options, expected in cases:
            with self.subTest(content=content, options=options):
                response.write_text(content, encoding="utf-8")
                arguments = [sys.executable, str(script), *options]
                if "@flags.rsp" not in options:
                    arguments.append("@flags.rsp")
                self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn(expected, json.loads(recorded.read_text(encoding="utf-8")))

    def test_an_unknown_rsp_quoting_value_is_rejected_before_execution(self):
        source = self.source("unit.c")
        (self.build / "flags.rsp").write_text("-DA=7", encoding="utf-8")
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\n")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [sys.executable, str(script), str(invoked), "--rsp-quoting=cl", "@flags.rsp"]}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("unsupported response-file quoting --rsp-quoting=cl", result.stderr)
        self.assertNotIn("Traceback", result.stderr)
        self.assertFalse(invoked.exists())

    def test_windows_quoted_response_tokens_match_clang(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to compare Windows response-file quoting")
        prefix = "error: no such file or directory: '"
        empty_error = prefix + "'" in subprocess.run(
            [compiler, "-###", "--rsp-quoting=windows", "-fsyntax-only", "-x", "c", ""],
            cwd=self.build, capture_output=True, text=True, encoding="utf-8").stderr
        response = self.build / "flags.rsp"
        for content in (r'a\\"b c"', 'a\\\\\\"b', "x\\ y", '"', '""', '"""', '""""', 'a""b', '"a""b"',
                        'x"y z', 'q\\\\"', '"\\"', "'a b'", "a\tb\r\nc", "a\vb\fc", '"a\nb" c'):
            with self.subTest(content=content):
                if not empty_error and content in ('"', '""'):
                    continue
                response.write_bytes(("-- " + content).encode("utf-8"))
                result = subprocess.run([compiler, "-###", "--rsp-quoting=windows", "-fsyntax-only", "-x", "c", "@flags.rsp"],
                                        cwd=self.build, capture_output=True, text=True, encoding="utf-8")
                tokens = [part[:part.rindex("'")] for part in result.stderr.split(prefix)[1:]]
                self.assertEqual(check_compile.expand_response_files(["@flags.rsp"], self.build, windows=True)[1:], tokens)

    def test_windows_quoted_response_files_compile_without_changing_build_outputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to compile Windows-quoted response files")
        source = self.source("unit.c", "_Static_assert(VALUE == 'x', \"Windows quoting keeps single quotes\");\n"
                                       "_Static_assert(sizeof(TEXT) == 4, \"Windows quoting keeps backslashes\");\n")
        outputs = [self.build / name for name in ("unit.o", "unit dependency.d")]
        response = self.build / "flags.rsp"
        response.write_text(f"-DVALUE='x' -DTEXT=\\\"a\\\\b\\\" -c \"{source}\" -o \"{outputs[0]}\" -MD -MF \"{outputs[1]}\"",
                            encoding="utf-8")
        arguments = [compiler, "--rsp-quoting=windows", "@flags.rsp"]
        original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
        self.assertEqual(original.returncode, 0, original.stderr)
        preserved = {path: path.read_bytes() for path in [source, response, *outputs]}
        self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)
        for path, content in preserved.items():
            self.assertEqual(path.read_bytes(), content, path.name)

    def test_response_files_are_expanded_without_rewriting_build_outputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check compilation-fragment side effects")
        source = self.source("unit.c")
        outputs = [self.build / name for name in ("unit.o", "unit.d", "fragment.json")]
        response = self.build / "unit flags.rsp"
        response.write_text(" ".join('"' + argument + '"' for argument in
                                    ("-c", source.as_posix(), "-MMD", "-MF", outputs[1].as_posix(),
                                     "-MJ", outputs[2].as_posix(), "-o", outputs[0].as_posix())), encoding="utf-8")
        arguments = [compiler, "@" + str(response)]
        original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
        self.assertEqual(original.returncode, 0, original.stderr)
        preserved = {path: path.read_bytes() for path in [source, response, *outputs]}
        for field in ("arguments", "command"):
            for jobs in ("1", "2"):
                with self.subTest(field=field, jobs=jobs):
                    command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                               " ".join(shlex.quote(argument) for argument in arguments))
                    self.manifest([{"directory": str(self.build), "file": str(source),
                                    field: arguments if field == "arguments" else command}])
                    result = self.tool("--jobs", jobs, "unit.c")
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("1 translation units compile", result.stdout)
                    self.assertNotIn("Traceback", result.stderr)
                    for path, content in preserved.items():
                        self.assertEqual(path.read_bytes(), content)

    def test_nested_response_files_are_expanded_without_rewriting_build_outputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check compilation-fragment side effects")
        source = self.source("unit.c")
        outputs = [self.build / name for name in ("unit.o", "unit.d", "fragment.json")]
        inner = self.build / "inner.rsp"
        inner.write_text(" ".join('"' + argument + '"' for argument in
                                  ("-MMD", "-MF", outputs[1].as_posix(),
                                   "-MJ", outputs[2].as_posix(), "-o", outputs[0].as_posix())), encoding="utf-8")
        outer = self.build / "outer.rsp"
        outer.write_text(f'"-c" "{source.as_posix()}" "@{inner.as_posix()}"', encoding="utf-8")
        arguments = [compiler, "@" + str(outer)]
        original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
        self.assertEqual(original.returncode, 0, original.stderr)
        preserved = {path: path.read_bytes() for path in [source, inner, outer, *outputs]}
        self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)
        self.assertNotIn("Traceback", result.stderr)
        for path, content in preserved.items():
            self.assertEqual(path.read_bytes(), content)

    def test_a_response_file_cycle_is_reported_without_a_traceback(self):
        source = self.source("unit.c")
        first = self.build / "first.rsp"
        second = self.build / "second.rsp"
        first.write_text(f'"@{second.as_posix()}"', encoding="utf-8")
        second.write_text(f'"@{first.as_posix()}"', encoding="utf-8")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": ["clang", "@" + str(first)]}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("response file cycle", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_a_response_file_relative_to_the_build_directory_is_found(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check response file resolution")
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        response.write_text(f'"-c" "{source.as_posix()}"', encoding="utf-8")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [compiler, "@flags.rsp"]}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("1 translation units compile", result.stdout)

    def test_a_missing_response_file_is_reported_without_a_traceback(self):
        source = self.source("unit.c")
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": ["clang", "@" + str(self.build / "missing.rsp")]}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def check_response_options(self, cases):
        compiler = shutil.which("clang") or shutil.which("gcc")
        if compiler is None:
            self.skipTest("needs a GNU-compatible C compiler")
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        output_options = self.build / "output flags.rsp"
        outputs = [self.build / name for name in ("unit.o", "unit.d")]
        output_options.write_text(" ".join('"' + value + '"' for value in
                                          ("-c", source.as_posix(), "-MMD", "-MF",
                                           outputs[1].as_posix(), "-o", outputs[0].as_posix())), encoding="utf-8")
        arguments = [compiler, "@" + str(response), "@" + str(output_options)]
        for content, text in cases:
            source.write_text(text, encoding="utf-8")
            response.write_bytes(content.encode("utf-8"))
            original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True, check=False)
            self.assertEqual(original.returncode, 0, original.stderr)
            preserved = {path: path.read_bytes() for path in (source, response, output_options, *outputs)}
            for field in ("arguments", "command"):
                for jobs in ("1", "2"):
                    with self.subTest(content=content, field=field, jobs=jobs):
                        command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                                   " ".join(shlex.quote(argument) for argument in arguments))
                        self.manifest([{"directory": str(self.build), "file": str(source),
                                        field: arguments if field == "arguments" else command}])
                        result = self.tool("--jobs", jobs, "unit.c")
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertIn("1 translation units compile", result.stdout)
                        for path, data in preserved.items():
                            self.assertEqual(path.read_bytes(), data, path.name)


    def test_response_macro_options_preserve_build_outputs(self):
        self.check_response_options((('  -DA=7 -DB=11', '_Static_assert(A == 7 && B == 11, "definitions");\n'), ('-DA=7\n-DB=11\n', '_Static_assert(A == 7 && B == 11, "definitions");\n')))

    def test_response_crlf_and_quoted_options_preserve_build_outputs(self):
        self.check_response_options((('-DA=7\r\n-DB=11\r\n', '_Static_assert(A == 7 && B == 11, "definitions");\n'), ('-DNAME=\\"hello\\" -DB=11', '_Static_assert(sizeof(NAME) == 6 && B == 11, "quoted definition");\n')))

    def test_unexpanded_response_files_cannot_rewrite_build_outputs(self):
        compiler = shutil.which("clang")
        if compiler is None:
            self.skipTest("Clang is required to check compilation-fragment side effects")
        source = self.source("unit.c")
        outputs = [self.build / name for name in ("unit.o", "unit.d", "fragment.json")]
        response = self.build / "unit flags.rsp"
        response.write_text(" ".join('"' + argument + '"' for argument in
                                    ("-c", source.as_posix(), "-MMD", "-MF", outputs[1].as_posix(),
                                     "-MJ", outputs[2].as_posix(), "-o", outputs[0].as_posix())), encoding="utf-8")
        arguments = [compiler, "@" + str(response)]
        original = subprocess.run(arguments, cwd=self.build, capture_output=True, text=True)
        self.assertEqual(original.returncode, 0, original.stderr)
        preserved = {path: path.read_bytes() for path in [source, response, *outputs]}
        for field in ("arguments", "command"):
            for jobs in ("1", "2"):
                with self.subTest(field=field, jobs=jobs):
                    command = (subprocess.list2cmdline(arguments) if os.name == "nt" else
                               " ".join(shlex.quote(argument) for argument in arguments))
                    self.manifest([{"directory": str(self.build), "file": str(source),
                                    field: arguments if field == "arguments" else command}])
                    result = self.tool("--jobs", jobs, "unit.c")
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("1 translation units compile", result.stdout)
                    self.assertNotIn("Traceback", result.stderr)
                    for path, content in preserved.items():
                        self.assertEqual(path.read_bytes(), content)



class CheckCompileLauncherTests(unittest.TestCase):
    def test_a_cmake_env_prefix_is_kept_and_only_the_command_after_it_is_rewritten(self):
        source = Path.cwd() / "unit.cpp"
        for prefix in (["cmake", "-E", "env", "CCACHE_SLOPPINESS=pch_defines,time_macros"],
                       ["/usr/bin/cmake", "-E", "env", "--unset=CCACHE_DIR", "--modify", "PATH=path_list_prepend:/x", "--"],
                       ["C:\\Program Files\\CMake\\bin\\cmake.exe", "-E", "env", "A=1", "B="]):
            with self.subTest(prefix=prefix):
                self.assertEqual(check_compile.syntax_arguments(
                    [*prefix, "ccache", "g++", "-DNAME=1", "-include", "pch.hxx", "-c", "-o", "unit.o", str(source)],
                    Path.cwd(), source),
                    [*prefix, "ccache", "g++", "-fsyntax-only", "-DNAME=1", "-include", "pch.hxx", str(check_compile.real(source))])

    def test_env_delimiter_preserves_a_command_path_containing_equals(self):
        source = Path.cwd() / "unit.cpp"
        prefix = ["cmake", "-E", "env", "--"]
        command = "compiler=tools/g++"
        self.assertEqual(check_compile.syntax_arguments([*prefix, command, "-c", str(source)], Path.cwd(), source),
                         [*prefix, command, "-fsyntax-only", str(check_compile.real(source))])

    def test_only_cmake_starts_an_env_prefix(self):
        source = Path.cwd() / "unit.cpp"
        self.assertEqual(check_compile.syntax_arguments(["g++", "-E", "env", "-c", str(source)], Path.cwd(), source),
                         ["g++", "-fsyntax-only", "env", str(check_compile.real(source))])

    def test_an_env_prefix_without_a_command_or_with_an_unmodeled_option_is_rejected(self):
        source = Path.cwd() / "unit.cpp"
        for arguments, message in ((["cmake", "-E", "env", "A=1"], "no command"),
                                   (["cmake", "-E", "env", "A=1", "--"], "no command"),
                                   (["cmake", "-E", "env", "--modify"], "--modify"),
                                   (["cmake", "-E", "env", "-", "cc", "-c", str(source)], "unsupported"),
                                   (["cmake", "-E", "env", "--chdir=x", "cc", "-c", str(source)], "unsupported")):
            with self.subTest(arguments=arguments):
                with self.assertRaisesRegex(ValueError, message):
                    check_compile.syntax_arguments(arguments, Path.cwd(), source)


class CheckCompileValidateArgumentsTests(unittest.TestCase):
    def test_accepts_nonempty_list_of_strings(self):
        check_compile.validate_arguments(["cc", "-c", "a.c"])

    def test_rejects_non_lists_empty_lists_and_empty_executable(self):
        for arguments in ("cc -c", ("cc",), [], [""], None):
            with self.subTest(arguments=arguments):
                with self.assertRaises(ValueError):
                    check_compile.validate_arguments(arguments)

    def test_rejects_non_string_elements(self):
        with self.assertRaises(ValueError):
            check_compile.validate_arguments(["cc", 1])


class CheckCompileWindowsResponseTextTests(unittest.TestCase):
    def test_decodes_utf16_with_either_byte_order_mark(self):
        self.assertEqual(check_compile.windows_response_text("-DA=é".encode("utf-16")), "-DA=é")
        self.assertEqual(check_compile.windows_response_text(b"\xfe\xff" + "-DA=é".encode("utf-16-be")), "-DA=é")

    def test_strips_utf8_byte_order_mark(self):
        self.assertEqual(check_compile.windows_response_text(b"\xef\xbb\xbf-DA"), "-DA")

    def test_plain_utf8_is_decoded_and_invalid_bytes_raise(self):
        self.assertEqual(check_compile.windows_response_text("-DA=é".encode("utf-8")), "-DA=é")
        with self.assertRaises(UnicodeDecodeError):
            check_compile.windows_response_text(b"\xff\xff\xff")


class CheckCompileResponseQuotingTests(unittest.TestCase):
    def test_defaults_to_posix(self):
        self.assertEqual(check_compile.response_quoting([]), "posix")
        self.assertEqual(check_compile.response_quoting(["cc", "-c"]), "posix")

    def test_last_option_wins(self):
        arguments = ["cc", "--rsp-quoting=windows", "--rsp-quoting=posix"]
        self.assertEqual(check_compile.response_quoting(arguments), "posix")
        self.assertEqual(check_compile.response_quoting(["--rsp-quoting=windows"]), "windows")

    def test_unknown_quoting_is_refused(self):
        with self.assertRaisesRegex(ValueError, "unsupported response-file quoting --rsp-quoting=shell"):
            check_compile.response_quoting(["--rsp-quoting=shell"])


class CheckCompilePositiveIntegerTests(unittest.TestCase):
    def test_returns_positive_integers(self):
        self.assertEqual(check_compile.positive_integer("3"), 3)
        self.assertEqual(check_compile.positive_integer(" 12 "), 12)

    def test_zero_and_negative_values_are_argument_errors(self):
        for value in ("0", "-4"):
            with self.subTest(value=value):
                with self.assertRaises(argparse.ArgumentTypeError):
                    check_compile.positive_integer(value)

    def test_non_numbers_raise_value_error(self):
        with self.assertRaises(ValueError):
            check_compile.positive_integer("many")


class CheckCompileLauncherPrefixTests(unittest.TestCase):
    def test_non_cmake_commands_have_no_prefix(self):
        self.assertEqual(check_compile.launcher_prefix(["cc", "-c", "a.c"]), [])
        self.assertEqual(check_compile.launcher_prefix(["cmake", "-E"]), [])
        self.assertEqual(check_compile.launcher_prefix(["cmake", "-P", "env", "A=1", "cc"]), [])

    def test_environment_assignments_end_at_the_first_other_word(self):
        arguments = ["cmake", "-E", "env", "A=1", "B=2", "cc", "-c", "a=b.c"]
        self.assertEqual(check_compile.launcher_prefix(arguments), arguments[:5])

    def test_double_dash_ends_the_options(self):
        arguments = ["cmake", "-E", "env", "A=1", "--", "cc"]
        self.assertEqual(check_compile.launcher_prefix(arguments), arguments[:5])

    def test_modify_and_unset_options_are_skipped(self):
        arguments = ["cmake", "-E", "env", "--modify", "PATH=path_list_prepend:x", "--unset=A", "cc"]
        self.assertEqual(check_compile.launcher_prefix(arguments), arguments[:6])

    def test_executable_name_ignores_case_and_windows_directory(self):
        arguments = ["C:\\Tools\\CMake.exe", "-E", "env", "A=1", "cc"]
        self.assertEqual(check_compile.launcher_prefix(arguments), arguments[:4])

    def test_malformed_launchers_are_refused(self):
        for arguments in (["cmake", "-E", "env", "--bogus", "cc"], ["cmake", "-E", "env", "--modify"],
                          ["cmake", "-E", "env", "A=1"], ["cmake", "-E", "env"]):
            with self.subTest(arguments=arguments):
                with self.assertRaises(ValueError):
                    check_compile.launcher_prefix(arguments)


class CheckCompileEntryKeyTests(unittest.TestCase):
    def test_relative_file_is_resolved_against_the_entry_directory(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            entry = {"directory": str(root / "build"), "file": "../src/a.cpp"}
            self.assertEqual(check_compile.entry_key(entry, root), "src/a.cpp")

    def test_absolute_file_inside_root_is_root_relative(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            entry = {"directory": str(root), "file": str(root / "core" / "b.cpp")}
            self.assertEqual(check_compile.entry_key(entry, root), "core/b.cpp")

    def test_file_outside_root_keeps_its_absolute_path(self):
        with tempfile.TemporaryDirectory() as name, tempfile.TemporaryDirectory() as other:
            outside = Path(other) / "c.cpp"
            entry = {"directory": name, "file": str(outside)}
            self.assertEqual(check_compile.entry_key(entry, Path(name)), check_compile.real(outside).as_posix())


class CheckCompileResolveSourceTests(unittest.TestCase):
    def test_existing_file_inside_root_gets_a_relative_name(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "core").mkdir()
            file = root / "core" / "a.cpp"
            file.write_text("")
            source, key = check_compile.resolve_source(file, root)
            self.assertEqual(source, check_compile.real(file))
            self.assertEqual(key, "core/a.cpp")

    def test_missing_file_is_still_resolved(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source, key = check_compile.resolve_source(root / "gone.cpp", root)
            self.assertEqual(key, "gone.cpp")
            self.assertEqual(source, check_compile.real(root / "gone.cpp"))

    def test_file_outside_root_keeps_an_absolute_name(self):
        with tempfile.TemporaryDirectory() as name, tempfile.TemporaryDirectory() as other:
            file = Path(other) / "x.cpp"
            _, key = check_compile.resolve_source(file, Path(name))
            self.assertEqual(key, check_compile.real(file).as_posix())


if __name__ == "__main__":
    unittest.main()
