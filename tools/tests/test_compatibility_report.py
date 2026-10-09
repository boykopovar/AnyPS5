import contextlib
import copy
import io
import json
import subprocess
import tempfile
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compatibility_report


def launch_failure_report():
    unavailable = {
        "title.id": "Synthetic fixture; no actual title was used.",
        "title.version": "Synthetic fixture; no actual title was used.",
        "build.commit": "Synthetic fixture; no emulator build was run.",
        "host.os": "Synthetic fixture; no host run occurred.",
        "session.exit_code": "No process was started.",
    }
    return {
        "schema_version": 1,
        "report_id": "synthetic-launch-failure",
        "title": {"id": None, "version": None},
        "build": {"commit": None},
        "host": {"os": None},
        "session": {
            "exit_reason": "launch_failure",
            "exit_code": None,
        },
        "progress": {
            "level": "not_tested",
            "objective": None,
            "milestones": [],
        },
        "checks": [
            {
                "name": "interactive_gameplay",
                "result": "untested",
                "source": "tester",
                "notes": "Launch failed before gameplay could be observed.",
                "evidence": [],
            }
        ],
        "unavailable_data": [
            {"field": field, "reason": reason}
            for field, reason in unavailable.items()
        ],
    }


class CompatibilityReportTests(unittest.TestCase):
    def test_partial_launch_failure_is_accepted_without_changes(self):
        report = launch_failure_report()
        original = copy.deepcopy(report)

        compatibility_report.validate_report(report)

        self.assertEqual(report, original)

    def test_completed_requires_an_objective(self):
        report = launch_failure_report()
        report["session"] = {
            "exit_reason": "normal_exit",
            "exit_code": 0,
        }
        report["unavailable_data"] = [
            entry for entry in report["unavailable_data"]
            if entry["field"] != "session.exit_code"
        ]
        report["progress"] = {
            "level": "completed",
            "objective": None,
            "milestones": [
                {
                    "name": "ending_observed",
                    "source": "tester",
                    "evidence": ["clips/synthetic-ending.webm"],
                }
            ],
        }

        with self.assertRaisesRegex(
                compatibility_report.ReportError,
                r"progress\.objective",
        ):
            compatibility_report.validate_report(report)

    def test_completed_requires_milestone_evidence(self):
        report = launch_failure_report()
        report["session"] = {
            "exit_reason": "normal_exit",
            "exit_code": 0,
        }
        report["unavailable_data"] = [
            entry for entry in report["unavailable_data"]
            if entry["field"] != "session.exit_code"
        ]
        report["checks"] = []
        report["progress"] = {
            "level": "completed",
            "objective": "Finish the synthetic test scenario.",
            "milestones": [
                {
                    "name": "ending_observed",
                    "source": "tester",
                    "evidence": [],
                }
            ],
        }

        with self.assertRaisesRegex(
                compatibility_report.ReportError,
                r"progress\.milestones",
        ):
            compatibility_report.validate_report(report)

    def test_invalid_check_status_is_rejected(self):
        for result in ("unknown", "success", "", None, True):
            with self.subTest(result=result):
                report = launch_failure_report()
                report["checks"][0]["result"] = result

                with self.assertRaisesRegex(
                        compatibility_report.ReportError,
                        r"checks\[0\]\.result",
                ):
                    compatibility_report.validate_report(report)

    def test_unavailable_value_requires_a_reason(self):
        for field in (
                "title.id",
                "title.version",
                "build.commit",
                "host.os",
                "session.exit_code",
        ):
            with self.subTest(field=field):
                report = launch_failure_report()
                report["unavailable_data"] = [
                    entry for entry in report["unavailable_data"]
                    if entry["field"] != field
                ]

                with self.assertRaises(
                        compatibility_report.ReportError
                ) as raised:
                    compatibility_report.validate_report(report)

                self.assertIn(f"report.{field}:", str(raised.exception))

    def test_schema_version_must_be_supported_integer(self):
        for version in (0, 2, "1", None, True, 1.0):
            with self.subTest(version=version):
                report = launch_failure_report()
                report["schema_version"] = version

                with self.assertRaisesRegex(
                        compatibility_report.ReportError,
                        r"report\.schema_version",
                ):
                    compatibility_report.validate_report(report)

    def test_required_top_level_fields_cannot_be_omitted(self):
        for field in (
                "schema_version",
                "report_id",
                "title",
                "build",
                "host",
                "session",
                "progress",
                "checks",
                "unavailable_data",
        ):
            with self.subTest(field=field):
                report = launch_failure_report()
                del report[field]

                with self.assertRaises(
                        compatibility_report.ReportError
                ) as raised:
                    compatibility_report.validate_report(report)

                self.assertIn(f"report.{field}:", str(raised.exception))

    def assert_invalid(self, report, location):
        with self.assertRaises(compatibility_report.ReportError) as raised:
            compatibility_report.validate_report(report)
        self.assertIn(location, str(raised.exception))

    def observed_report(self, level="completed"):
        report = launch_failure_report()
        report["report_id"] = "synthetic-observed-session"
        report["session"] = {"exit_reason": "normal_exit", "exit_code": 0}
        report["unavailable_data"] = [
            entry for entry in report["unavailable_data"]
            if entry["field"] != "session.exit_code"
        ]
        report["progress"] = {
            "level": level,
            "objective": "Finish the synthetic scenario.",
            "milestones": [{
                "name": "synthetic_observation",
                "source": "tester",
                "evidence": ["clips/synthetic-observation.webm"],
            }],
        }
        report["checks"][0]["notes"] = "This separate check was not exercised."
        return report

    def test_known_metadata_is_accepted(self):
        report = self.observed_report()
        report["title"] = {"id": "synthetic-title", "version": "test-version"}
        report["build"]["commit"] = "a" * 40
        report["host"]["os"] = "synthetic-host"
        report["unavailable_data"] = []
        original = copy.deepcopy(report)
        compatibility_report.validate_report(report)
        self.assertEqual(report, original)

    def test_all_check_results_are_preserved(self):
        for result in ("pass", "fail", "untested", "not_applicable"):
            with self.subTest(result=result):
                report = self.observed_report()
                report["checks"][0]["result"] = result
                report["checks"][0]["evidence"] = ["logs/synthetic-check.txt"]
                original = copy.deepcopy(report)
                compatibility_report.validate_report(report)
                self.assertEqual(report, original)

    def test_unknown_identity_values_require_reasons_and_are_preserved(self):
        for section, name in (("title", "id"), ("title", "version"), ("build", "commit"), ("host", "os")):
            with self.subTest(field=f"{section}.{name}"):
                report = launch_failure_report()
                report[section][name] = "unknown"
                original = copy.deepcopy(report)
                compatibility_report.validate_report(report)
                self.assertEqual(report, original)
                report["unavailable_data"] = [
                    entry for entry in report["unavailable_data"]
                    if entry["field"] != f"{section}.{name}"
                ]
                self.assert_invalid(report, f"report.{section}.{name}")

    def test_every_observed_level_requires_evidence(self):
        for level in ("boots", "reaches_gameplay", "gameplay_tested", "completed"):
            with self.subTest(level=level):
                report = self.observed_report(level)
                compatibility_report.validate_report(report)
                for milestones in ([], [{"name": "observation", "source": "tester", "evidence": []}]):
                    report["progress"]["milestones"] = milestones
                    self.assert_invalid(report, "report.progress.milestones")

    def test_invalid_milestone_shapes_and_references(self):
        for milestone in (None, [], {}, {"name": "observed", "source": "tester"}):
            with self.subTest(milestone=milestone):
                report = self.observed_report()
                report["progress"]["milestones"] = [milestone]
                self.assert_invalid(report, "report.progress.milestones[0]")
        for evidence in (None, "clip.webm", [], [""], ["   "], [None], [1], [{}]):
            with self.subTest(evidence=evidence):
                report = self.observed_report()
                report["progress"]["milestones"][0]["evidence"] = evidence
                self.assert_invalid(report, "report.progress.milestones[0].evidence")

    def test_completed_objective_must_be_nonempty_text(self):
        for objective in (None, "", "  ", 12, True, [], {}):
            with self.subTest(objective=objective):
                report = self.observed_report()
                report["progress"]["objective"] = objective
                self.assert_invalid(report, "report.progress.objective")

    def test_invalid_sources_are_rejected_for_checks_and_milestones(self):
        for source in ("unknown", "", None, [], {}):
            for kind in ("checks", "milestones"):
                with self.subTest(source=source, kind=kind):
                    report = self.observed_report()
                    entries = report["checks"] if kind == "checks" else report["progress"]["milestones"]
                    entries[0]["source"] = source
                    self.assert_invalid(report, f"{kind}[0].source")

    def test_allowed_sources_are_preserved(self):
        for source in ("collector", "instrumentation", "tester", "replay", "external_reference"):
            with self.subTest(source=source):
                report = self.observed_report()
                report["checks"][0]["source"] = source
                report["progress"]["milestones"][0]["source"] = source
                original = copy.deepcopy(report)
                compatibility_report.validate_report(report)
                self.assertEqual(report, original)

    def test_pass_and_fail_checks_require_evidence(self):
        for result in ("pass", "fail"):
            with self.subTest(result=result):
                report = launch_failure_report()
                report["checks"][0]["result"] = result
                self.assert_invalid(report, "report.checks[0].evidence")

    def test_duplicate_check_names_are_rejected(self):
        report = launch_failure_report()
        report["checks"].append(copy.deepcopy(report["checks"][0]))
        report["checks"][1]["result"] = "not_applicable"
        self.assert_invalid(report, "report.checks[1].name")

    def test_invalid_metadata_types_and_commit_ids(self):
        for section, name in (("title", "id"), ("title", "version"), ("build", "commit"), ("host", "os")):
            for value in ("", "  ", 1, True, [], {}):
                with self.subTest(field=f"{section}.{name}", value=value):
                    report = launch_failure_report()
                    report[section][name] = value
                    self.assert_invalid(report, f"report.{section}.{name}")
        for commit in ("abc123", "z" * 40, "a" * 39, "a" * 41):
            report = launch_failure_report()
            report["build"]["commit"] = commit
            self.assert_invalid(report, "report.build.commit")

    def test_missing_nested_fields_are_rejected(self):
        for section, fields in (
            ("title", ("id", "version")), ("build", ("commit",)), ("host", ("os",)),
            ("session", ("exit_reason", "exit_code")), ("progress", ("level", "objective", "milestones")),
        ):
            for field in fields:
                with self.subTest(section=section, field=field):
                    report = launch_failure_report()
                    del report[section][field]
                    self.assert_invalid(report, f"report.{section}.{field}")
        for field in ("name", "result", "source", "notes", "evidence"):
            report = launch_failure_report()
            del report["checks"][0][field]
            self.assert_invalid(report, f"report.checks[0].{field}")

    def test_wrong_container_types_are_rejected(self):
        for value in (None, True, 1, "report", []):
            self.assert_invalid(value, "report")
        for section in ("title", "build", "host", "session", "progress"):
            for value in (None, [], "object", 1):
                with self.subTest(section=section, value=value):
                    report = launch_failure_report()
                    report[section] = value
                    self.assert_invalid(report, f"report.{section}")
        for section in ("checks", "unavailable_data"):
            for value in (None, {}, "list", 1):
                report = launch_failure_report()
                report[section] = value
                self.assert_invalid(report, f"report.{section}")
        for value in (None, {}, "list", 1):
            report = launch_failure_report()
            report["progress"]["milestones"] = value
            self.assert_invalid(report, "report.progress.milestones")

    def test_invalid_progress_and_session_values_are_rejected(self):
        for value in (None, "invalid", "", [], {}, True):
            for section, name in (("progress", "level"), ("session", "exit_reason")):
                report = launch_failure_report()
                report[section][name] = value
                self.assert_invalid(report, f"report.{section}.{name}")
        for code in (True, False, "0", "unknown", 1.0, [], {}):
            report = launch_failure_report()
            report["session"]["exit_code"] = code
            self.assert_invalid(report, "report.session.exit_code")

    def test_invalid_unavailable_entries_are_rejected(self):
        for entry in (None, [], {}, {"field": "build.commit"}, {"field": "build.commit", "reason": "  "},
                      {"field": "build.typo", "reason": "Unavailable"}):
            report = launch_failure_report()
            report["unavailable_data"] = [entry]
            self.assert_invalid(report, "report.unavailable_data[0]")
        report = launch_failure_report()
        report["unavailable_data"].append(copy.deepcopy(report["unavailable_data"][0]))
        self.assert_invalid(report, "duplicate unavailable")
        report = launch_failure_report()
        report["title"]["id"] = "synthetic-known-title"
        self.assert_invalid(report, "report.title.id has a known value")

    def test_prelaunch_outcomes_cannot_claim_progress_or_runtime_exit(self):
        for reason in ("not_run", "conversion_failure", "launch_failure"):
            with self.subTest(reason=reason):
                report = self.observed_report()
                report["session"] = {"exit_reason": reason, "exit_code": None}
                self.assert_invalid(report, "report.progress.level")
                report = launch_failure_report()
                report["session"] = {"exit_reason": reason, "exit_code": 0}
                self.assert_invalid(report, "report.session.exit_code")

    def test_normal_exit_does_not_promote_progress(self):
        report = self.observed_report("not_tested")
        report["progress"]["milestones"] = []
        report["progress"]["objective"] = None
        original = copy.deepcopy(report)
        compatibility_report.validate_report(report)
        self.assertEqual(report, original)
        report["session"]["exit_code"] = 1
        self.assert_invalid(report, "report.session.exit_code")

    def test_not_tested_cannot_claim_observed_milestones(self):
        report = self.observed_report("not_tested")
        self.assert_invalid(report, "report.progress.milestones")

    def test_runtime_failures_and_collector_errors_remain_distinct(self):
        for reason, code in (("nonzero_exit", 1), ("crash", -11), ("user_stop", 0), ("timeout", None)):
            with self.subTest(reason=reason):
                report = self.observed_report()
                report["session"] = {"exit_reason": reason, "exit_code": code}
                if code is None:
                    report["unavailable_data"].append({"field": "session.exit_code", "reason": "Exit code not collected."})
                report["collector_errors"] = ["Synthetic artifact copy failed."]
                report["known_issues"] = ["Synthetic graphics defect."]
                original = copy.deepcopy(report)
                compatibility_report.validate_report(report)
                self.assertEqual(report, original)
        report = self.observed_report()
        report["session"] = {"exit_reason": "nonzero_exit", "exit_code": 0}
        self.assert_invalid(report, "report.session.exit_code")

    def test_unknown_session_outcome_requires_a_reason(self):
        report = launch_failure_report()
        report["session"]["exit_reason"] = "unknown"
        self.assert_invalid(report, "report.session.exit_reason")
        report["unavailable_data"].append({"field": "session.exit_reason", "reason": "Outcome was not recorded."})
        original = copy.deepcopy(report)
        compatibility_report.validate_report(report)
        self.assertEqual(report, original)

    def test_invalid_collector_errors_are_rejected(self):
        for errors in (None, "error", {}, [""], [None]):
            report = launch_failure_report()
            report["collector_errors"] = errors
            self.assert_invalid(report, "report.collector_errors")

    def test_json_reader_accepts_utf8_bom(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "report.json"
            report = launch_failure_report()
            path.write_text(json.dumps(report), encoding="utf-8-sig")
            self.assertEqual(compatibility_report.read_report(path), report)

    def test_json_reader_rejects_ambiguous_or_malformed_input(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "report.json"
            for data in (b"[1", b"\xff", b'{"checks": [], "checks": []}',
                         b'{"nested": {"result": "fail", "result": "pass"}}',
                         b'{"value": NaN}', b'{"value": Infinity}', b'{"value": -Infinity}', b'{"value": 1e999}'):
                with self.subTest(data=data):
                    path.write_bytes(data)
                    with self.assertRaises(compatibility_report.ReportError) as raised:
                        compatibility_report.read_report(path)
                    self.assertIn(str(path), str(raised.exception))
            with self.assertRaises(compatibility_report.ReportError):
                compatibility_report.read_report(Path(root) / "missing.json")

    def test_main_reports_structure_only_and_preserves_input_file(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "report.json"
            path.write_text(json.dumps(launch_failure_report()), encoding="utf-8")
            before = path.read_bytes()
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                code = compatibility_report.main([str(path)])
            self.assertEqual((code, err.getvalue()), (0, ""))
            self.assertIn("structure only; gameplay not verified", out.getvalue())
            self.assertEqual(path.read_bytes(), before)

    def test_main_returns_two_with_field_and_file_on_invalid_report(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "report.json"
            report = launch_failure_report()
            report["checks"][0]["result"] = "success"
            path.write_text(json.dumps(report), encoding="utf-8")
            out, err = io.StringIO(), io.StringIO()
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                code = compatibility_report.main([str(path)])
            self.assertEqual((code, out.getvalue()), (2, ""))
            self.assertIn(str(path), err.getvalue())
            self.assertIn("report.checks[0].result", err.getvalue())
            self.assertNotIn("Traceback", err.getvalue())

    def test_cli_process_exit_codes(self):
        script = Path(compatibility_report.__file__).resolve()
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "report.json"
            path.write_text(json.dumps(launch_failure_report()), encoding="utf-8")
            result = subprocess.run([sys.executable, str(script), str(path)], capture_output=True, text=True, timeout=10)
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            path.write_text("{}", encoding="utf-8")
            result = subprocess.run([sys.executable, str(script), str(path)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertIn("report.schema_version", result.stderr)
            path.unlink()
            result = subprocess.run([sys.executable, str(script), str(path)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertNotIn("Traceback", result.stderr)


if __name__ == "__main__":
    unittest.main()
