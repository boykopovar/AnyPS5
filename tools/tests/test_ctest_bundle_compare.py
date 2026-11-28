import contextlib
import copy
import hashlib
import io
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ctest_bundle_compare as subject


TOOL = Path(subject.__file__)


def junit(cases, root="testsuite", count=None):
    suite = ET.Element(root, tests=str(len(cases) if count is None else count))
    for item in cases:
        attributes = {"name": item["name"], "status": item.get("status", "run")}
        if "time" in item:
            attributes["time"] = item["time"]
        case = ET.SubElement(suite, "testcase", attributes)
        for detail in item.get("details", []):
            child = ET.SubElement(case, detail["kind"],
                                  {key: value for key, value in detail.items() if key not in ("kind", "text")})
            child.text = detail.get("text", "")
        for channel, field in (("system-out", "out"), ("system-err", "err")):
            values = item.get(field, [])
            if isinstance(values, str):
                values = [values]
            for value in values:
                ET.SubElement(case, channel).text = value
    return ET.tostring(suite, encoding="utf-8", xml_declaration=True)


def failure(name="failure", out="fixture failure", message="Failed"):
    return {"name": name, "status": "fail", "out": out,
            "details": [{"kind": "failure", "message": message}]}


class BundleFixture:
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="anyps5-ctest-comparison-unit-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.sequence = 0

    def bundle(self, cases=None, raw=None, changes=None):
        self.sequence += 1
        directory = self.root / f"synthetic-bundle-{self.sequence}"
        directory.mkdir()
        data = raw if raw is not None else junit(cases if cases is not None else [{"name": "passed"}])
        log = b"Explicitly synthetic unit fixture, not historical CI provenance.\n"
        (directory / "ctest.xml").write_bytes(data)
        (directory / "ctest.log").write_bytes(log)
        source = {"directory": "/synthetic/source", "head": "a" * 40, "tree": "b" * 40,
                  "tracked_dirty": False, "error": None}
        manifest = {
            "format": "anyps5-ctest-bundle/1", "phase": "collected", "directory": "/synthetic/bundle",
            "artifact_name": "synthetic-unit-fixture", "prepared_at": "2026-01-01T00:00:00+00:00",
            "collected_at": "2026-01-01T00:00:01+00:00", "matrix": {"os": "synthetic-linux"},
            "source": source, "source_at_collection": copy.deepcopy(source), "source_matches_preparation": True,
            "ci": {"repository": "synthetic/fixture", "workflow": "Synthetic unit fixture",
                   "workflow_ref": "synthetic/fixture/.github/workflows/fixture.yml@refs/heads/main",
                   "workflow_sha": "c" * 40, "run_id": "1", "run_attempt": "1", "job": "synthetic",
                   "event_name": "synthetic", "event_sha": "d" * 40},
            "runner": {"os": "Linux", "arch": "X64", "name": "synthetic-unit-runner",
                       "image_os": "synthetic-linux", "image_version": "1"},
            "toolchain": {"python": "3.13.0", "winlibs_release": None, "winlibs_zip": None,
                          "winlibs_sha256": None, "winlibs_identifiers_are_configured": True},
            "test": {"selection": "all", "repeat": None, "build_directory": "synthetic-build",
                     "require_vulkan": None, "base_sha": "a" * 40, "pr_heads": [], "pr_numbers": [],
                     "step_outcome": "success", "exit_code": None},
            "reports": {
                "junit": {"path": "ctest.xml", "state": "available_unvalidated", "bytes": len(data),
                          "sha256": hashlib.sha256(data).hexdigest(), "reason": None},
                "log": {"path": "ctest.log", "state": "available_unvalidated", "bytes": len(log),
                        "sha256": hashlib.sha256(log).hexdigest(), "reason": None},
            },
        }
        if changes is not None:
            changes(manifest)
        self.write_manifest(directory, manifest)
        return directory

    def paired_bundles(self, cases=None, raw=None, changes=None):
        return tuple(self.bundle(cases, raw=raw, changes=changes) for _ in range(2))

    def manifest(self, directory):
        return json.loads((directory / "manifest.json").read_bytes())

    def write_manifest(self, directory, value):
        (directory / "manifest.json").write_text(json.dumps(value), encoding="utf-8")

    def change(self, directory, operation):
        value = self.manifest(directory)
        operation(value)
        self.write_manifest(directory, value)

    def compare(self, baseline, candidate):
        return subject.compare_ctest_bundles(subject.load_ctest_bundle(baseline),
                                            subject.load_ctest_bundle(candidate))

    def cli(self, baseline, candidate, *arguments):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = subject.main(["--baseline", str(baseline), "--candidate", str(candidate), *arguments])
        return code, out.getvalue(), err.getvalue()

    def directory_link(self, target, alias):
        try:
            alias.symlink_to(target, target_is_directory=True)
        except OSError as error:
            if os.name != "nt" or getattr(error, "winerror", None) != 1314:
                raise
            import _winapi
            _winapi.CreateJunction(str(target), str(alias))


class BundleValidationTests(BundleFixture, unittest.TestCase):
    def test_collected_synthetic_schema_loads_without_producer_dependency(self):
        directory = self.bundle()
        loaded = subject.load_ctest_bundle(directory)
        self.assertEqual(set(loaded["cases"]), {"passed"})
        self.assertEqual(loaded["cases"]["passed"]["state"], "passed")
        self.assertEqual(loaded["manifest_sha256"], hashlib.sha256((directory / "manifest.json").read_bytes()).hexdigest())

    def test_present_report_tampering_and_declared_size_mismatch_are_invalid(self):
        for filename in ("ctest.xml", "ctest.log"):
            with self.subTest(filename=filename):
                directory = self.bundle()
                path = directory / filename
                path.write_bytes(path.read_bytes() + b"tampered")
                with self.assertRaises(ValueError):
                    subject.load_ctest_bundle(directory)
        directory = self.bundle()
        self.change(directory, lambda value: value["reports"]["junit"].update(bytes=1))
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory)

    def test_physically_missing_available_report_is_a_gap(self):
        directory = self.bundle()
        (directory / "ctest.xml").unlink()
        loaded = subject.load_ctest_bundle(directory)
        self.assertEqual(loaded["cases"], {})
        self.assertIsNone(loaded["junit"])
        self.assertTrue(loaded["gaps"])

    def test_declared_missing_and_omitted_reports_remain_unknown(self):
        for state in ("missing", "omitted"):
            with self.subTest(state=state):
                directory = self.bundle()
                self.change(directory, lambda value: value["reports"]["junit"].update(
                    state=state, bytes=None, sha256=None, reason="synthetic unavailable evidence"))
                loaded = subject.load_ctest_bundle(directory)
                self.assertFalse(loaded["cases"])
                self.assertTrue(loaded["gaps"])

    def test_invalid_phase_format_and_metadata_types_are_rejected(self):
        changes = (lambda value: value.update(phase="prepared"),
                   lambda value: value.update(format="unsupported/9"),
                   lambda value: value.update(source=[]),
                   lambda value: value["reports"]["junit"].update(bytes=True),
                   lambda value: value["reports"]["junit"].update(sha256="not a hash"))
        for operation in changes:
            with self.subTest(operation=operation):
                directory = self.bundle(changes=operation)
                with self.assertRaises(ValueError):
                    subject.load_ctest_bundle(directory)

    def test_duplicate_json_keys_and_nonfinite_constants_are_rejected(self):
        directory = self.bundle()
        path = directory / "manifest.json"
        original = path.read_text(encoding="utf-8")
        path.write_text(original[:-1] + ', "format": "anyps5-ctest-bundle/1"}', encoding="utf-8")
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory)
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value):
                directory = self.bundle(changes=lambda manifest: manifest["matrix"].update(value=value))
                with self.assertRaises(ValueError):
                    subject.load_ctest_bundle(directory)

    def test_manifest_and_report_size_limits_are_enforced(self):
        directory = self.bundle()
        (directory / "manifest.json").write_bytes(b" " * (64 * 1024 + 1))
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory)
        directory = self.bundle()
        with (directory / "ctest.xml").open("wb") as stream:
            stream.truncate(32 * 1024 * 1024 + 1)
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory)

    def test_report_paths_cannot_escape_or_alias_the_manifest(self):
        for path in ("../outside.xml", "manifest.json", "/outside.xml", "sub/ctest.xml"):
            with self.subTest(path=path):
                directory = self.bundle()
                self.change(directory, lambda value: value["reports"]["junit"].update(path=path))
                with self.assertRaises(ValueError):
                    subject.load_ctest_bundle(directory)

    def test_hardlinked_report_is_rejected_without_modifying_target(self):
        directory = self.bundle()
        report = directory / "ctest.xml"
        outside = self.root / "outside.xml"
        original = report.read_bytes()
        os.link(report, outside)
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory)
        self.assertEqual(outside.read_bytes(), original)

    def test_linked_bundle_ancestor_is_rejected(self):
        directory = self.bundle()
        parent = self.root / "real-parent"
        parent.mkdir()
        relocated = parent / directory.name
        directory.rename(relocated)
        alias = self.root / "linked-parent"
        self.directory_link(parent, alias)
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(alias / relocated.name)

    def test_reading_never_changes_input_bytes(self):
        directory = self.bundle([failure()])
        before = {path.name: path.read_bytes() for path in directory.iterdir()}
        subject.load_ctest_bundle(directory)
        self.assertEqual({path.name: path.read_bytes() for path in directory.iterdir()}, before)


class OutcomeTests(BundleFixture, unittest.TestCase):
    def test_recognized_states_keep_skips_and_nonexecution_distinct(self):
        cases = [{"name": "passed"}, failure(),
                 {"name": "error", "status": "error", "details": [{"kind": "error", "text": "fixture error"}]},
                 {"name": "skip", "status": "notrun", "details": [{"kind": "skipped", "message": "fixture skip"}]},
                 {"name": "disabled", "status": "disabled", "details": [{"kind": "skipped"}]},
                 {"name": "notrun", "status": "notrun"}, failure("timeout", message="Timeout")]
        parsed = subject.parse_bundle_junit(junit(cases))
        self.assertEqual({name: case["state"] for name, case in parsed["cases"].items()},
                         {"passed": "passed", "failure": "failed", "error": "error", "skip": "skipped",
                          "disabled": "disabled", "notrun": "notrun", "timeout": "error"})
        self.assertEqual(parsed["cases"]["timeout"]["details"][0]["message"], "Timeout")

    def test_unknown_and_conflicting_outcomes_never_become_passes(self):
        cases = [{"name": "unsupported", "status": "future-status"},
                 {"name": "conflicting", "details": [{"kind": "failure"}, {"kind": "skipped"}]},
                 {"name": "contradiction", "status": "passed", "details": [{"kind": "failure"}]},
                 failure("unrecognized_exit", message="Exit code 0xc0000374")]
        parsed = subject.parse_bundle_junit(junit(cases))
        for case in parsed["cases"].values():
            self.assertEqual(case["state"], "unknown")
            self.assertTrue(case["reason"])

    def test_duplicate_and_missing_case_names_are_invalid(self):
        for raw in (junit([{"name": "same"}, {"name": "same"}]),
                    b'<testsuite><testcase status="run"/></testsuite>',
                    b'<testsuite><testcase name=""/></testsuite>'):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                subject.parse_bundle_junit(raw)

    def test_malformed_wrong_root_and_empty_xml_are_invalid(self):
        for raw in (b"", b"not xml", b"<testsuite>", b"<different/>"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                subject.parse_bundle_junit(raw)

    def test_dtd_and_entities_are_rejected_in_utf8_and_utf16(self):
        text = '<!DOCTYPE testsuite [<!ENTITY sample "expanded">]><testsuite><testcase name="a"><system-out>&sample;</system-out></testcase></testsuite>'
        for encoding in ("utf-8", "utf-16", "utf-16-le", "utf-16-be"):
            with self.subTest(encoding=encoding), self.assertRaises(ValueError):
                subject.parse_bundle_junit(text.encode(encoding))

    def test_nested_suites_are_read_without_inventing_declared_cases(self):
        raw = b'<testsuites tests="900"><testsuite><testcase name="only" status="run"/></testsuite></testsuites>'
        parsed = subject.parse_bundle_junit(raw)
        self.assertEqual(parsed["case_count"], 1)
        self.assertEqual(set(parsed["cases"]), {"only"})
        self.assertTrue(parsed["gaps"])
        report = self.compare(*self.paired_bundles(raw=raw))
        self.assertFalse(report["summary"]["paired_observations_complete"])
        self.assertFalse(report["summary"]["execution_coverage_complete"])

    def test_empty_valid_suite_is_a_coverage_gap(self):
        parsed = subject.parse_bundle_junit(b'<testsuite tests="20"/>')
        self.assertEqual(parsed["cases"], {})
        self.assertEqual(parsed["case_count"], 0)
        self.assertTrue(parsed["gaps"])

    def test_unsupported_structure_and_invalid_counters_prevent_complete_pairing(self):
        reports = (b'<testsuite tests="-1"><testcase name="a"/></testsuite>',
                   b'<testsuite tests="1" failures="0"><testcase name="a"><failure/></testcase></testsuite>',
                   b'<testsuite><future-summary/><testcase name="a"/></testsuite>',
                   b'<testsuite><testcase name="a"><future-result/></testcase></testsuite>')
        for raw in reports:
            with self.subTest(raw=raw):
                parsed = subject.parse_bundle_junit(raw)
                self.assertEqual(parsed["case_count"], 1)
                self.assertTrue(parsed["gaps"])
                report = self.compare(*self.paired_bundles(raw=raw))
                self.assertFalse(report["summary"]["paired_observations_complete"])
                self.assertFalse(report["summary"]["observed_execution_complete"])

    def test_invalid_or_missing_duration_is_not_a_performance_value(self):
        for value in (None, "nan", "inf", "-1", "not-a-number"):
            with self.subTest(value=value):
                item = {"name": "case"}
                if value is not None:
                    item["time"] = value
                case = subject.parse_bundle_junit(junit([item]))["cases"]["case"]
                self.assertIsNone(case["duration_s"])
        case = subject.parse_bundle_junit(junit([{"name": "case", "time": "0.125"}]))["cases"]["case"]
        self.assertEqual(case["duration_s"], 0.125)


class ObservationTests(BundleFixture, unittest.TestCase):
    def test_union_keeps_missing_and_added_cases_visible(self):
        baseline = self.bundle([failure("missing"), {"name": "shared"}])
        candidate = self.bundle([{ "name": "shared"}, failure("added")])
        report = self.compare(baseline, candidate)
        self.assertEqual(report["summary"]["baseline_cases"], 2)
        self.assertEqual(report["summary"]["candidate_cases"], 2)
        self.assertEqual(report["summary"]["shared_cases"], 1)
        self.assertEqual(report["summary"]["missing_cases"], ["missing"])
        self.assertEqual(report["summary"]["added_cases"], ["added"])
        self.assertIsNone(report["cases"]["missing"]["candidate"])
        self.assertFalse(report["summary"]["paired_observations_complete"])

    def test_disjoint_and_zero_cohorts_do_not_imply_coverage(self):
        for left, right in (([{"name": "left"}], [{"name": "right"}]), ([], [])):
            with self.subTest(left=left, right=right):
                report = self.compare(self.bundle(left), self.bundle(right))
                self.assertFalse(report["summary"]["paired_observations_complete"])
                self.assertFalse(report["summary"]["execution_coverage_complete"])

    def test_failed_to_passed_and_failed_to_skip_remain_distinct(self):
        baseline = self.bundle([failure("improved"), failure("not_executed")])
        candidate = self.bundle([{"name": "improved"}, {"name": "not_executed", "status": "notrun",
                                                       "details": [{"kind": "skipped"}]}])
        report = self.compare(baseline, candidate)
        self.assertEqual(report["cases"]["improved"]["transition"], "failed -> passed")
        self.assertEqual(report["cases"]["not_executed"]["transition"], "failed -> skipped")
        self.assertFalse(report["summary"]["execution_coverage_complete"])

    def test_context_changes_and_nulls_are_reported(self):
        baseline = self.bundle()
        candidate = self.bundle(changes=lambda value: value["runner"].update(os="Windows", arch=None))
        report = self.compare(baseline, candidate)
        self.assertTrue(report["context"]["differences"])
        self.assertTrue(report["context"]["unknowns"])
        self.assertFalse(report["summary"]["execution_coverage_complete"])
        self.assertFalse(report["summary"]["observed_execution_complete"])

    def test_matching_observed_cohort_does_not_prove_expected_plan_coverage(self):
        report = self.compare(*self.paired_bundles())
        self.assertTrue(report["summary"]["paired_observations_complete"])
        self.assertTrue(report["summary"]["observed_execution_complete"])
        self.assertFalse(report["summary"]["execution_coverage_complete"])
        self.assertEqual(report["summary"]["expected_plan_coverage"], "unknown")
        self.assertTrue(report["gaps"])

    def test_repeated_final_pass_cannot_prove_retry_history(self):
        baseline, candidate = self.paired_bundles(
            changes=lambda value: value["test"].update(repeat="after-timeout:2"))
        report = self.compare(baseline, candidate)
        self.assertEqual(report["cases"]["passed"]["transition"], "passed -> passed")
        rendered = json.dumps(report).lower()
        self.assertIn("repeat", rendered)
        self.assertIn("unknown", rendered)
        self.assertFalse(report["summary"]["execution_coverage_complete"])
        self.assertFalse(report["summary"]["observed_execution_complete"])

    def test_cancelled_or_skipped_step_does_not_confirm_populated_cohort(self):
        for outcome in ("cancelled", "skipped"):
            with self.subTest(outcome=outcome):
                operation = lambda value: value["test"].update(step_outcome=outcome)
                baseline, candidate = self.paired_bundles(changes=operation)
                report = self.compare(baseline, candidate)
                self.assertEqual(report["cases"]["passed"]["transition"], "passed -> passed")
                self.assertFalse(report["summary"]["paired_observations_complete"])
                self.assertFalse(report["summary"]["execution_coverage_complete"])

    def test_source_base_relation_is_an_observation_not_pr_attribution(self):
        baseline = self.bundle()
        candidate = self.bundle(changes=lambda value: value["test"].update(
            base_sha="a" * 40, pr_heads=["e" * 40, "f" * 40], pr_numbers=["11", "12"]))
        report = self.compare(baseline, candidate)
        self.assertEqual(report["context"]["baseline_source_relation"]["state"], "matches")
        self.assertIn(json.dumps(["e" * 40, "f" * 40]), json.dumps(report["candidate"]))

    def test_valid_expected_source_heads_are_pinned_to_actual_source(self):
        directory = self.bundle()
        loaded = subject.load_ctest_bundle(directory, expected_head="a" * 40)
        self.assertEqual(loaded["expected_head_verification"], "matched")
        self.assertTrue(loaded["source_consistency_verified"])
        with self.assertRaises(ValueError):
            subject.load_ctest_bundle(directory, expected_head="f" * 40)

    def test_unknown_dirty_drifting_and_unconfirmed_expected_pins_fail(self):
        changes = (lambda value: value["source"].update(head=None),
                   lambda value: value["source"].update(tracked_dirty=True),
                   lambda value: value["source"].update(error="SyntheticGitError"),
                   lambda value: value.update(source_matches_preparation=None),
                   lambda value: value["source_at_collection"].update(head="f" * 40),
                   lambda value: value.update(source_matches_preparation=False))
        for operation in changes:
            with self.subTest(operation=operation):
                directory = self.bundle(changes=operation)
                with self.assertRaises(ValueError):
                    subject.load_ctest_bundle(directory, expected_head="a" * 40)

    def test_event_workflow_and_pr_heads_may_differ_from_collected_source(self):
        directory = self.bundle(changes=lambda value: value["test"].update(
            pr_heads=["e" * 40], pr_numbers=["11"]))
        loaded = subject.load_ctest_bundle(directory, expected_head="a" * 40)
        self.assertEqual(loaded["manifest"]["source"]["head"], "a" * 40)
        self.assertEqual(loaded["manifest"]["ci"]["event_sha"], "d" * 40)
        self.assertEqual(loaded["manifest"]["test"]["pr_heads"], ["e" * 40])


class FingerprintTests(BundleFixture, unittest.TestCase):
    def test_equal_failed_elements_with_different_stdout_are_distinguished(self):
        left = self.bundle([failure(out="first failure")])
        right = self.bundle([failure(out="different failure")])
        case = self.compare(left, right)["cases"]["failure"]
        self.assertTrue(case["state_equal"])
        self.assertFalse(case["diagnostics_equal"])
        self.assertFalse(case["output_equal"])

    def test_stderr_and_whitespace_are_not_normalized_away(self):
        original = {"name": "case", "out": "address 0x1234\n", "err": "path /old\n"}
        for changed in ({"err": "path /new\n"}, {"out": "address 0x5678\n"}, {"out": "address 0x1234 \n"}):
            with self.subTest(changed=changed):
                report = self.compare(self.bundle([original]), self.bundle([dict(original, **changed)]))
                self.assertFalse(report["cases"]["case"]["output_equal"])

    def test_duration_changes_case_but_not_diagnostic_fingerprint(self):
        left = {"name": "case", "time": "1", "out": "same"}
        right = dict(left, time="2")
        case = self.compare(self.bundle([left]), self.bundle([right]))["cases"]["case"]
        self.assertTrue(case["diagnostics_equal"])
        self.assertTrue(case["output_equal"])
        self.assertFalse(case["case_equal"])

    def test_multiple_output_elements_and_unicode_are_retained(self):
        raw = junit([{"name": "case", "out": ["\u03bb one", " two\n"], "err": ["warning", "last"]}])
        parsed = subject.parse_bundle_junit(raw)
        case = parsed["cases"]["case"]
        self.assertEqual(case["system_out"], ["\u03bb one", " two\n"])
        self.assertEqual(case["system_err"], ["warning", "last"])
        self.assertEqual(parsed["sha256"], hashlib.sha256(raw).hexdigest())

    def test_canonical_case_hash_is_not_original_xml_serialization(self):
        left = b'<testsuite><testcase name="a" status="run" time="1"/></testsuite>'
        right = b'<testsuite><testcase time="1" status="run" name="a"></testcase></testsuite>'
        first, second = subject.parse_bundle_junit(left), subject.parse_bundle_junit(right)
        self.assertNotEqual(first["sha256"], second["sha256"])
        self.assertEqual(first["cases"]["a"]["fingerprints"]["case_sha256"],
                         second["cases"]["a"]["fingerprints"]["case_sha256"])


class CliTests(BundleFixture, unittest.TestCase):
    def test_json_report_generation_zero_is_not_a_test_pass(self):
        baseline, candidate = self.paired_bundles([failure()])
        code, out, err = self.cli(baseline, candidate, "--json")
        self.assertEqual(code, 0, err)
        report = json.loads(out)
        self.assertEqual(report["format"], "anyps5-ctest-comparison/1")
        self.assertEqual(report["cases"]["failure"]["transition"], "failed -> failed")
        self.assertTrue(report["notes"])

    def test_invalid_artifact_and_expected_pin_failure_return_two(self):
        baseline, candidate = self.paired_bundles()
        for arguments in (("--expect-baseline-head", "a" * 40, "--expect-candidate-head", "f" * 40),):
            code, out, err = self.cli(baseline, candidate, *arguments)
            self.assertEqual(code, 2)
            self.assertTrue(err)
        (candidate / "ctest.xml").write_bytes(b"corrupt")
        code, out, err = self.cli(baseline, candidate)
        self.assertEqual(code, 2)
        self.assertTrue(err)

    def test_expected_heads_are_an_optional_pair(self):
        baseline, candidate = self.paired_bundles()
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            try:
                code = subject.main(["--baseline", str(baseline), "--candidate", str(candidate),
                                     "--expect-baseline-head", "a" * 40])
            except SystemExit as error:
                code = error.code
        self.assertEqual(code, 2)

    def test_nullable_local_context_and_success_with_skips_do_not_invent_passes(self):
        def local(value):
            value["ci"] = {key: None for key in value["ci"]}
            value["runner"] = {key: None for key in value["runner"]}
            value["test"].update(selection=None, repeat=None, step_outcome="success")
        case = {"name": "not_executed", "status": "notrun", "details": [{"kind": "skipped"}]}
        baseline, candidate = self.paired_bundles([case], changes=local)
        code, out, err = self.cli(baseline, candidate, "--json")
        self.assertEqual(code, 0, err)
        report = json.loads(out)
        self.assertEqual(report["cases"]["not_executed"]["candidate"]["state"], "skipped")
        self.assertFalse(report["summary"]["execution_coverage_complete"])
        self.assertTrue(report["context"]["unknowns"])

    def test_text_report_and_json_keep_partial_observations_visible(self):
        baseline, candidate = self.bundle([failure("missing")]), self.bundle([failure("added")])
        code, out, err = self.cli(baseline, candidate)
        self.assertEqual(code, 0, err)
        self.assertIn("missing", out)
        self.assertIn("added", out)
        self.assertIn("observ", out.lower())
        self.assertNotIn("verdict: no regressions", out.lower())
        code, out, err = self.cli(baseline, candidate, "--json")
        self.assertEqual(code, 0, err)
        report = json.loads(out)
        self.assertFalse(report["summary"]["paired_observations_complete"])
        self.assertNotIn(report.get("verdict"), ("passed", "safe", "no_regressions", "baseline_exonerates"))
        self.assertFalse(report.get("no_regressions", False))

    def test_cli_executes_no_recorded_commands(self):
        baseline, candidate = self.paired_bundles()
        with mock.patch.object(subprocess, "Popen", side_effect=AssertionError("artifact reader executed a command")):
            code, out, err = self.cli(baseline, candidate, "--json")
        self.assertEqual(code, 0, err)
        self.assertEqual(json.loads(out)["summary"]["baseline_cases"], 1)


class CTestIntegrationTests(BundleFixture, unittest.TestCase):
    def test_real_cpu_ctest_mixed_outcomes_are_observed_without_changing_results(self):
        cmake, ctest, ninja = shutil.which("cmake"), shutil.which("ctest"), shutil.which("ninja")
        if not cmake or not ctest or not ninja:
            self.skipTest("CMake, CTest and Ninja are unavailable")
        source = self.root / "synthetic-source"
        source.mkdir()
        script = source / "case.py"
        script.write_text("import sys, time\nmode = sys.argv[1]\n"
                          "if mode == 'failed':\n print('synthetic failure'); sys.exit(3)\n"
                          "if mode == 'skip':\n sys.exit(77)\n"
                          "if mode == 'timeout':\n print('synthetic timeout', flush=True); time.sleep(2)\n",
                          encoding="utf-8")
        python = Path(sys.executable).as_posix()
        project = "cmake_minimum_required(VERSION 3.20)\nproject(SyntheticComparison NONE)\nenable_testing()\n"
        for name, mode in (("passed", "passed"), ("failed", "failed"), ("skip", "skip"),
                           ("timeout", "timeout"), ("disabled", "passed"), ("setup", "failed"), ("consumer", "passed")):
            project += f'add_test(NAME {name} COMMAND "{python}" "${{CMAKE_CURRENT_SOURCE_DIR}}/case.py" {mode})\n'
        project += "set_tests_properties(skip PROPERTIES SKIP_RETURN_CODE 77)\n"
        project += "set_tests_properties(timeout PROPERTIES TIMEOUT 0.25)\n"
        project += "set_tests_properties(disabled PROPERTIES DISABLED TRUE)\n"
        project += "set_tests_properties(setup PROPERTIES FIXTURES_SETUP unavailable_fixture)\n"
        project += "set_tests_properties(consumer PROPERTIES FIXTURES_REQUIRED unavailable_fixture)\n"
        project += 'add_test(NAME missing COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/missing-program")\n'
        (source / "CMakeLists.txt").write_text(project, encoding="utf-8")
        build = self.root / "synthetic-build"
        configured = subprocess.run([cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                                     f"-DCMAKE_MAKE_PROGRAM={ninja}"], capture_output=True,
                                    text=True, timeout=15)
        self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
        baseline = self.root / "actual-baseline.xml"
        candidate = self.root / "actual-candidate.xml"
        command = [ctest, "--test-dir", str(build), "--parallel", "1", "--output-on-failure", "--output-junit"]
        first = subprocess.run([*command, str(baseline)], capture_output=True, text=True, timeout=15)
        second = subprocess.run([*command, str(candidate)], capture_output=True, text=True, timeout=15)
        self.assertEqual(first.returncode, 8, first.stdout + first.stderr)
        self.assertEqual(second.returncode, first.returncode, second.stdout + second.stderr)
        original = (baseline.read_bytes(), candidate.read_bytes())
        left, right = self.bundle(raw=original[0]), self.bundle(raw=original[1])
        report = self.compare(left, right)
        self.assertEqual(report["summary"]["baseline_cases"], 8)
        self.assertEqual(report["summary"]["candidate_cases"], 8)
        self.assertEqual(report["cases"]["passed"]["baseline"]["state"], "passed")
        self.assertEqual(report["cases"]["failed"]["baseline"]["state"], "failed")
        self.assertEqual(report["cases"]["skip"]["baseline"]["state"], "skipped")
        self.assertEqual(report["cases"]["disabled"]["baseline"]["state"], "disabled")
        self.assertNotEqual(report["cases"]["consumer"]["baseline"]["state"], "passed")
        self.assertNotEqual(report["cases"]["missing"]["baseline"]["state"], "passed")
        self.assertFalse(report["summary"]["execution_coverage_complete"])
        self.assertEqual((baseline.read_bytes(), candidate.read_bytes()), original)


if __name__ == "__main__":
    unittest.main()
