import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import vulkan_validation_gate as gate
import vulkan_validation_matrix as matrix


AMD = "[Execution/src/VulkanDevice.cpp:1678 VulkanDevice] Selected GPU name=Radeon fixture vendor=0x1002 device=0x744c subgroup=32"
INTEL = "Selected GPU name=Intel fixture vendor=32902 device=22176"
VUID = "Validation Error: [ VUID-Fixture-00001 ] synthetic"


class MatrixTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def fixture(self, name, selector="fixture", baseline=AMD, candidate=AMD,
                commits=("a" * 40, "b" * 40), pattern=".*", tests=("probe",), targets=("fixture",)):
        output = self.directory / name
        iterations = []
        for side, contents in (("baseline", baseline), ("candidate", candidate)):
            junit = output / side / "iteration-1.xml"
            junit.parent.mkdir(parents=True)
            suite = ET.Element("testsuite")
            for test in tests:
                case = ET.SubElement(suite, "testcase", name=test)
                ET.SubElement(case, "system-out").text = contents
                ET.SubElement(case, "system-err").text = gate.INSERTED_LAYER
            ET.ElementTree(suite).write(junit, encoding="utf-8")
            results, problems = gate.parse_report(junit, tests)
            iterations.append({"side": side, "iteration": 1, "returncode": 0,
                               "tests": results, "problems": problems})
        report = {"status": "complete", "iterations": iterations, "environment": {"ANYPS5_GPU": selector},
                  "builds": {side: {"commit": commit, "tests": [{"name": test} for test in tests]}
                             for side, commit in zip(matrix.SIDES, commits)},
                  "options": {"tests": pattern, "targets": list(targets), "repeat": 1,
                              "cmake_args": [], "runtime": []},
                  **gate.compare(iterations)}
        gate.save(output, report)
        return output / "report.json"

    def test_cli_groups_comparable_gpu_runs_and_reports_actual_identities(self):
        amd = self.fixture("amd", selector="RADEON", targets=("fixture", "libs"))
        intel = self.fixture("intel", selector="Intel", baseline=INTEL, candidate=INTEL, targets=("libs", "fixture"))
        command = [sys.executable, matrix.__file__, "--expect-gpu", "RADEON", "--expect-gpu", "Intel", str(amd), str(intel)]
        result = subprocess.run(command, capture_output=True, encoding="utf-8", check=True)
        report = json.loads(result.stdout)
        self.assertEqual(report["exit_code"], 0)
        self.assertFalse(report["incomplete"])
        group, = report["groups"]
        self.assertEqual(group["missing_gpu_selectors"], [])
        self.assertEqual(group["comparison"]["test_names"], ["probe"])
        self.assertEqual([run["requested_gpu"] for run in group["runs"]], ["RADEON", "Intel"])
        self.assertEqual([run["observed_gpus"] for run in group["runs"]], [
            [{"name": "Radeon fixture", "vendor_id": 0x1002, "device_id": 0x744C}],
            [{"name": "Intel fixture", "vendor_id": 32902, "device_id": 22176}],
        ])

    def test_commit_order_test_selection_and_targets_separate_groups(self):
        paths = [
            self.fixture("baseline"),
            self.fixture("reversed", commits=("b" * 40, "a" * 40)),
            self.fixture("other-regex", pattern="^probe$"),
            self.fixture("other-matched-tests", tests=("different",)),
            self.fixture("other-targets", targets=("different",)),
        ]
        report = matrix.aggregate(paths, [])
        self.assertEqual(len(report["groups"]), len(paths))
        self.assertTrue(all(len(group["runs"]) == 1 for group in report["groups"]))

    def test_missing_device_line_and_evidence_file_stay_incomplete(self):
        path = self.fixture("missing-device", candidate="Physical device candidate=0x1 name=Radeon fixture")
        (path.parent / "baseline" / "iteration-1.xml").unlink()
        report = matrix.aggregate([path], ["fixture", "unsubmitted"])
        self.assertEqual(report["exit_code"], 2)
        group, = report["groups"]
        self.assertEqual(group["missing_gpu_selectors"], ["fixture", "unsubmitted"])
        run, = group["runs"]
        self.assertEqual(run["gate_exit_code"], 0)
        self.assertEqual(run["observed_gpus"], [])
        self.assertIn("error", run["gpu_samples"][0])
        self.assertEqual(run["gpu_samples"][1]["missing_tests"], ["probe"])

    def test_different_devices_with_same_selector_are_inconsistent(self):
        path = self.fixture("mixed", candidate=INTEL)
        report = matrix.aggregate([path], [])
        self.assertEqual(report["exit_code"], 2)
        run = report["groups"][0]["runs"][0]
        self.assertEqual(len(run["observed_gpus"]), 2)
        self.assertIn("the run records more than one observed GPU identity", run["gpu_issues"])

    def test_selector_mismatch_and_gate_failure_are_reported_independently(self):
        bad = self.fixture("regression", selector="Intel", candidate=AMD + "\n" + VUID)
        clean = self.fixture("retry", selector="Radeon")
        report = matrix.aggregate([bad, clean], ["Intel", "Radeon"])
        self.assertEqual(report["exit_code"], 1)
        self.assertTrue(report["gate_failed"])
        self.assertTrue(report["incomplete"])
        group, = report["groups"]
        self.assertEqual(group["missing_gpu_selectors"], ["Intel"])
        failure, retry = group["runs"]
        self.assertEqual(failure["new_diagnostics"], {"probe": ["VUID-Fixture-00001"]})
        self.assertIn("an observed GPU name does not match the requested selector", failure["gpu_issues"])
        self.assertEqual(retry["gate_exit_code"], 0)

    def test_interrupted_unreadable_and_truncated_inputs_remain_visible(self):
        path = self.fixture("truncated")
        report = json.loads(path.read_text(encoding="utf-8"))
        report["iterations"].pop()
        gate.save(path.parent, report)
        interrupted = self.directory / "interrupted"
        interrupted.mkdir()
        gate.save(interrupted, {"status": "incomplete", "exit_code": 130, "environment": {"ANYPS5_GPU": "fixture"},
                                "builds": {}, "error": "interrupted"})
        result = matrix.aggregate([path, interrupted / "report.json", self.directory / "missing.json"], [])
        self.assertEqual(result["exit_code"], 2)
        run = result["groups"][0]["runs"][0]
        self.assertIn("candidate: expected 1 retained iterations, found 0", run["gpu_issues"])
        cancelled, missing = result["ungrouped"]
        self.assertEqual(cancelled["gate_exit_code"], 130)
        self.assertEqual(cancelled["requested_gpu"], "fixture")
        self.assertIsNone(missing["gate_status"])
        self.assertIn("missing.json", missing["input_error"])


if __name__ == "__main__":
    unittest.main()
