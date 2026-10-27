import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_compile

TOOL = Path(__file__).resolve().parents[1] / "check_compile.py"


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

    def test_invalid_entries_are_rejected_before_any_compiler_runs(self):
        source = self.source("unit.cpp")
        invoked = self.root / "invoked"
        script = self.compiler("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).touch()\n")
        valid = self.entry("unit.cpp", f'"{sys.executable}" "{script}" "{invoked}" "{source}"')
        cases = [(value, "expected a command object") for value in (None, [], 42, "command")]
        for field in ("directory", "file"):
            missing = dict(valid)
            del missing[field]
            cases.append((missing, f"missing {field}"))
            cases.extend(({**valid, field: value}, f"{field} must be a nonempty string")
                         for value in (None, [], 42, ""))
        cases.append(({key: value for key, value in valid.items() if key != "command"},
                      "expected arguments or command"))
        cases.extend(({**valid, "command": value}, "command must be a nonempty string")
                     for value in (None, [], 42, "", " \t"))
        cases.append(({**valid, "arguments": []},
                      "arguments must be a nonempty list of strings with an executable"))
        for invalid, message in cases:
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

    def test_rejected_saved_temporary_mode_is_not_hidden(self):
        compiler = shutil.which("gcc") or shutil.which("clang")
        if compiler is None:
            self.skipTest("needs a C compiler")
        source = self.source("unit.c")
        original = subprocess.run([compiler, "-save-temps=invalid", "-fsyntax-only", str(source)],
                                  cwd=self.build, capture_output=True, text=True)
        if original.returncode == 0:
            self.skipTest("compiler accepts unknown saved-temporary modes with -fsyntax-only")
        self.assertIn("save-temps", original.stderr)
        self.manifest([{"directory": str(self.build), "file": str(source),
                        "arguments": [compiler, "-save-temps=invalid", "-c", str(source),
                                      "-o", str(self.build / "unit.o")]}])
        result = self.tool("unit.c")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("save-temps", result.stderr)
        self.assertFalse((self.build / "unit.o").exists())


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
        arguments = [compiler, "-wrapper", sys.executable + "," + str(recorder),
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

    def test_response_options_compile_without_changing_build_outputs(self):
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
        cases = (
            ("", "int x;\n"),
            (" \t\r\n", "int x;\n"),
            ("  -DA=7 -DB=11", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
            ("-DA=7\n-DB=11\n", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
            ("-DA=7\r\n-DB=11\r\n", "_Static_assert(A == 7 && B == 11, \"definitions\");\n"),
            (r'-DNAME=\"hello\" -DB=11', '_Static_assert(sizeof(NAME) == 6 && B == 11, "quoted definition");\n'),
        )
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

    def test_windows_response_grammars_are_rejected_before_execution(self):
        source = self.source("unit.c")
        response = self.build / "flags.rsp"
        response.write_text("-DA=7", encoding="utf-8")
        for arguments in (["clang-cl", "@flags.rsp"], ["cl.exe", "@flags.rsp"],
                          ["clang", "--driver-mode=cl", "@flags.rsp"],
                          ["clang", "--rsp-quoting=windows", "@flags.rsp"]):
            with self.subTest(arguments=arguments):
                self.manifest([{"directory": str(self.build), "file": str(source), "arguments": arguments}])
                result = self.tool("unit.c")
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Windows-style compiler response files are unsupported", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

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


if __name__ == "__main__":
    unittest.main()
