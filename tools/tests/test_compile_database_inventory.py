import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compile_database_inventory

SCRIPT = 'compile_database_inventory.py'
TOOLS = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    @unittest.skipUnless(hasattr(os, "mkfifo"), "requires a named pipe")
    def test_named_pipe_is_rejected_before_reading(self):
        path = self.root / "database.json"
        os.mkfifo(path)
        with patch.object(Path, "read_text", side_effect=AssertionError("unexpected read")), \
                self.assertRaises(SystemExit) as status:
            compile_database_inventory.main([str(path)])
        self.assertEqual(status.exception.code, 2)

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def run_tool(self, *args):
        return subprocess.run([sys.executable, str(TOOLS / SCRIPT), *map(str, args)], capture_output=True,
                              encoding="utf-8", timeout=10, env={**os.environ, "PATH": "", "PYTHONIOENCODING": "ascii"})

    def test_groups_raw_variants_without_commands_or_writes(self):
        path=self.root/"编译.json"
        items=[{"directory":"dir","file":"b.cpp","arguments":["missing-tool","-D名字=1","b.cpp"]},
               {"directory":"dir","file":"a.c","command":"missing-tool a.c"},
               {"directory":"dir","file":"b.cpp","command":"missing-tool -DALT b.cpp"}]
        path.write_text(json.dumps(items,ensure_ascii=False),encoding="utf-8-sig")
        saved=path.read_bytes(),path.stat().st_mtime_ns
        r=self.run_tool(path)
        self.assertEqual(r.returncode,0,r.stderr)
        data=json.loads(r.stdout)
        self.assertEqual(data["entries"],3)
        self.assertEqual([(u["file"],u["variants"]) for u in data["units"]],[("a.c",1),("b.cpp",2)])
        self.assertEqual(data["units"][1]["records"],[{"arguments":items[0]["arguments"]},{"command":items[2]["command"]}])
        self.assertEqual(saved,(path.read_bytes(),path.stat().st_mtime_ns))
        self.assertEqual(list(self.root.iterdir()),[path])

    def test_same_file_in_different_directories_forms_separate_units(self):
        path = self.root / "compile_commands.json"
        items = [{"directory": "build-b", "file": "a.c", "command": "missing-compiler -DFIRST a.c"},
                 {"directory": "build-a", "file": "a.c", "arguments": ["missing-compiler", "-c", "a.c"]},
                 {"directory": "build-b", "file": "a.c", "command": "missing-compiler -DSECOND a.c"}]
        path.write_text(json.dumps(items), encoding="utf-8")
        before = path.read_bytes(), path.stat().st_mtime_ns
        result = self.run_tool(path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {"entries": 3, "units": [
            {"directory": "build-a", "file": "a.c", "variants": 1, "records": [
                {"arguments": ["missing-compiler", "-c", "a.c"]}]},
            {"directory": "build-b", "file": "a.c", "variants": 2, "records": [
                {"command": "missing-compiler -DFIRST a.c"},
                {"command": "missing-compiler -DSECOND a.c"}]}]})
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)
        self.assertEqual(list(self.root.iterdir()), [path])

    def test_empty_and_invalid_databases(self):
        path=self.root/"db.json"
        for value in ({},[{}],[{"directory":"x","file":"y","arguments":[]}],"bad"):
            path.write_text(json.dumps(value),encoding="utf-8")
            r=self.run_tool(path)
            self.assertEqual(r.returncode,2,r.stdout)
            self.assertEqual(r.stdout,"")
        path.write_text("[]",encoding="utf-8")
        self.assertEqual(json.loads(self.run_tool(path).stdout),{"entries":0,"units":[]})

    def test_non_string_output_is_rejected_before_printing_inventory(self):
        path = self.root / "compile_commands.json"
        for output in ("1e400", "null", "false", "[]", "{}"):
            with self.subTest(output=output):
                path.write_text('[{"directory":"dir","file":"a.c","command":"cc a.c","output":'
                                + output + '}]', encoding="utf-8")
                result = self.run_tool(path)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("entry 0: output must be a string", result.stderr)

    def test_optional_outputs_distinguish_identical_compile_actions(self):
        path = self.root / "compile_commands.json"
        arguments = ["missing-compiler", "-c", "a.c"]
        common = {"directory": "dir", "file": "a.c"}
        items = [{**common, "arguments": arguments, "command": "ignored command", "output": "a.o"},
                 {**common, "arguments": arguments, "output": "b.o"},
                 {**common, "command": "missing-compiler a.c", "output": "命令.o"},
                 {**common, "arguments": arguments},
                 {**common, "arguments": arguments, "output": ""}]
        path.write_text(json.dumps(items, ensure_ascii=False), encoding="utf-8")
        before = path.read_bytes(), path.stat().st_mtime_ns
        result = self.run_tool(path)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {"entries": 5, "units": [
            {"directory": "dir", "file": "a.c", "variants": 5, "records": [
                {"arguments": arguments, "output": "a.o"},
                {"arguments": arguments, "output": "b.o"},
                {"command": "missing-compiler a.c", "output": "命令.o"},
                {"arguments": arguments},
                {"arguments": arguments, "output": ""}]}]})
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)
        self.assertEqual(list(self.root.iterdir()), [path])



    def test_ninja_log_joins_latest_outputs_and_reports_coverage(self):
        db = self.root / "compile_commands.json"
        log = self.root / ".ninja_log"
        common = {"directory": "C:/original/build", "file": "one.c", "command": "cc one.c"}
        db.write_text(json.dumps([
            {**common, "output": "CMakeFiles/one.dir/one.c.obj"},
            {**common, "output": "c:/ORIGINAL/build/CMakeFiles/one.dir/one.c.obj"},
            {**common, "output": "CMakeFiles/two.dir/one.c.obj"},
            {**common, "output": "CMakeFiles/three.dir/one.c.obj"},
            {**common},
        ]), encoding="utf-8")
        log.write_text(
            "# ninja log v5\n"
            "10\t40\t1\tCMakeFiles/one.dir/one.c.obj\tone\n"
            "13\t30\t1\tCMakeFiles/two.dir/one.c.obj\ttwo\n"
            "2\t8\t1\tCMakeFiles\\one.dir\\ONE.c.obj\tone\n"
            "0\t9\t1\toutput.exe\tlink\n",
            encoding="utf-8",
        )
        before = [(p.read_bytes(), p.stat().st_mtime_ns) for p in (db, log)]
        result = self.run_tool(db, "--ninja-log", log)
        self.assertEqual(result.returncode, 0, result.stderr)
        parsed = json.loads(result.stdout)
        self.assertEqual([r.get("ninja_duration_ms") for r in parsed["units"][0]["records"]], [6, 6, 17, None, None])
        self.assertEqual(parsed["ninja_timings"], {
            "compile_outputs": 3,
            "matched_compile_outputs": 2,
            "entries_without_output": 1,
            "summed_compile_duration_ms": 23,
            "other_outputs": [{"output": "output.exe", "duration_ms": 9}],
        })
        self.assertEqual([(p.read_bytes(), p.stat().st_mtime_ns) for p in (db, log)], before)

    def test_explicit_ninja_working_directory_for_nested_compile_directory(self):
        db = self.root / "compile_commands.json"
        log = self.root / ".ninja_log"
        db.write_text(json.dumps([{
            "directory": "/original/build/nested",
            "file": "one.c", "command": "cc one.c", "output": "one.o",
        }]), encoding="utf-8")
        log.write_text("# ninja log v7\n0\t1\t7\tnested/one.o\tcommand\n", encoding="utf-8")
        result = self.run_tool(db, "--ninja-log", log, "--ninja-build-dir", "/original/build")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["units"][0]["records"][0]["ninja_duration_ms"], 1)
        self.assertEqual(report["ninja_timings"]["matched_compile_outputs"], 1)

    def test_invalid_ninja_logs_leave_stdout_empty(self):
        db = self.root / "compile_commands.json"
        log = self.root / ".ninja_log"
        db.write_text("[]", encoding="utf-8")
        cases = (
            ("ninja: error\n", "unsupported Ninja build log header"),
            ("# ninja log v7\n0\t1\t7\t", "invalid Ninja build log record"),
            ("# ninja log v7\n2\t1\t7\tone.o\tcommand\n", "invalid Ninja build log record"),
        )
        for content, diagnostic in cases:
            with self.subTest(content=content):
                log.write_text(content, encoding="utf-8")
                result = self.run_tool(db, "--ninja-log", log)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertIn(diagnostic, result.stderr)
                self.assertEqual(result.stdout, "")
        result = self.run_tool(db, "--ninja-log", self.root)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn("Ninja log must be a regular file", result.stderr)
        self.assertEqual(result.stdout, "")

    @unittest.skipUnless(
        shutil.which("cmake") and shutil.which("ninja") and (shutil.which("gcc") or shutil.which("cc")),
        "CMake, Ninja and a C compiler are required",
    )
    def test_real_cmake_ninja_build_reports_each_compiled_variant(self):
        source, build = self.root / "source with spaces", self.root / "out"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.20)\n"
            "project(test_ninja_cost C)\n"
            "add_library(first STATIC shared.c)\n"
            "add_library(second STATIC shared.c)\n"
            "target_compile_definitions(second PRIVATE SECOND)\n",
            encoding="utf-8",
        )
        (source / "shared.c").write_text("int number(void) { return 1; }\n", encoding="utf-8")
        subprocess.run(
            ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
             "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
             f"-DCMAKE_C_COMPILER={shutil.which('gcc') or shutil.which('cc')}"],
            check=True, capture_output=True, timeout=60,
        )
        subprocess.run(["cmake", "--build", str(build)], check=True, capture_output=True, timeout=60)
        result = self.run_tool(build / "compile_commands.json", "--ninja-log", build / ".ninja_log")
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report["entries"], 2)
        self.assertEqual(len(report["units"]), 1)
        unit = report["units"][0]
        self.assertEqual(unit["variants"], 2)
        timings = report["ninja_timings"]
        self.assertEqual(timings["matched_compile_outputs"], 2)
        self.assertEqual(timings["summed_compile_duration_ms"], sum(r["ninja_duration_ms"] for r in unit["records"]))
        archive_names = {Path(r["output"]).stem.removeprefix("lib") for r in timings["other_outputs"]}
        self.assertEqual(archive_names, {"first", "second"})


if __name__ == "__main__":
    unittest.main()
