import argparse
import json
import math
import re
import sys
from pathlib import Path

CHECK_RESULTS = ("pass", "fail", "untested", "not_applicable")
OBSERVATION_SOURCES = ("collector", "instrumentation", "tester", "replay", "external_reference")
PROGRESS_LEVELS = ("not_tested", "boots", "reaches_gameplay", "gameplay_tested", "completed")
EXIT_REASONS = (
    "not_run", "conversion_failure", "launch_failure", "normal_exit",
    "nonzero_exit", "crash", "user_stop", "timeout", "unknown",
)
UNAVAILABLE_FIELDS = (
    "title.id", "title.version", "build.commit", "host.os",
    "session.exit_reason", "session.exit_code",
)


class ReportError(Exception):
    pass


def require_fields(value, fields, location):
    if not isinstance(value, dict):
        raise ReportError(f"{location}: expected an object")
    for field in fields:
        if field not in value:
            raise ReportError(f"{location}.{field}: required field is missing")


def require_text(value, location):
    if not isinstance(value, str) or not value.strip():
        raise ReportError(f"{location}: expected a nonempty string")


def require_choice(value, choices, location):
    if not isinstance(value, str) or value not in choices:
        raise ReportError(f"{location}: expected one of {', '.join(choices)}")


def require_list(value, location):
    if not isinstance(value, list):
        raise ReportError(f"{location}: expected a list")


def validate_evidence(value, location, required=False):
    require_list(value, location)
    if required and not value:
        raise ReportError(f"{location}: expected at least one evidence reference")
    for index, reference in enumerate(value):
        require_text(reference, f"{location}[{index}]")


def validate_identity(report):
    for section, field in (("title", "id"), ("title", "version"), ("build", "commit"), ("host", "os")):
        value = report[section][field]
        if value is not None:
            require_text(value, f"report.{section}.{field}")
    commit = report["build"]["commit"]
    if commit is not None and commit != "unknown" and not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ReportError("report.build.commit: expected a full 40-character commit ID, null or 'unknown'")


def validate_progress(progress):
    require_choice(progress["level"], PROGRESS_LEVELS, "report.progress.level")
    if progress["objective"] is not None:
        require_text(progress["objective"], "report.progress.objective")
    if progress["level"] == "completed":
        require_text(progress["objective"], "report.progress.objective")
    milestones = progress["milestones"]
    require_list(milestones, "report.progress.milestones")
    if progress["level"] != "not_tested" and not milestones:
        raise ReportError("report.progress.milestones: observed progress requires milestone evidence")
    if progress["level"] == "not_tested" and milestones:
        raise ReportError("report.progress.milestones: not_tested cannot claim observed progress milestones")
    for index, milestone in enumerate(milestones):
        location = f"report.progress.milestones[{index}]"
        require_fields(milestone, ("name", "source", "evidence"), location)
        require_text(milestone["name"], f"{location}.name")
        require_choice(milestone["source"], OBSERVATION_SOURCES, f"{location}.source")
        validate_evidence(milestone["evidence"], f"{location}.evidence", required=True)


def validate_checks(checks):
    require_list(checks, "report.checks")
    names = set()
    for index, check in enumerate(checks):
        location = f"report.checks[{index}]"
        require_fields(check, ("name", "result", "source", "notes", "evidence"), location)
        require_text(check["name"], f"{location}.name")
        if check["name"] in names:
            raise ReportError(f"{location}.name: duplicate check name {check['name']!r}")
        names.add(check["name"])
        require_choice(check["result"], CHECK_RESULTS, f"{location}.result")
        require_choice(check["source"], OBSERVATION_SOURCES, f"{location}.source")
        require_text(check["notes"], f"{location}.notes")
        validate_evidence(check["evidence"], f"{location}.evidence", required=check["result"] in ("pass", "fail"))


def validate_session(session, progress):
    reason, code = session["exit_reason"], session["exit_code"]
    require_choice(reason, EXIT_REASONS, "report.session.exit_reason")
    if code is not None and type(code) is not int:
        raise ReportError("report.session.exit_code: expected an integer or null")
    if reason in ("not_run", "conversion_failure", "launch_failure"):
        if code is not None:
            raise ReportError("report.session.exit_code: no runtime exit code before launch")
        if progress["level"] != "not_tested":
            raise ReportError("report.progress.level: observed progress contradicts the session exit reason")
    if reason == "normal_exit" and code is not None and code != 0:
        raise ReportError("report.session.exit_code: normal_exit requires zero or an explicitly unavailable code")
    if reason == "nonzero_exit" and code == 0:
        raise ReportError("report.session.exit_code: nonzero_exit cannot have code zero")


def validate_unavailable(report):
    unavailable = report["unavailable_data"]
    require_list(unavailable, "report.unavailable_data")
    fields = set()
    for index, entry in enumerate(unavailable):
        location = f"report.unavailable_data[{index}]"
        require_fields(entry, ("field", "reason"), location)
        require_choice(entry["field"], UNAVAILABLE_FIELDS, f"{location}.field")
        require_text(entry["reason"], f"{location}.reason")
        field = entry["field"]
        if field in fields:
            raise ReportError(f"{location}.field: duplicate unavailable field {field!r}")
        section, name = field.split(".")
        value = report[section][name]
        if value is not None and value != "unknown":
            raise ReportError(f"{location}.field: report.{field} has a known value")
        fields.add(field)
    for field in UNAVAILABLE_FIELDS:
        section, name = field.split(".")
        value = report[section][name]
        if (value is None or value == "unknown") and field not in fields:
            raise ReportError(f"report.{field}: unavailable value requires a reason")


def validate_report(report):
    require_fields(report, (
        "schema_version", "report_id", "title", "build", "host", "session",
        "progress", "checks", "unavailable_data",
    ), "report")
    version = report["schema_version"]
    if type(version) is not int or version != 1:
        raise ReportError("report.schema_version: expected integer 1")
    require_text(report["report_id"], "report.report_id")
    for section, fields in (
        ("title", ("id", "version")), ("build", ("commit",)), ("host", ("os",)),
        ("session", ("exit_reason", "exit_code")), ("progress", ("level", "objective", "milestones")),
    ):
        require_fields(report[section], fields, f"report.{section}")
    validate_identity(report)
    validate_progress(report["progress"])
    validate_checks(report["checks"])
    validate_session(report["session"], report["progress"])
    validate_unavailable(report)
    if "collector_errors" in report:
        require_list(report["collector_errors"], "report.collector_errors")
        for index, error in enumerate(report["collector_errors"]):
            require_text(error, f"report.collector_errors[{index}]")


def unique_object(pairs):
    value = {}
    for name, item in pairs:
        if name in value:
            raise ReportError(f"duplicate JSON field {name!r}")
        value[name] = item
    return value


def finite_number(value):
    number = float(value)
    if not math.isfinite(number):
        raise ReportError(f"non-finite JSON number {value!r}")
    return number


def read_report(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object,
                          parse_float=finite_number, parse_constant=finite_number)
    except (OSError, UnicodeError, ValueError, RecursionError, ReportError) as error:
        raise ReportError(f"{path}: cannot read report: {error}") from error


def main(argv=None):
    parser = argparse.ArgumentParser(description="validate a compatibility report's structure and evidence references")
    parser.add_argument("report", type=Path, help="version 1 report.json; validation does not verify gameplay")
    args = parser.parse_args(argv)
    try:
        report = read_report(args.report)
        validate_report(report)
    except ReportError as error:
        print(f"FAIL: {args.report}: {error}", file=sys.stderr)
        return 2
    print(f"{args.report}: report valid (structure only; gameplay not verified)")
    return 0


if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    raise SystemExit(main())
