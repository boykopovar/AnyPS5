import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
import unittest.mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import vulkan_validation_gate as gate
from test_check_compile import spawnable_python


def probe_command():
    interpreter = spawnable_python()
    if interpreter is None:
        return None
    return " ".join(f'"{part}"' for part in (Path(interpreter[0]).as_posix(), *interpreter[1:]))


PYTHON = probe_command()


class ReportTests(unittest.TestCase):
    def test_effective_timeouts_cannot_override_the_requested_limit(self):
        for value in (0, 2, -1, float("inf"), float("nan"), "1", True):
            with self.subTest(value=value), self.assertRaisesRegex(gate.GateError, "probe: TIMEOUT"):
                gate.check_timeouts([{"name": "probe", "properties": [{"name": "TIMEOUT", "value": value}]}], 1)
        for value in (.05, 1):
            gate.check_timeouts([{"name": "probe", "properties": [{"name": "TIMEOUT", "value": value}]}], 1)
        gate.check_timeouts([{"name": "default", "properties": []}], 1)
        with self.assertRaisesRegex(gate.GateError, "TIMEOUT_AFTER_MATCH"):
            gate.check_timeouts([{"name": "probe", "properties": [{"name": "TIMEOUT_AFTER_MATCH", "value": ["1", "ready"]}]}], 1)

    def report(self, text):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "junit.xml"
            path.write_text(text, encoding="utf-8")
            return gate.parse_report(path, {"probe"})

    def test_test_body_messages_are_distinct_from_configuration_echoes(self):
        text = '<testsuite><testcase name="probe"><system-out>'
        text += 'environment: VK_INSTANCE_LAYERS=VUID-Fake-1\n'
        text += 'Validation Error: [ VUID-Real-1 ] message\n'
        text += 'Validation Warning: [ SYNC-HAZARD-WRITE_AFTER_WRITE ] message\n'
        text += 'VUID-Old-2 (ERROR / SPEC): msgNum: 1\n'
        text += '</system-out><system-err>WARNING: [Loader Message] layer was not found but was requested by env var VK_INSTANCE_LAYERS</system-err></testcase></testsuite>'
        tests, problems = self.report(text)
        self.assertEqual(tests["probe"]["diagnostics"], {"VUID-Real-1": 1, "SYNC-HAZARD-WRITE_AFTER_WRITE": 1, "VUID-Old-2": 1})
        self.assertEqual(len(tests["probe"]["missing_layers"]), 1)
        self.assertEqual(problems, ["probe"])

    def test_unassigned_validation_errors_are_compared(self):
        inserted = 'Insert instance layer "VK_LAYER_KHRONOS_validation"'
        text = ('<testsuite><testcase name="probe"><system-err>' + inserted + '\n'
                'Validation Error: [ UNASSIGNED-Threading-Info ] invalid image handle\n'
                'Validation Error: [ UNASSIGNED-Device address out of bounds ] invalid address\n'
                'environment: UNASSIGNED-Fake-Configuration'
                '</system-err></testcase></testsuite>')
        tests, problems = self.report(text)
        self.assertEqual(tests["probe"]["diagnostics"], {
            "UNASSIGNED-Threading-Info": 1,
            "UNASSIGNED-Device address out of bounds": 1,
        })
        self.assertEqual(problems, [])
        report = gate.compare([
            {"side": "baseline", "iteration": 1, "returncode": 0, "problems": [],
             "tests": {"probe": {"diagnostics": {}}}},
            {"side": "candidate", "iteration": 1, "returncode": 0, "problems": [], "tests": tests},
        ])
        self.assertEqual(report["new_diagnostics"], {
            "probe": ["UNASSIGNED-Device address out of bounds", "UNASSIGNED-Threading-Info"]})
        self.assertEqual(report["exit_code"], 1)

    def test_a_test_without_the_layer_insertion_line_is_not_a_success(self):
        inserted = '[Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (layer.json)'
        for line, problems in ((inserted, []), ('Insert instance layer "VK_LAYER_OTHER" (layer.json)', ["probe"]), ("", ["probe"])):
            with self.subTest(line=line):
                tests, found = self.report('<testsuite><testcase name="probe"><system-err>' + line + '</system-err></testcase></testsuite>')
                self.assertEqual(tests["probe"]["layer_inserted"], not problems)
                self.assertEqual(found, problems)

    def test_skip_disabled_failure_and_missing_tests_are_not_successes(self):
        for body in ('<testcase name="probe"><skipped/></testcase>',
                     '<testcase name="probe" status="disabled"/>',
                     '<testcase name="probe"><failure/></testcase>', ''):
            with self.subTest(body=body):
                self.assertTrue(self.report('<testsuite>' + body + '</testsuite>')[1])

    def test_invalid_or_duplicate_test_names_are_setup_errors(self):
        for body in ('<testcase/>', '<testcase name="probe"/><testcase name="probe"/>'):
            with self.subTest(body=body), self.assertRaises(gate.GateError):
                self.report('<testsuite>' + body + '</testsuite>')

    def test_new_ids_are_compared_per_test_and_counts_are_retained(self):
        def iteration(side, test, messages):
            return {"side": side, "iteration": 1, "returncode": 0, "problems": [],
                    "tests": {test: {"diagnostics": messages}}}
        report = gate.compare([iteration("baseline", "probe", {"VUID-A-1": 1}),
                               iteration("candidate", "probe", {"VUID-A-1": 3})])
        self.assertEqual(report["exit_code"], 0)
        self.assertEqual(report["counts"]["candidate"]["probe"]["VUID-A-1"], 3)
        report = gate.compare([iteration("baseline", "other", {"VUID-A-1": 1}),
                               iteration("candidate", "probe", {"VUID-A-1": 1})])
        self.assertEqual(report["new_diagnostics"], {"probe": ["VUID-A-1"]})
        self.assertEqual(report["exit_code"], 1)

    def test_relative_executable_follows_the_test_working_directory(self):
        for mode in ("default", "absolute", "relative"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as directory:
                build = Path(directory).resolve()
                working = build if mode == "default" else build / "working directory"
                working.mkdir(exist_ok=True)
                executable = working / "probe"
                executable.write_bytes(b"original executable")
                properties = [] if mode == "default" else [{"name": "WORKING_DIRECTORY",
                    "value": str(working if mode == "absolute" else working.relative_to(build))}]
                tests = [{"name": "probe", "command": ["./probe"], "properties": properties}]
                before = gate.artifact_hashes(build, tests, [])
                self.assertIn(str(executable), before)
                executable.write_bytes(b"changed executable")
                self.assertNotEqual(before, gate.artifact_hashes(build, tests, []))

    def test_relative_command_files_follow_the_test_working_directory(self):
        for mode in ("default", "absolute", "relative"):
            for definition in (False, True):
                with self.subTest(mode=mode, definition=definition), tempfile.TemporaryDirectory() as directory:
                    build = Path(directory).resolve()
                    working = build if mode == "default" else build / "working directory"
                    working.mkdir(exist_ok=True)
                    script = working / "probe.py"
                    script.write_text("original", encoding="utf-8")
                    argument = "-DINPUT=probe.py" if definition else "probe.py"
                    properties = [] if mode == "default" else [{
                        "name": "WORKING_DIRECTORY",
                        "value": str(working if mode == "absolute" else working.relative_to(build)),
                    }]
                    command = build / "probe-command"
                    command.write_bytes(b"probe")
                    tests = [{"name": "probe", "command": [str(command), argument], "properties": properties}]
                    before = gate.artifact_hashes(build, tests, [])
                    self.assertIn(str(script.resolve()), before)
                    script.write_text("changed", encoding="utf-8")
                    self.assertNotEqual(before, gate.artifact_hashes(build, tests, []))

    def test_unsupported_platforms_are_refused_before_output_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "evidence"
            arguments = ["--baseline", directory, "0" * 40, "--candidate", directory, "0" * 40,
                         "--output", str(output), "--target", "fixture", "--tests", ".*", "--gpu", "fixture"]
            for module, attribute, value in ((sys, "platform", "cygwin"), (sys, "platform", "msys"), (os, "name", "java")):
                with self.subTest(attribute=attribute, value=value):
                    stderr = io.StringIO()
                    with unittest.mock.patch.object(module, attribute, value), contextlib.redirect_stderr(stderr), \
                            self.assertRaises(SystemExit) as raised:
                        gate.main(arguments)
                    self.assertEqual(raised.exception.code, 2)
                    self.assertIn("Cygwin and MSYS", stderr.getvalue())
                    self.assertFalse(output.exists())


@unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja") and shutil.which("ctest"), "requires CMake, Ninja and CTest")
class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()

    def source(self, name, message="pass", test="probe", property_text="", layer=True):
        if PYTHON is None:
            self.skipTest("needs a Python interpreter that is not an App Execution Alias")
        source = self.root / name
        source.mkdir()
        program = 'import os, sys\nfrom pathlib import Path\n'
        program += 'assert os.environ["ANYPS5_REQUIRE_VULKAN"] == "1"\n'
        program += 'assert os.environ["ANYPS5_NO_SHADER_CACHE"] == "1"\n'
        program += 'assert os.environ["VK_INSTANCE_LAYERS"] == "VK_LAYER_KHRONOS_validation"\n'
        program += 'assert os.environ["VK_LOADER_DEBUG"] == "layer"\n'
        program += 'assert os.environ["ANYPS5_GPU"] == "fixture"\n'
        if layer:
            program += 'print(\'[Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (fixture)\', file=sys.stderr)\n'
        if message == "skip":
            program += 'print("skipped, synthetic unavailable device")\nsys.exit(77)\n'
        elif message == "fail":
            program += 'sys.exit(7)\n'
        elif message == "mutate":
            program += 'with Path(__file__).open("a") as output: output.write("\\n")\n'
        elif message == "large":
            program += 'print("Validation Error: [ VUID-Early-00001 ] synthetic")\n'
            program += 'print("x" * 131072)\n'
            program += 'print("Validation Error: [ VUID-Late-00002 ] synthetic")\n'
        else:
            program += 'print(' + repr(message) + ')\n'
        (source / "probe.py").write_text(program, encoding="utf-8")
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.20)\nproject(gate_fixture NONE)\nenable_testing()\n'
            'add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/probe.py" COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/probe.py" "${CMAKE_BINARY_DIR}/probe.py" DEPENDS "${CMAKE_SOURCE_DIR}/probe.py")\n'
            'add_custom_target(fixture DEPENDS "${CMAKE_BINARY_DIR}/probe.py")\n'
            'add_test(NAME ' + test + ' COMMAND ' + PYTHON + ' "${CMAKE_BINARY_DIR}/probe.py")\n'
            'set_tests_properties(' + test + ' PROPERTIES SKIP_RETURN_CODE 77 ' + property_text + ')\n', encoding="utf-8")
        subprocess.run(["git", "init", "-q", str(source)], check=True)
        subprocess.run(["git", "-C", str(source), "add", "."], check=True)
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgSign=false", "commit", "-qm", "fixture"], check=True)
        return source, gate.git(source, "rev-parse", "HEAD")

    def execute(self, baseline, candidate, *extra):
        output = self.root / "evidence"
        arguments = ["--baseline", str(baseline[0]), baseline[1], "--candidate", str(candidate[0]), candidate[1],
                     "--output", str(output), "--target", "fixture", "--tests", ".*", "--gpu", "fixture", *extra]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            code = gate.main(arguments)
        report = json.loads((output / "report.json").read_text()) if (output / "report.json").is_file() else None
        return code, report


class TimeoutTests(FixtureTests):
    def test_timeout_reset_property_is_refused_before_running_tests(self):
        baseline = self.source("base-reset")
        candidate = self.source("head-reset", property_text='TIMEOUT_AFTER_MATCH "1;ready"')
        code, report = self.execute(baseline, candidate, "--repeat", "1", "--test-timeout", "1")
        self.assertEqual(code, 2, report)
        self.assertIn("TIMEOUT_AFTER_MATCH", report["error"])
        self.assertEqual(report["iterations"], [])

    def test_a_shorter_ctest_property_remains_usable(self):
        baseline = self.source("base-short", property_text="TIMEOUT 0.5")
        candidate = self.source("head-short", property_text="TIMEOUT 0.5")
        report_text = ('<testsuite><testcase name="probe"><system-err>' + gate.INSERTED_LAYER
                       + '</system-err></testcase></testsuite>')
        ctest_runs = []
        real_run = gate.run

        def run(command, log, environment, timeout):
            if "--output-junit" not in command:
                return real_run(command, log, environment, timeout)
            ctest_runs.append(command)
            Path(command[command.index("--output-junit") + 1]).write_text(report_text, encoding="utf-8")
            return 0

        with unittest.mock.patch.object(gate, "run", run):
            code, report = self.execute(baseline, candidate, "--repeat", "1", "--test-timeout", "1")
        self.assertEqual(code, 0, report)
        self.assertEqual(report["status"], "complete")
        self.assertEqual(len(ctest_runs), 2)
        for command in ctest_runs:
            self.assertEqual(command[command.index("--timeout") + 1], "1.0")

    def test_request_above_thirty_seconds_is_refused_before_output_creation(self):
        with self.assertRaises(SystemExit) as raised:
            self.execute(self.source("base-limit"), self.source("head-limit"), "--test-timeout", "31")
        self.assertEqual(raised.exception.code, 2)
        self.assertFalse((self.root / "evidence").exists())

    def test_ctest_timeout_property_is_checked_before_running_tests(self):
        for value in ("0", "2"):
            with self.subTest(value=value):
                baseline = self.source("base-" + value)
                candidate = self.source("head-" + value, property_text="TIMEOUT " + value)
                code, report = self.execute(baseline, candidate, "--repeat", "1", "--test-timeout", ".05")
                self.assertEqual(code, 2, report)
                self.assertIn("probe: TIMEOUT", report["error"])
                self.assertEqual(report["iterations"], [])
                self.assertEqual(report["status"], "incomplete")
                (self.root / "evidence").rename(self.root / ("evidence-" + value))


class IntegrationTests(FixtureTests):
    def test_pass_builds_both_targets_and_alternates_with_attribution(self):
        code, report = self.execute(self.source("base"), self.source("head"))
        self.assertEqual(code, 0, report)
        self.assertEqual([entry["side"] for entry in report["iterations"]], ["baseline", "candidate", "candidate", "baseline"])
        self.assertEqual(report["status"], "complete")
        for side in ("baseline", "candidate"):
            files = report["builds"][side]["artifact_sha256"]
            self.assertTrue(any(path.endswith("probe.py") for path in files))
            self.assertTrue((self.root / "evidence" / side / "iteration-1.xml").is_file())


class ValidationTests(FixtureTests):
    def test_ctest_success_with_validation_errors_is_a_gate_failure(self):
        code, report = self.execute(self.source("base"), self.source("head", "Validation Error: [ VUID-Fixture-00001 ] synthetic"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(all(entry["returncode"] == 0 for entry in report["iterations"]))
        self.assertEqual(report["new_diagnostics"], {"probe": ["VUID-Fixture-00001"]})


class LayerTests(FixtureTests):
    def test_ctest_success_with_missing_layer_is_a_gate_failure(self):
        message = "WARNING: [Loader Message] layer was not found but was requested by env var VK_INSTANCE_LAYERS"
        code, report = self.execute(self.source("base", message), self.source("head"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(all(entry["returncode"] == 0 for entry in report["iterations"]))
        self.assertTrue(report["problems"])

    def test_missing_layer_without_severity_is_a_gate_failure(self):
        message = "Layer VK_LAYER_KHRONOS_validation was not found but was requested by env var VK_INSTANCE_LAYERS"
        code, report = self.execute(self.source("base"), self.source("head", message), "--repeat", "1")
        self.assertEqual(code, 1, report)

    def test_ctest_success_without_layer_insertion_is_a_gate_failure(self):
        code, report = self.execute(self.source("base"), self.source("head", layer=False), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(all(entry["returncode"] == 0 for entry in report["iterations"]))
        self.assertEqual({entry["side"]: entry["tests"]["probe"]["layer_inserted"] for entry in report["iterations"]},
                         {"baseline": True, "candidate": False})


class OutputTests(FixtureTests):
    def test_large_passed_output_retains_early_and_late_validation_messages(self):
        code, report = self.execute(self.source("base"), self.source("head", "large"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertEqual(report["new_diagnostics"], {"probe": ["VUID-Early-00001", "VUID-Late-00002"]})

    def test_configuration_like_text_does_not_create_a_validation_error(self):
        text = "VUID-Fixture-00001 was not found but was requested by env var VK_INSTANCE_LAYERS"
        code, report = self.execute(self.source("base"), self.source("head", property_text='ENVIRONMENT "GATE_CONFIGURATION_ECHO=' + text + '"'), "--repeat", "1")
        self.assertEqual(code, 0, report)
        self.assertIn(text, (self.root / "evidence/candidate/iteration-1.log").read_text())

    def test_a_skipped_selected_test_is_not_accepted(self):
        code, report = self.execute(self.source("base"), self.source("head", "skip"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(report["problems"])


class AttributionTests(FixtureTests):
    def test_relative_script_mutation_invalidates_successful_ctest_results(self):
        baseline, candidate = self.source("base"), self.source("head", "mutate")
        cmake = candidate[0] / "CMakeLists.txt"
        text = cmake.read_text(encoding="utf-8")
        absolute = 'add_test(NAME probe COMMAND ' + PYTHON + ' "${CMAKE_BINARY_DIR}/probe.py")'
        self.assertIn(absolute, text)
        cmake.write_text(text.replace(absolute, 'add_test(NAME probe COMMAND ' + PYTHON + ' probe.py)'), encoding="utf-8")
        subprocess.run(["git", "-C", str(candidate[0]), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgSign=false", "commit", "-qam", "relative fixture"], check=True)
        candidate = candidate[0], gate.git(candidate[0], "rev-parse", "HEAD")
        code, report = self.execute(baseline, candidate, "--repeat", "1")
        self.assertTrue(all(iteration["returncode"] == 0 for iteration in report["iterations"]), report)
        self.assertEqual(report["builds"]["candidate"]["tests"][0]["command"][-1], "probe.py")
        self.assertNotEqual((self.root / "evidence/candidate/build/probe.py").read_bytes(), (candidate[0] / "probe.py").read_bytes())
        self.assertEqual(code, 2, report)
        self.assertIn("artifacts changed", report["error"])

    def test_different_selected_names_stop_before_iterations(self):
        code, report = self.execute(self.source("base"), self.source("head", test="other"))
        self.assertEqual(code, 2, report)
        self.assertEqual(report["iterations"], [])
        self.assertIn("test-name sets differ", report["error"])

    def test_changed_artifact_invalidates_completed_test_results(self):
        code, report = self.execute(self.source("base"), self.source("head", "mutate"), "--repeat", "1")
        self.assertEqual(code, 2, report)
        self.assertIn("artifacts changed", report["error"])


class SetupTests(FixtureTests):
    def test_test_properties_cannot_disable_required_validation(self):
        code, report = self.execute(self.source("base"), self.source("head", property_text='ENVIRONMENT "ANYPS5_REQUIRE_VULKAN=0"'))
        self.assertEqual(code, 2, report)
        self.assertIn("overrides required environment", report["error"])

    def test_test_properties_cannot_override_driver_selection(self):
        code, report = self.execute(self.source("base"), self.source("head", property_text='ENVIRONMENT "VK_DRIVER_FILES=/different/driver.json"'))
        self.assertEqual(code, 2, report)
        self.assertIn("overrides required environment", report["error"])

    def test_dirty_source_is_rejected_before_output_creation(self):
        baseline, candidate = self.source("base"), self.source("head")
        (candidate[0] / "untracked.txt").write_text("dirty")
        code, report = self.execute(baseline, candidate)
        self.assertEqual(code, 2)
        self.assertIsNone(report)

    def test_wrong_expected_commit_is_rejected_before_output_creation(self):
        baseline, candidate = self.source("base"), self.source("head")
        code, report = self.execute(baseline, (candidate[0], "0" * 40))
        self.assertEqual(code, 2)
        self.assertIsNone(report)

    def test_existing_output_is_preserved(self):
        output = self.root / "evidence"
        output.mkdir()
        marker = output / "keep"
        marker.write_text("existing data")
        code, report = self.execute(self.source("base"), self.source("head"))
        self.assertEqual(code, 2)
        self.assertIsNone(report)
        self.assertEqual(marker.read_text(), "existing data")

    def test_empty_selection_is_a_setup_error(self):
        code, report = self.execute(self.source("base"), self.source("head"), "--tests", "^missing$")
        self.assertEqual(code, 2, report)
        self.assertIn("nonempty unique test names", report["error"])


class CommandTests(FixtureTests):
    def test_unexpected_command_failure_is_retained(self):
        code, report = self.execute(self.source("base"), self.source("head", "fail"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(any(entry["returncode"] != 0 for entry in report["iterations"]))

    def test_command_cleanup_removes_descendant_after_parent_exit_and_timeout(self):
        for timeout in (True, False):
            with self.subTest(timeout=timeout):
                marker = self.root / ("timeout-marker" if timeout else "exit-marker")
                ready = marker.with_suffix(".ready")
                child = f"import time; from pathlib import Path; Path({str(ready)!r}).touch(); time.sleep(1); Path({str(marker)!r}).touch()"
                parent = f"import subprocess,sys,time; from pathlib import Path; subprocess.Popen([sys.executable,'-c',{child!r}])\nwhile not Path({str(ready)!r}).exists(): time.sleep(.01)\n"
                if timeout:
                    parent += "time.sleep(30)\n"
                    with self.assertRaises(subprocess.TimeoutExpired):
                        gate.run([sys.executable, "-c", parent], self.root / "timeout.log", os.environ, .5)
                else:
                    self.assertEqual(gate.run([sys.executable, "-c", parent], self.root / "exit.log", os.environ, 5), 0)
                self.assertTrue(ready.exists())
                time.sleep(1.1)
                self.assertFalse(marker.exists())

    def test_symlink_loops_are_setup_errors(self):
        try:
            (self.root / "first").symlink_to("second")
            (self.root / "second").symlink_to("first")
        except OSError as error:
            self.skipTest(f"symlinks unavailable: {error}")
        loop, source = self.root / "first", self.source("base")
        for baseline, output in ((source, loop / "evidence"), ((loop, source[1]), self.root / "evidence")):
            with self.subTest(baseline=baseline[0], output=output):
                arguments = ["--baseline", str(baseline[0]), baseline[1], "--candidate", str(source[0]), source[1],
                             "--output", str(output), "--target", "fixture", "--tests", ".*", "--gpu", "fixture"]
                stderr = io.StringIO()
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(stderr):
                    self.assertEqual(gate.main(arguments), 2)
                self.assertIn("validation gate:", stderr.getvalue())
                self.assertFalse((self.root / "evidence").exists())


@unittest.skipUnless(os.name == "nt", "Windows job containment")
class ContainmentTests(unittest.TestCase):
    def setUp(self):
        import ctypes
        from ctypes import wintypes
        self.ctypes = ctypes
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel32.OpenProcess.restype = wintypes.HANDLE
        self.kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        self.kernel32.WaitForSingleObject.restype = wintypes.DWORD
        self.kernel32.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
        self.kernel32.TerminateProcess.restype = wintypes.BOOL
        self.kernel32.TerminateProcess.argtypes = (wintypes.HANDLE, wintypes.UINT)
        self.kernel32.CloseHandle.restype = wintypes.BOOL
        self.kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)

    def kill_pid(self, pid):
        handle = self.kernel32.OpenProcess(0x00100001, False, pid)
        if handle:
            self.kernel32.TerminateProcess(handle, 1)
            self.kernel32.WaitForSingleObject(handle, 2000)
            self.kernel32.CloseHandle(handle)

    def is_dead(self, pid, timeout):
        handle = self.kernel32.OpenProcess(0x00100000, False, pid)
        if not handle:
            return True
        try:
            return self.kernel32.WaitForSingleObject(handle, int(timeout * 1000)) == 0
        finally:
            self.kernel32.CloseHandle(handle)

    def wait_for_file(self, path, timeout=10.0):
        deadline = time.monotonic() + timeout
        while not path.exists():
            if time.monotonic() >= deadline:
                self.fail(f"timed out waiting for {path}")
            time.sleep(0.01)

    def repo(self, name):
        source = self.root / name
        source.mkdir()
        (source / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.20)\nproject(p NONE)\n", encoding="utf-8")
        subprocess.run(["git", "init", "-q", str(source)], check=True)
        subprocess.run(["git", "-C", str(source), "add", "."], check=True)
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgSign=false", "commit", "-qm", "fixture"], check=True)
        return source, gate.git(source, "rev-parse", "HEAD")

    def test_job_assignment_failure_kills_suspended_child_and_propagates(self):
        if hasattr(gate, "guard"):
            gate.guard()
        marker = self.root / "assign-marker"
        spawned = []
        real_popen = subprocess.Popen
        real_win32 = gate.win32
        current = gate.KERNEL32.GetCurrentProcess() if hasattr(gate.KERNEL32, "GetCurrentProcess") else None

        def popen(*args, **kwargs):
            process = real_popen(*args, **kwargs)
            if kwargs.get("creationflags") == gate.CREATE_SUSPENDED:
                spawned.append(process.pid)
                self.addCleanup(self.kill_pid, process.pid)
            return process

        def win32(name, *arguments, **kwargs):
            if name == "AssignProcessToJobObject" and arguments[1] != current:
                raise OSError("synthetic assign failure")
            return real_win32(name, *arguments, **kwargs)

        with unittest.mock.patch.object(subprocess, "Popen", popen), unittest.mock.patch.object(gate, "win32", win32):
            with self.assertRaisesRegex(OSError, "synthetic assign failure"):
                gate.run([sys.executable, "-c", f"from pathlib import Path; Path({str(marker)!r}).touch()"],
                         self.root / "assign.log", os.environ, 5)
            self.assertEqual(len(spawned), 1)
            self.assertTrue(self.is_dead(spawned[0], 2.0))
            self.assertFalse(marker.exists())
            if shutil.which("git"):
                baseline, candidate = self.repo("assign-base"), self.repo("assign-head")
                output = self.root / "assign-evidence"
                arguments = ["--baseline", str(baseline[0]), baseline[1], "--candidate", str(candidate[0]), candidate[1],
                             "--output", str(output), "--target", "fixture", "--tests", ".*", "--gpu", "fixture",
                             "--cmake", sys.executable]
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(gate.main(arguments), 2)
                report = json.loads((output / "report.json").read_text(encoding="utf-8"))
                self.assertIn("synthetic assign failure", report["error"])
                self.assertEqual(len(spawned), 2)
                self.assertTrue(self.is_dead(spawned[1], 2.0))

    def test_keyboard_interrupt_during_sliced_wait_cleans_up_tree(self):
        ready = self.root / "interrupt.ready"
        marker = self.root / "interrupt.marker"
        child = f"import time; from pathlib import Path; Path({str(ready)!r}).touch(); time.sleep(1); Path({str(marker)!r}).touch()"
        parent = (f"import subprocess, sys, time; from pathlib import Path; subprocess.Popen([sys.executable, '-c', {child!r}])\n"
                  f"while not Path({str(ready)!r}).exists(): time.sleep(.01)\ntime.sleep(30)\n")
        calls = 0
        real_wait = subprocess.Popen.wait

        def sliced_wait(process, timeout=None):
            nonlocal calls
            calls += 1
            if calls == 1:
                self.wait_for_file(ready)
                return real_wait(process, timeout=0.05)
            if calls == 2:
                raise KeyboardInterrupt
            return real_wait(process, timeout=timeout)

        with unittest.mock.patch.object(subprocess.Popen, "wait", sliced_wait):
            with self.assertRaises(KeyboardInterrupt):
                gate.run([sys.executable, "-c", parent], self.root / "interrupt.log", os.environ, 30)
        self.assertTrue(ready.exists())
        time.sleep(1.2)
        self.assertFalse(marker.exists())

    def test_surviving_job_processes_fail_normal_exit_and_preserve_timeout(self):
        real_popen = subprocess.Popen
        real_win32 = gate.win32
        real_close = getattr(gate, "close", None)
        ignore_terminate = False
        jobs = set()
        processes = []

        def cleanup_process(process):
            self.kill_pid(process.pid)
            with contextlib.suppress(OSError, subprocess.TimeoutExpired):
                process.wait(timeout=2)

        def popen(*args, **kwargs):
            process = real_popen(*args, **kwargs)
            processes.append(process)
            self.addCleanup(cleanup_process, process)
            return process

        def close(handle):
            if ignore_terminate and handle in jobs:
                return
            real_close(handle)

        def win32(name, *arguments, **kwargs):
            if name == "TerminateJobObject" and ignore_terminate:
                return 1
            if name == "CloseHandle" and ignore_terminate and arguments[0] in jobs:
                return 1
            if name == "QueryInformationJobObject":
                if hasattr(gate, "Accounting"):
                    self.ctypes.cast(arguments[2], self.ctypes.POINTER(gate.Accounting)).contents.active = 1
                return 1
            result = real_win32(name, *arguments, **kwargs)
            if name == "CreateJobObjectW":
                jobs.add(result)
                self.addCleanup(self.kernel32.CloseHandle, result)
            return result

        with contextlib.ExitStack() as stack:
            stack.enter_context(unittest.mock.patch.object(gate, "DRAIN_SECONDS", 0.2, create=True))
            stack.enter_context(unittest.mock.patch.object(subprocess, "Popen", popen))
            stack.enter_context(unittest.mock.patch.object(gate, "win32", win32))
            if real_close is not None:
                stack.enter_context(unittest.mock.patch.object(gate, "close", close))
            with self.assertRaisesRegex(gate.GateError, "survived job termination"):
                gate.run([sys.executable, "-c", "pass"], self.root / "survive.log", os.environ, 5)
            ignore_terminate = True
            started = time.monotonic()
            with self.assertRaises(subprocess.TimeoutExpired):
                gate.run([sys.executable, "-c", "import time; time.sleep(5)"], self.root / "survive-timeout.log", os.environ, 0.2)
            self.assertLess(time.monotonic() - started, 2.0)

    def test_killing_gate_process_terminates_active_command_tree(self):
        ready = self.root / "killed-gate.ready"
        marker = self.root / "killed-gate.marker"
        pids = self.root / "killed-gate.pids"
        tools_dir = str(Path(__file__).resolve().parents[1])
        child = f"import time; from pathlib import Path; Path({str(ready)!r}).touch(); time.sleep(1); Path({str(marker)!r}).touch()"
        parent = (f"import os, subprocess, sys, time; from pathlib import Path\n"
                  f"descendant = subprocess.Popen([sys.executable, '-c', {child!r}])\n"
                  f"Path({str(pids)!r}).write_text(f'{{os.getpid()}} {{descendant.pid}}', encoding='utf-8')\n"
                  f"while not Path({str(ready)!r}).exists(): time.sleep(.01)\ntime.sleep(30)\n")
        runner_code = (f"import os, sys; from pathlib import Path; sys.path.insert(0, {tools_dir!r}); "
                       f"import vulkan_validation_gate as gate; "
                       f"gate.run([sys.executable, '-c', {parent!r}], Path({str(self.root / 'killed-gate.log')!r}), os.environ, 30)")
        runner = subprocess.Popen([sys.executable, "-c", runner_code])
        self.addCleanup(self.kill_pid, runner.pid)
        self.wait_for_file(ready)
        self.wait_for_file(pids)
        for pid in map(int, pids.read_text(encoding="utf-8").split()):
            self.addCleanup(self.kill_pid, pid)
        self.kernel32.TerminateProcess(int(runner._handle), 1)
        runner.wait(timeout=5)
        time.sleep(2.0)
        self.assertFalse(marker.exists())

    def test_guard_job_kills_suspended_child_when_gate_dies_before_containment(self):
        tools_dir = str(Path(__file__).resolve().parents[1])
        script = self.root / "guard_probe.py"
        script.write_text(
            "import os, sys\nfrom pathlib import Path\n"
            f"sys.path.insert(0, {tools_dir!r})\nimport vulkan_validation_gate as gate\n"
            "if sys.argv[1] == '0':\n    gate.guard = lambda: None\n"
            "def stop_before_job(pid):\n"
            "    Path(sys.argv[2]).write_text(str(pid), encoding='utf-8')\n"
            "    os._exit(1)\n"
            "gate.contain = stop_before_job\n"
            "gate.run([sys.executable, '-c', 'import time; time.sleep(60)'], Path(sys.argv[3]), os.environ, 30)\n",
            encoding="utf-8")
        for guarded in ("1", "0"):
            with self.subTest(guarded=guarded):
                pid_file = self.root / f"guard-{guarded}.pid"
                result = subprocess.run([sys.executable, str(script), guarded, str(pid_file), str(self.root / f"guard-{guarded}.log")])
                self.assertEqual(result.returncode, 1)
                pid = int(pid_file.read_text(encoding="utf-8"))
                self.addCleanup(self.kill_pid, pid)
                if guarded == "1":
                    self.assertTrue(self.is_dead(pid, 2.0))
                else:
                    self.assertFalse(self.is_dead(pid, 0.3))
                    self.kill_pid(pid)
                    self.assertTrue(self.is_dead(pid, 2.0))

    def test_unexpected_resume_suspend_count_fails_and_kills_child(self):
        spawned = []
        real_popen = subprocess.Popen
        real_win32 = gate.win32

        def popen(*args, **kwargs):
            process = real_popen(*args, **kwargs)
            if kwargs.get("creationflags") == gate.CREATE_SUSPENDED:
                spawned.append(process.pid)
                self.addCleanup(self.kill_pid, process.pid)
            return process

        def win32(name, *arguments, **kwargs):
            if name == "ResumeThread":
                return 2
            return real_win32(name, *arguments, **kwargs)

        with unittest.mock.patch.object(subprocess, "Popen", popen), unittest.mock.patch.object(gate, "win32", win32):
            with self.assertRaisesRegex(gate.GateError, r"\b2\b"):
                gate.run([sys.executable, "-c", "pass"], self.root / "resume.log", os.environ, 0.5)
        self.assertEqual(len(spawned), 1)
        self.assertTrue(self.is_dead(spawned[0], 2.0))

    def test_job_information_structures_match_win32_layout(self):
        pointer = self.ctypes.sizeof(self.ctypes.c_void_p)
        self.assertEqual(self.ctypes.sizeof(gate.Limits), 144 if pointer == 8 else 112)
        self.assertEqual(gate.Limits.flags.offset, 16)
        self.assertEqual(gate.Limits.io.offset, 64 if pointer == 8 else 48)
        self.assertEqual(gate.Limits.memory.offset, 112 if pointer == 8 else 96)
        self.assertEqual(self.ctypes.sizeof(gate.Accounting), 48)
        self.assertEqual(gate.Accounting.active.offset, 40)


@unittest.skipUnless(os.name == "nt", "Windows job containment")
class NestedContainmentTests(FixtureTests):
    def probe_source(self, name, body):
        source, _ = self.source(name)
        program = ('import os, subprocess, sys, time\nfrom pathlib import Path\n'
                   'print(\'[Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (fixture)\', file=sys.stderr)\n'
                   + body)
        (source / "probe.py").write_text(program, encoding="utf-8")
        subprocess.run(["git", "-C", str(source), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgSign=false", "commit", "-qam", "custom probe"], check=True)
        return source, gate.git(source, "rev-parse", "HEAD")

    def test_ctest_descendant_with_devnull_stdio_is_terminated_after_test_exit(self):
        ready = self.root / "nested.ready"
        marker = self.root / "nested.marker"
        child = f"import time; from pathlib import Path; Path({str(ready)!r}).touch(); time.sleep(1); Path({str(marker)!r}).touch()"
        body = (f"subprocess.Popen([sys.executable, '-c', {child!r}], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)\n"
                f"while not Path({str(ready)!r}).exists(): time.sleep(.01)\n")
        code, report = self.execute(self.source("nested-base"), self.probe_source("nested-head", body), "--repeat", "1")
        self.assertEqual(code, 0, report)
        self.assertTrue(ready.exists())
        time.sleep(1.2)
        self.assertFalse(marker.exists())

    def test_breakaway_is_refused_directly_and_contained_under_ctest(self):
        direct_marker = self.root / "breakaway-direct.marker"
        direct_status = self.root / "breakaway-direct.status"
        direct_child = f"from pathlib import Path; Path({str(direct_marker)!r}).touch()"
        direct_parent = (
            f"import subprocess, sys; from pathlib import Path\n"
            f"try:\n"
            f"    subprocess.Popen([sys.executable, '-c', {direct_child!r}], stdin=subprocess.DEVNULL, "
            f"stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=0x01000000)\n"
            f"    Path({str(direct_status)!r}).write_text('spawned', encoding='utf-8')\n"
            f"except OSError as error:\n"
            f"    Path({str(direct_status)!r}).write_text(str(error.winerror), encoding='utf-8')\n")
        self.assertEqual(gate.run([sys.executable, "-c", direct_parent], self.root / "breakaway-direct.log", os.environ, 10), 0)
        self.assertEqual(direct_status.read_text(encoding="utf-8"), "5")
        self.assertFalse(direct_marker.exists())

        ready = self.root / "breakaway-ctest.ready"
        ctest_marker = self.root / "breakaway-ctest.marker"
        ctest_child = f"import time; from pathlib import Path; Path({str(ready)!r}).touch(); time.sleep(1); Path({str(ctest_marker)!r}).touch()"
        body = (f"subprocess.Popen([sys.executable, '-c', {ctest_child!r}], stdin=subprocess.DEVNULL, "
                f"stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=0x01000000)\n"
                f"while not Path({str(ready)!r}).exists(): time.sleep(.01)\n")
        code, report = self.execute(self.source("breakaway-base"), self.probe_source("breakaway-head", body), "--repeat", "1")
        self.assertEqual(code, 0, report)
        self.assertTrue(ready.exists())
        time.sleep(1.2)
        self.assertFalse(ctest_marker.exists())


if __name__ == "__main__":
    unittest.main()
