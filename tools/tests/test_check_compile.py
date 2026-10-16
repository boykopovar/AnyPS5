import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_compile

TOOL = Path(__file__).resolve().parents[1] / "check_compile.py"


class CheckCompileTests(unittest.TestCase):
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
                              capture_output=True, text=True, env=environment)

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
        stub.write_text("import json\nprint(json.dumps(%r))\n" % [entry], encoding="utf-8")
        if os.name == "nt":
            (bin / "ninja.cmd").write_text(f'@"{sys.executable}" "{stub}"\n', encoding="utf-8")
        else:
            ninja = bin / "ninja"
            ninja.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{stub}"\n', encoding="utf-8")
            ninja.chmod(0o755)
        result = self.tool("unit.cpp", env={"PATH": f"{bin}{os.pathsep}{os.environ['PATH']}"})
        self.assertEqual(result.returncode, 0)
        self.assertIn("1 translation units compile", result.stdout)


if __name__ == "__main__":
    unittest.main()
