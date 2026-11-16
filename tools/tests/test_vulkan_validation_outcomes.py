import contextlib
import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import vulkan_validation_gate as gate
import vulkan_validation_outcomes as outcomes


INSERTED = gate.INSERTED_LAYER


def testcase(name="probe", status=None, kind=None, message=None, detail="", stdout=INSERTED, stderr=""):
    attributes = {"name": name}
    if status is not None:
        attributes["status"] = status
    case = ET.Element("testcase", attributes)
    if kind is not None:
        attributes = {} if message is None else {"message": message}
        ET.SubElement(case, kind, attributes).text = detail
    ET.SubElement(case, "system-out").text = stdout
    ET.SubElement(case, "system-err").text = stderr
    return case


class Fixtures(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.sequence = 0

    def junit(self, cases, name=None):
        self.sequence += 1
        path = self.directory / (name or f"junit-{self.sequence}.xml")
        root = ET.Element("testsuite")
        root.extend(cases)
        ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)
        return path

    def expectations(self, tests=None, text=None):
        path = self.directory / "expectations.json"
        value = {"version": 1, "tests": tests or {}}
        path.write_text(json.dumps(value) if text is None else text, encoding="utf-8")
        return path

    def iteration(self, side, number, case, declarations=None, code=0):
        path = self.junit([case])
        tests, problems = gate.parse_report(path, {"probe"})
        claims = {name: claim[side] for name, claim in (declarations or {}).items()}
        return {"side": side, "iteration": number, "returncode": code, "tests": tests,
                "problems": problems, "outcomes": outcomes.read_outcomes(path, claims)}


class ExpectationTests(Fixtures):
    def test_valid_claims_retain_input_identity_and_literal_markers(self):
        tests = {"probe": {"baseline": {"state": "failed", "marker": "word [0] < expected"},
                           "candidate": {"state": "passed"}}}
        path = self.expectations(tests)
        result = outcomes.load_expectations(path)
        self.assertEqual(result["tests"], tests)
        self.assertEqual(result["path"], str(path.resolve()))
        self.assertEqual(result["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(result["version"], 1)

    def test_schema_versions_root_types_and_unknown_keys_are_rejected(self):
        values = (None, [], "claims", {}, {"version": 1}, {"version": 1, "tests": []},
                  {"version": 2, "tests": {}}, {"version": True, "tests": {}},
                  {"version": 1.0, "tests": {}}, {"version": 1, "tests": {}, "waive": True})
        for value in values:
            with self.subTest(value=value), self.assertRaises(ValueError):
                outcomes.load_expectations(self.expectations(text=json.dumps(value)))

    def test_duplicate_keys_are_rejected_at_every_schema_level(self):
        texts = ('{"version":1,"version":1,"tests":{}}',
                 '{"version":1,"tests":{"probe":{},"probe":{}}}',
                 '{"version":1,"tests":{"probe":{"baseline":{},"baseline":{}}}}',
                 '{"version":1,"tests":{"probe":{"baseline":{"state":"passed","state":"failed"},"candidate":{"state":"passed"}}}}')
        for text in texts:
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, "duplicate"):
                outcomes.load_expectations(self.expectations(text=text))

    def test_both_sides_supported_states_and_only_claim_fields_are_required(self):
        claims = ({"baseline": {"state": "passed"}},
                  {"baseline": {"state": "passed"}, "candidate": {"state": "passed"}, "other": {}},
                  {"baseline": {"state": "unknown"}, "candidate": {"state": "passed"}},
                  {"baseline": {"state": "passed", "regex": ".*"}, "candidate": {"state": "passed"}},
                  {"baseline": "passed", "candidate": {"state": "passed"}})
        for claim in claims:
            with self.subTest(claim=claim), self.assertRaises(ValueError):
                outcomes.load_expectations(self.expectations({"probe": claim}))
        with self.assertRaises(ValueError):
            outcomes.load_expectations(self.expectations({"": {"baseline": {"state": "passed"},
                                                              "candidate": {"state": "passed"}}}))

    def test_failed_and_error_claims_need_nonvacuous_literal_markers(self):
        for state in ("failed", "error"):
            for marker in (None, "", " \n\t", False, 1, [], {}):
                claim = {"state": state}
                if marker is not None:
                    claim["marker"] = marker
                with self.subTest(state=state, marker=marker), self.assertRaises(ValueError):
                    outcomes.load_expectations(self.expectations({"probe": {
                        "baseline": claim, "candidate": {"state": "passed"}}}))
        with self.assertRaises(ValueError):
            outcomes.load_expectations(self.expectations({"probe": {
                "baseline": {"state": "passed", "marker": ""}, "candidate": {"state": "passed"}}}))


class ObservationTests(Fixtures):
    def test_junit_states_are_distinct(self):
        values = ((None, None, "passed"), ("run", None, "passed"), ("passed", None, "passed"),
                  (None, "failure", "failed"), (None, "error", "error"),
                  (None, "skipped", "skipped"), ("disabled", None, "disabled"),
                  ("disabled", "skipped", "disabled"), ("notrun", None, "notrun"),
                  ("notrun", "skipped", "skipped"))
        for status, kind, expected in values:
            with self.subTest(status=status, kind=kind):
                result = outcomes.read_outcomes(self.junit([testcase(status=status, kind=kind)]))
                self.assertEqual(result["cases"]["probe"]["state"], expected)

    def test_unknown_and_contradictory_statuses_are_unavailable(self):
        cases = [testcase(status="unrecognized"), testcase(status="fail"),
                 testcase(status="skipped"), testcase(status="passed", kind="failure")]
        conflict = testcase(kind="failure")
        ET.SubElement(conflict, "skipped")
        cases.append(conflict)
        for case in cases:
            with self.subTest(attributes=case.attrib):
                found = outcomes.read_outcomes(self.junit([case]), {
                    "probe": {"state": "failed", "marker": "marker"}})["cases"]["probe"]
                self.assertEqual(found["state"], "unknown")
                self.assertTrue(found["reason"])
                self.assertEqual(found["expectation"]["result"], "unavailable")

    def test_ctest_timeout_and_unrecognized_termination_cannot_match_assertion_marker(self):
        for message, state in (("Failed", "failed"), ("Timeout", "error"),
                               ("SEGFAULT", "unknown"), ("Exception", "unknown"),
                               ("Subprocess aborted", "unknown")):
            with self.subTest(message=message):
                path = self.junit([testcase(status="fail", kind="failure", message=message,
                                           stdout=INSERTED + "\nEXPECTED_ASSERTION_MARKER")])
                found = outcomes.read_outcomes(path, {"probe": {
                    "state": "failed", "marker": "EXPECTED_ASSERTION_MARKER"}})["cases"]["probe"]
                self.assertEqual(found["state"], state)
                self.assertEqual(found["expectation"]["result"],
                                 "matched" if state == "failed" else "mismatched" if state == "error" else "unavailable")

    def test_detail_content_and_junit_provenance_are_retained_without_full_output(self):
        case = testcase(name="shader_\u03bb", kind="failure", message="expected < actual",
                        detail="first", stdout="PRIVATE_OUTPUT_NOT_FOR_REPORT", stderr="PRIVATE_ERROR_NOT_FOR_REPORT")
        child = case.find("failure")
        child.set("type", "AssertionError")
        ET.SubElement(child, "part").text = " second"
        path = self.junit([case])
        result = outcomes.read_outcomes(path)
        self.assertEqual(result["junit"], str(path.resolve()))
        self.assertEqual(result["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
        self.assertEqual(result["cases"]["shader_\u03bb"]["details"], [{
            "kind": "failure", "message": "expected < actual", "type": "AssertionError", "text": "first second"}])
        encoded = json.dumps(result)
        self.assertNotIn("PRIVATE_OUTPUT_NOT_FOR_REPORT", encoded)
        self.assertNotIn("PRIVATE_ERROR_NOT_FOR_REPORT", encoded)

    def test_markers_are_literal_and_can_match_each_cases_own_output_or_details(self):
        marker = "[0].* < expected"
        for location in ("stdout", "stderr", "message", "detail", "type"):
            case = testcase(kind="failure", stdout=INSERTED)
            if location in ("stdout", "stderr"):
                case.find("system-out" if location == "stdout" else "system-err").text = marker
            elif location == "type":
                case.find("failure").set("type", marker)
            elif location == "message":
                case.find("failure").set("message", marker)
            else:
                case.find("failure").text = marker
            with self.subTest(location=location):
                found = outcomes.read_outcomes(self.junit([case]), {
                    "probe": {"state": "failed", "marker": marker}})["cases"]["probe"]
                self.assertEqual(found["expectation"]["result"], "matched")
        path = self.junit([testcase(kind="failure", stdout="0xyz < expected")])
        found = outcomes.read_outcomes(path, {"probe": {"state": "failed", "marker": marker}})
        self.assertFalse(found["cases"]["probe"]["expectation"]["marker_match"])

    def test_markers_cannot_be_borrowed_from_other_cases_or_concatenated_channels(self):
        path = self.junit([testcase(kind="failure", stdout="prefix", stderr="suffix"),
                           testcase(name="other", stdout="prefixsuffix")])
        found = outcomes.read_outcomes(path, {"probe": {"state": "failed", "marker": "prefixsuffix"}})
        self.assertEqual(found["cases"]["probe"]["expectation"]["result"], "mismatched")
        passed = outcomes.read_outcomes(self.junit([testcase(stdout="marker")]), {
            "probe": {"state": "failed", "marker": "marker"}})["cases"]["probe"]
        self.assertTrue(passed["expectation"]["marker_match"])
        self.assertFalse(passed["expectation"]["state_match"])
        self.assertEqual(passed["expectation"]["result"], "mismatched")

    def test_duplicate_missing_names_bad_root_and_malformed_xml_are_rejected(self):
        for cases in ([testcase(), testcase()], [ET.Element("testcase")]):
            with self.subTest(cases=len(cases)), self.assertRaises(ValueError):
                outcomes.read_outcomes(self.junit(cases))
        path = self.directory / "bad.xml"
        path.write_text("<results/>", encoding="utf-8")
        with self.assertRaises(ValueError):
            outcomes.read_outcomes(path)
        path.write_text("<testsuite><testcase", encoding="utf-8")
        with self.assertRaises(ET.ParseError):
            outcomes.read_outcomes(path)


class SummaryTests(Fixtures):
    def test_negative_controls_are_evidence_without_strict_gate_waivers(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "wrong answer"},
                                   "candidate": {"state": "passed"}}}
        runs = [self.iteration("baseline", 1, testcase(kind="failure", detail="wrong answer"), declarations, 8),
                self.iteration("candidate", 1, testcase(), declarations)]
        strict = gate.compare(runs)
        evidence = outcomes.summarize_outcomes(runs, {"probe"}, 1, declarations)
        self.assertEqual(strict["exit_code"], 1)
        self.assertEqual(evidence["transitions"], {"failed -> passed": 1})
        self.assertEqual(evidence["expectations"], {"declared": 1, "results": {"matched": 1}})
        self.assertTrue(evidence["observations_complete"])
        self.assertTrue(evidence["execution_coverage_complete"])
        self.assertIn("strict gate exit 1, unchanged", outcomes.format_outcomes(evidence, strict["exit_code"]))

    def test_inherited_failure_details_and_capability_skips_stay_visible(self):
        for kind, coverage in (("failure", True), ("error", False), ("skipped", False)):
            with self.subTest(kind=kind):
                runs = [self.iteration(side, 1, testcase(kind=kind, detail="same details"))
                        for side in ("baseline", "candidate")]
                evidence = outcomes.summarize_outcomes(runs, {"probe"}, 1)
                self.assertTrue(evidence["observations_complete"])
                self.assertEqual(evidence["execution_coverage_complete"], coverage)
                self.assertTrue(evidence["cases"]["probe"]["pairs"][0]["details_equal"])
                self.assertEqual(gate.compare(runs)["exit_code"], 1)
        runs = [self.iteration("baseline", 1, testcase(kind="failure", detail="old cause")),
                self.iteration("candidate", 1, testcase(kind="failure", detail="new cause"))]
        self.assertFalse(outcomes.summarize_outcomes(runs, {"probe"}, 1)["cases"]["probe"]["pairs"][0]["details_equal"])

    def test_repeat_state_variation_and_each_iteration_marker_are_not_averaged_away(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "specific cause"},
                                   "candidate": {"state": "passed"}}}
        runs = [self.iteration("baseline", 1, testcase(kind="failure", detail="specific cause"), declarations, 8),
                self.iteration("candidate", 1, testcase(), declarations),
                self.iteration("baseline", 2, testcase(stdout=INSERTED + "\nspecific cause"), declarations),
                self.iteration("candidate", 2, testcase(), declarations)]
        found = outcomes.summarize_outcomes(runs, {"probe"}, 2, declarations)
        self.assertEqual(found["state_variation_tests"], ["probe"])
        self.assertEqual(found["cases"]["probe"]["expectation"], "mismatched")
        self.assertEqual(found["transitions"], {"failed -> passed": 1, "passed -> passed": 1})
        self.assertEqual(gate.compare(runs)["exit_code"], 1)

    def test_expected_marker_on_the_other_side_cannot_satisfy_a_baseline_claim(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "specific cause"},
                                   "candidate": {"state": "passed"}}}
        runs = [self.iteration("baseline", 1, testcase(kind="failure", detail="different cause"), declarations, 8),
                self.iteration("candidate", 1, testcase(stdout=INSERTED + "\nspecific cause"), declarations)]
        found = outcomes.summarize_outcomes(runs, {"probe"}, 1, declarations)
        pair = found["cases"]["probe"]["pairs"][0]
        self.assertEqual(pair["baseline_expectation"], "mismatched")
        self.assertEqual(pair["candidate_expectation"], "matched")
        self.assertEqual(found["cases"]["probe"]["expectation"], "mismatched")

    def test_unknown_observations_remain_incomplete_even_when_the_legacy_gate_passes(self):
        runs = [self.iteration("baseline", 1, testcase(status="unrecognized")),
                self.iteration("candidate", 1, testcase())]
        self.assertEqual(gate.compare(runs)["exit_code"], 0)
        found = outcomes.summarize_outcomes(runs, {"probe"}, 1)
        self.assertEqual(found["transitions"], {"unknown -> passed": 1})
        self.assertFalse(found["observations_complete"])
        self.assertFalse(found["execution_coverage_complete"])
        self.assertIn("strict gate exit 0, unchanged", outcomes.format_outcomes(found, 0))

    def test_detail_variation_and_marker_mismatch_are_retained_with_missing_repeats(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "expected cause"},
                                   "candidate": {"state": "passed"}}}
        runs = [self.iteration("baseline", 1, testcase(kind="failure", detail="different cause"), declarations, 8),
                self.iteration("candidate", 1, testcase(), declarations)]
        found = outcomes.summarize_outcomes(runs, {"probe"}, 2, declarations)
        self.assertFalse(found["observations_complete"])
        self.assertEqual(found["cases"]["probe"]["expectation"], "mismatched")
        runs += [self.iteration("baseline", 2, testcase(kind="failure", detail="expected cause"), declarations, 8),
                 self.iteration("candidate", 2, testcase(), declarations)]
        found = outcomes.summarize_outcomes(runs, {"probe"}, 2, declarations)
        self.assertTrue(found["cases"]["probe"]["detail_variation"]["baseline"])
        self.assertFalse(found["cases"]["probe"]["state_variation"]["baseline"])
        self.assertEqual(found["cases"]["probe"]["expectation"], "mismatched")

    def test_absent_duplicate_extra_and_invalid_iterations_are_incomplete(self):
        baseline = self.iteration("baseline", 1, testcase())
        candidate = self.iteration("candidate", 1, testcase())
        invalid = dict(candidate, side="other")
        out_of_range = dict(candidate, iteration=2)
        for runs in ([baseline], [baseline, candidate, candidate], [baseline, candidate, invalid],
                     [baseline, candidate, out_of_range]):
            with self.subTest(runs=len(runs)):
                found = outcomes.summarize_outcomes(runs, {"probe"}, 1)
                self.assertFalse(found["observations_complete"])
                self.assertFalse(found["execution_coverage_complete"])
        extra = self.iteration("candidate", 1, testcase(name="unexpected"))
        found = outcomes.summarize_outcomes([baseline, extra], {"probe"}, 1)
        self.assertEqual(found["unexpected_tests"], ["unexpected"])
        self.assertTrue(found["missing_observations"])
        self.assertFalse(found["observations_complete"])

    def test_timeout_disabled_notrun_and_unknown_do_not_complete_execution_coverage(self):
        cases = [testcase(status="fail", kind="failure", message="Timeout"),
                 testcase(status="disabled"), testcase(status="notrun"), testcase(status="unrecognized")]
        for case in cases:
            with self.subTest(attributes=case.attrib):
                runs = [self.iteration("baseline", 1, case), self.iteration("candidate", 1, testcase())]
                self.assertFalse(outcomes.summarize_outcomes(runs, {"probe"}, 1)["execution_coverage_complete"])
        for repeat in (0, -1, True, 1.0, "1", None):
            with self.subTest(repeat=repeat), self.assertRaises(ValueError):
                outcomes.summarize_outcomes([], {"probe"}, repeat)


class GateWiringTests(Fixtures):
    def invoke(self, cases, declarations=None, selected=("probe",), artifact_drift=False, malformed=False,
               expectation_text=None):
        source = {side: self.directory / side for side in ("baseline", "candidate")}
        for path in source.values():
            path.mkdir(exist_ok=True)
        output = self.directory / "output"
        arguments = ["--baseline", str(source["baseline"]), "a" * 40,
                     "--candidate", str(source["candidate"]), "b" * 40,
                     "--output", str(output), "--target", "probe", "--tests", ".*", "--gpu", "test GPU",
                     "--repeat", "1", "--cmake", "fake-cmake", "--ctest", "fake-ctest"]
        if declarations is not None or expectation_text is not None:
            arguments += ["--expect-outcomes", str(self.expectations(declarations, expectation_text))]
        commands = []

        def run(command, log, environment, timeout):
            commands.append(command)
            if command[0] == "fake-cmake":
                log.write_text("simulated build", encoding="utf-8")
                return 0
            if "--show-only=json-v1" in command:
                log.write_text(json.dumps({"tests": [{"name": name, "command": ["probe"], "properties": []}
                                                     for name in selected]}), encoding="utf-8")
                return 0
            junit = Path(command[command.index("--output-junit") + 1])
            side = junit.parent.name
            log.write_text("simulated ctest output", encoding="utf-8")
            if malformed and side == "candidate":
                junit.write_text("<testsuite><testcase", encoding="utf-8")
                return 8
            root = ET.Element("testsuite")
            root.extend(cases[side])
            ET.ElementTree(root).write(junit, encoding="utf-8", xml_declaration=True)
            return 8 if any(case.find("failure") is not None or case.find("error") is not None
                            for case in cases[side]) else 0

        def cache(build):
            return {"CMAKE_HOME_DIRECTORY": str(source[build.parent.name]), "CMAKE_GENERATOR": "Ninja",
                    "CMAKE_BUILD_TYPE": "Release", "CMAKE_CXX_COMPILER": "same-compiler"}

        hashes = mock.Mock(return_value={"probe": "unchanged"})
        if artifact_drift:
            hashes.side_effect = [{"probe": "unchanged"}, {"probe": "unchanged"}, {"probe": "changed"}]
        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch.object(gate, "identity", side_effect=lambda path, commit: {"source": str(path), "commit": commit}), \
                mock.patch.object(gate, "run", side_effect=run), mock.patch.object(gate, "cache_values", side_effect=cache), \
                mock.patch.object(gate, "artifact_hashes", hashes), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = gate.main(arguments)
        report_path = output / "report.json"
        report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else None
        return code, report, commands, stdout.getvalue(), stderr.getvalue(), output

    def test_cli_records_claims_and_raw_references_without_changing_failed_strict_result(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "specific regression"},
                                   "candidate": {"state": "passed"}}}
        code, report, commands, text, _, output = self.invoke({
            "baseline": [testcase(kind="failure", detail="specific regression")], "candidate": [testcase()]}, declarations)
        self.assertEqual(code, 1)
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual(report["outcome_evidence"]["expectations"]["results"], {"matched": 1})
        self.assertEqual(report["outcome_expectations"]["tests"], declarations)
        self.assertIn("strict gate exit 1, unchanged", text)
        self.assertEqual(len([command for command in commands if "--output-junit" in command]), 2)
        for run in report["iterations"]:
            reference = run["outcomes"]
            junit = output / reference["junit"]
            self.assertTrue((output / reference["log"]).is_file())
            self.assertEqual(reference["sha256"], hashlib.sha256(junit.read_bytes()).hexdigest())

    def test_claim_mismatches_do_not_change_a_clean_strict_result(self):
        declarations = {"probe": {"baseline": {"state": "failed", "marker": "marker"},
                                   "candidate": {"state": "passed"}}}
        code, report, _, _, _, _ = self.invoke({"baseline": [testcase()], "candidate": [testcase()]}, declarations)
        self.assertEqual(code, 0)
        self.assertEqual(report["outcome_evidence"]["expectations"]["results"], {"mismatched": 1})

    def test_matched_passing_claims_cannot_suppress_layer_or_validation_problems(self):
        declarations = {"probe": {side: {"state": "passed"} for side in ("baseline", "candidate")}}
        for output in ("no layer", INSERTED + "\nValidation Error: [ VUID-New-1 ] bad state"):
            with self.subTest(output=output):
                self.sequence += 1
                original = self.directory
                self.directory = original / str(self.sequence)
                self.directory.mkdir()
                try:
                    code, report, _, _, _, _ = self.invoke({"baseline": [testcase()],
                                                          "candidate": [testcase(stdout=output)]}, declarations)
                finally:
                    self.directory = original
                self.assertEqual(code, 1)
                self.assertEqual(report["outcome_evidence"]["expectations"]["results"], {"matched": 1})
                self.assertTrue(report["new_diagnostics"] or report["problems"])

    def test_unselected_expectations_are_rejected_before_any_ctest_run(self):
        declarations = {"unselected": {side: {"state": "passed"} for side in ("baseline", "candidate")}}
        code, report, commands, _, _, _ = self.invoke({"baseline": [testcase()], "candidate": [testcase()]}, declarations)
        self.assertEqual(code, 2)
        self.assertIn("unselected", report["error"])
        self.assertFalse(any("--output-junit" in command for command in commands))

    def test_invalid_claim_files_are_rejected_before_build_or_output_creation(self):
        texts = ('{"version":1,"version":1,"tests":{}}',
                 '{"version":1,"tests":{"probe":{"baseline":{"state":"failed"},"candidate":{"state":"passed"}}}}',
                 '{"version":1,"tests":{"probe":{"baseline":{"state":"failed","marker":" "},"candidate":{"state":"passed"}}}}')
        for text in texts:
            with self.subTest(text=text):
                code, report, commands, _, error, output = self.invoke({}, expectation_text=text)
                self.assertEqual(code, 2)
                self.assertIsNone(report)
                self.assertFalse(commands)
                self.assertFalse(output.exists())
                self.assertTrue(error)

    def test_artifact_drift_and_malformed_junit_remain_setup_failures_with_claims(self):
        declarations = {"probe": {side: {"state": "passed"} for side in ("baseline", "candidate")}}
        for mode in ("artifact", "xml"):
            with self.subTest(mode=mode):
                original = self.directory
                self.directory = original / mode
                self.directory.mkdir()
                try:
                    code, report, _, _, _, _ = self.invoke({"baseline": [testcase()], "candidate": [testcase()]},
                                                          declarations, artifact_drift=mode == "artifact", malformed=mode == "xml")
                finally:
                    self.directory = original
                self.assertEqual(code, 2)
                self.assertEqual(report["status"], "incomplete")
                self.assertIn("error", report)


if __name__ == "__main__":
    unittest.main()
