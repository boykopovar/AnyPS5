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
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import vulkan_validation_gate as gate


class ReportTests(unittest.TestCase):
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

    def test_windows_is_refused_before_output_is_created(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "not-created"
            arguments = ["--baseline", directory, "a" * 40, "--candidate", directory, "b" * 40,
                         "--output", str(output), "--target", "fixture", "--tests", "probe", "--gpu", "fixture"]
            with mock.patch.object(gate.os, "name", "nt"), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    gate.main(arguments)
            self.assertEqual(error.exception.code, 2)
            self.assertFalse(output.exists())

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
                    tests = [{"name": "probe", "command": [sys.executable, argument], "properties": properties}]
                    before = gate.artifact_hashes(build, tests, [])
                    self.assertIn(str(script.resolve()), before)
                    script.write_text("changed", encoding="utf-8")
                    self.assertNotEqual(before, gate.artifact_hashes(build, tests, []))


@unittest.skipUnless(os.name == "posix" and shutil.which("cmake") and shutil.which("ninja") and shutil.which("ctest"), "requires POSIX, CMake, Ninja and CTest")
class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()

    def source(self, name, message="pass", test="probe", property_text=""):
        source = self.root / name
        source.mkdir()
        program = 'import os, sys\nfrom pathlib import Path\n'
        program += 'assert os.environ["ANYPS5_REQUIRE_VULKAN"] == "1"\n'
        program += 'assert os.environ["ANYPS5_NO_SHADER_CACHE"] == "1"\n'
        program += 'assert os.environ["VK_INSTANCE_LAYERS"] == "VK_LAYER_KHRONOS_validation"\n'
        program += 'assert os.environ["ANYPS5_GPU"] == "fixture"\n'
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
            'add_test(NAME ' + test + ' COMMAND "' + sys.executable + '" "${CMAKE_BINARY_DIR}/probe.py")\n'
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

    def test_ctest_success_with_validation_errors_is_a_gate_failure(self):
        code, report = self.execute(self.source("base"), self.source("head", "Validation Error: [ VUID-Fixture-00001 ] synthetic"), "--repeat", "1")
        self.assertEqual(code, 1, report)
        self.assertTrue(all(entry["returncode"] == 0 for entry in report["iterations"]))
        self.assertEqual(report["new_diagnostics"], {"probe": ["VUID-Fixture-00001"]})

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
        absolute = 'add_test(NAME probe COMMAND "' + sys.executable + '" "${CMAKE_BINARY_DIR}/probe.py")'
        self.assertIn(absolute, text)
        cmake.write_text(text.replace(absolute, 'add_test(NAME probe COMMAND "' + sys.executable + '" probe.py)'), encoding="utf-8")
        subprocess.run(["git", "-C", str(candidate[0]), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                        "-c", "commit.gpgSign=false", "commit", "-qam", "relative fixture"], check=True)
        candidate = candidate[0], gate.git(candidate[0], "rev-parse", "HEAD")
        code, report = self.execute(baseline, candidate, "--repeat", "1")
        self.assertTrue(all(iteration["returncode"] == 0 for iteration in report["iterations"]), report)
        self.assertEqual(report["builds"]["candidate"]["tests"][0]["command"][1], "probe.py")
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


if __name__ == "__main__":
    unittest.main()
