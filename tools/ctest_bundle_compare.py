import argparse
import collections
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import sys
import xml.etree.ElementTree as ET


BUNDLE_FORMAT = "anyps5-ctest-bundle/1"
FORMAT = "anyps5-ctest-comparison/1"
MAX_MANIFEST_BYTES = 64 * 1024
MAX_REPORT_BYTES = 32 * 1024 * 1024
MAX_XML_DEPTH = 64
MAX_XML_NODES = 200000
MAX_CASES = 50000
STATES = ("passed", "failed", "error", "skipped", "disabled", "notrun", "unknown")
NOTES = [
    "These are recorded observations; matching names, states or diagnostics do not establish test equivalence or causality.",
    "The bundle format records checkout provenance and configured settings, not a per-test command/environment inventory or proof of the executed binaries or hardware.",
    "CTest JUnit can retain only the final retry result; omitted attempts cannot establish stability or observed variation.",
    "Completeness fields describe retained testcase observations only; expected-plan execution coverage remains unconfirmed.",
    "CTest can truncate process output. Case and diagnostic fingerprints describe retained parsed evidence, not necessarily full process output.",
    "Exit 0 means that this observation report was produced, including failures and gaps; it is not a test-pass verdict.",
]


def _fingerprint(value):
    raw = json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
                     allow_nan=False).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def _gap(code, message, **fields):
    return dict(code=code, message=message, **fields)


def _regular(info):
    return (stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and
            not getattr(info, "st_file_attributes", 0) & 0x400)


def _file_identity(info):
    return info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns


def read_bundle_file(path, limit=MAX_REPORT_BYTES, missing_ok=False):
    path = Path(path)
    try:
        before = path.lstat()
    except FileNotFoundError:
        if missing_ok:
            return None
        raise
    if not _regular(before):
        raise ValueError(f"{path}: expected a regular unlinked file without a reparse point")
    if before.st_size > limit:
        raise ValueError(f"{path}: exceeds the {limit}-byte limit")
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if not _regular(opened) or _file_identity(opened) != _file_identity(before):
            raise ValueError(f"{path}: changed while opening")
        raw = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    current = path.lstat()
    if (len(raw) > limit or len(raw) != before.st_size or not _regular(after) or
            not _regular(current) or _file_identity(after) != _file_identity(before) or
            _file_identity(current) != _file_identity(before)):
        raise ValueError(f"{path}: changed while reading")
    return raw


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_constant(value):
    raise ValueError(f"nonfinite JSON constant: {value}")


def _bounded_json(value):
    pending = [(value, 0)]
    nodes = 0
    while pending:
        current, depth = pending.pop()
        nodes += 1
        if depth > 32 or nodes > 10000:
            raise ValueError("manifest exceeds JSON structure limits")
        if isinstance(current, dict):
            pending.extend((item, depth + 1) for item in current.values())
        elif isinstance(current, list):
            pending.extend((item, depth + 1) for item in current)
        elif isinstance(current, float) and not math.isfinite(current):
            raise ValueError("manifest contains a nonfinite number")


def _object(value, label):
    if not isinstance(value, dict):
        raise ValueError(f"{label}: expected an object")
    return value


def _string(value, label, limit=1024):
    if value is not None and (not isinstance(value, str) or len(value) > limit or "\x00" in value):
        raise ValueError(f"{label}: expected a bounded string or null")


def _sha(value, label, lengths=(40, 64)):
    if value is not None and (not isinstance(value, str) or len(value) not in lengths or
                              re.fullmatch(r"[0-9a-f]+", value) is None):
        raise ValueError(f"{label}: invalid lowercase hash")


def _source(value, label):
    value = _object(value, label)
    for field in ("directory", "error"):
        _string(value.get(field), f"{label}.{field}")
    for field in ("head", "tree"):
        _sha(value.get(field), f"{label}.{field}")
    if value.get("tracked_dirty") is not None and type(value["tracked_dirty"]) is not bool:
        raise ValueError(f"{label}.tracked_dirty: expected a boolean or null")
    return value


def _validate_manifest(manifest):
    _object(manifest, "manifest")
    if manifest.get("format") != BUNDLE_FORMAT or manifest.get("phase") != "collected":
        raise ValueError("expected a collected anyps5-ctest-bundle/1 manifest")
    for section in ("ci", "runner", "toolchain", "test", "reports"):
        _object(manifest.get(section), section)
    for section in ("ci", "runner"):
        for key, value in manifest[section].items():
            _string(value, f"{section}.{key}")
    for key, value in manifest["toolchain"].items():
        if key == "winlibs_identifiers_are_configured":
            if type(value) is not bool:
                raise ValueError("toolchain.winlibs_identifiers_are_configured: expected a boolean")
        else:
            _string(value, f"toolchain.{key}")
    if manifest.get("matrix") is not None:
        _object(manifest["matrix"], "matrix")
    source = _source(manifest.get("source"), "source")
    collected = manifest.get("source_at_collection")
    if collected is not None:
        _source(collected, "source_at_collection")
    matches = manifest.get("source_matches_preparation")
    if matches is not None and type(matches) is not bool:
        raise ValueError("source_matches_preparation: expected a boolean or null")
    if matches is True and (source != collected or source.get("tracked_dirty") is True or
                            source.get("error") is not None):
        raise ValueError("source_matches_preparation contradicts source evidence")
    test = manifest["test"]
    for field in ("selection", "repeat", "build_directory", "require_vulkan", "step_outcome"):
        _string(test.get(field), f"test.{field}")
    if test.get("step_outcome") not in (None, "success", "failure", "skipped", "cancelled"):
        raise ValueError("test.step_outcome: unsupported outcome")
    if test.get("exit_code") is not None and type(test["exit_code"]) is not int:
        raise ValueError("test.exit_code: expected an integer or null")
    _sha(test.get("base_sha"), "test.base_sha")
    heads, numbers = test.get("pr_heads", []), test.get("pr_numbers", [])
    if not isinstance(heads, list) or not isinstance(numbers, list) or len(heads) != len(numbers):
        raise ValueError("test: ordered PR heads and numbers must be equally sized arrays")
    for head in heads:
        if head is None:
            raise ValueError("test.pr_heads: null head")
        _sha(head, "test.pr_heads")
    for number in numbers:
        if not isinstance(number, str) or re.fullmatch(r"[1-9][0-9]*", number) is None:
            raise ValueError("test.pr_numbers: invalid PR number")
    if set(manifest["reports"]) != {"junit", "log"}:
        raise ValueError("reports must contain exactly junit and log descriptors")
    for kind, filename in (("junit", "ctest.xml"), ("log", "ctest.log")):
        record = _object(manifest["reports"][kind], f"reports.{kind}")
        if record.get("path") != filename:
            raise ValueError(f"reports.{kind}: expected allowlisted path {filename}")
        if record.get("state") not in ("available_unvalidated", "missing", "omitted"):
            raise ValueError(f"reports.{kind}: unsupported report state")
        size = record.get("bytes")
        if size is not None and (type(size) is not int or size < 0):
            raise ValueError(f"reports.{kind}: invalid byte count")
        _sha(record.get("sha256"), f"reports.{kind}.sha256", (64,))
        _string(record.get("reason"), f"reports.{kind}.reason")
        if record["state"] == "available_unvalidated":
            if size is None or size > MAX_REPORT_BYTES or record.get("sha256") is None:
                raise ValueError(f"reports.{kind}: available report needs a bounded size and SHA256")


class _BoundedTreeBuilder(ET.TreeBuilder):
    def __init__(self):
        super().__init__()
        self.depth = 0
        self.nodes = 0

    def start(self, tag, attributes):
        self.depth += 1
        self.nodes += 1
        if self.depth > MAX_XML_DEPTH or self.nodes > MAX_XML_NODES:
            raise ValueError("JUnit exceeds XML structure limits")
        return super().start(tag, attributes)

    def end(self, tag):
        result = super().end(tag)
        self.depth -= 1
        return result

    def doctype(self, name, public_id, system_id):
        raise ValueError("JUnit DTD and entity declarations are not allowed")


def _element_value(element):
    return {"tag": element.tag, "attributes": dict(element.attrib), "text": element.text,
            "tail": element.tail, "children": [_element_value(child) for child in element]}


def _case_state(status, details):
    accepted = (None, "run", "passed", "fail", "failed", "error", "skipped", "disabled", "notrun")
    kinds = {detail["kind"] for detail in details}
    if status not in accepted:
        return "unknown", "unsupported JUnit status"
    if len(kinds) > 1:
        return "unknown", "conflicting JUnit outcome elements"
    if status == "disabled" and not kinds.difference({"skipped"}):
        return "disabled", None
    if kinds:
        kind = next(iter(kinds))
        compatible = {"failure": (None, "run", "fail", "failed"),
                      "error": (None, "run", "fail", "failed", "error"),
                      "skipped": (None, "run", "skipped", "notrun")}
        if status not in compatible[kind]:
            return "unknown", "JUnit status contradicts outcome element"
        if kind == "failure" and status == "fail":
            messages = {detail["message"] for detail in details}
            if messages == {"Timeout"}:
                return "error", None
            if messages.difference({None, "", "Failed"}):
                return "unknown", "unrecognized CTest failure termination"
        return {"failure": "failed", "error": "error", "skipped": "skipped"}[kind], None
    if status == "notrun":
        return "notrun", None
    if status in (None, "run", "passed"):
        return "passed", None
    return "unknown", "JUnit status lacks its outcome element"


def parse_bundle_junit(raw):
    if not isinstance(raw, bytes) or len(raw) > MAX_REPORT_BYTES:
        raise ValueError("JUnit must be bounded bytes")
    parser = ET.XMLParser(target=_BoundedTreeBuilder())
    try:
        parser.feed(raw)
        root = parser.close()
    except (ET.ParseError, RecursionError) as error:
        raise ValueError(f"malformed JUnit: {error}") from error
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError("expected a JUnit testsuite or testsuites root")
    cases, gaps, declared_counters = {}, [], []
    for suite in root.iter():
        if suite.tag not in ("testsuite", "testsuites"):
            continue
        allowed = {"testsuite", "properties", "system-out", "system-err"}
        allowed.add("testcase" if suite.tag == "testsuite" else "testsuites")
        for child in suite:
            if child.tag not in allowed:
                gaps.append(_gap("unknown_junit_structure", "unsupported suite child", tag=child.tag))
        observed = list(suite.iter("testcase"))
        skipped = sum(any(child.tag == "skipped" for child in case) for case in observed)
        disabled = sum(case.get("status") == "disabled" for case in observed)
        actual = {"tests": {len(observed)},
                  "failures": {sum(any(child.tag == "failure" for child in case) for case in observed)},
                  "errors": {sum(any(child.tag == "error" for child in case) for case in observed)},
                  "skipped": {skipped, sum(case.get("status") != "disabled" and
                                            any(child.tag == "skipped" for child in case) for case in observed)},
                  "disabled": {disabled}}
        for field, counts in actual.items():
            value = suite.get(field)
            if value is None:
                continue
            record = {"suite": suite.get("name"), "field": field, "declared": value,
                      "observed": sorted(counts)}
            declared_counters.append(record)
            if re.fullmatch(r"[0-9]{1,12}", value) is None:
                gaps.append(_gap("invalid_suite_counter", "suite counter is not a bounded nonnegative integer", **record))
            elif int(value) not in counts:
                gaps.append(_gap("suite_counter_mismatch", "declared suite counter contradicts retained cases", **record))
    for element in root.iter("testcase"):
        name = element.get("name")
        if not name or not name.strip() or name in cases:
            raise ValueError("JUnit contains a missing or duplicate testcase name")
        if len(cases) >= MAX_CASES:
            raise ValueError("JUnit exceeds testcase count limit")
        details = [{"kind": child.tag, "message": child.get("message"), "type": child.get("type"),
                    "text": "".join(child.itertext()), "attributes": dict(child.attrib)}
                   for child in element if child.tag in ("failure", "error", "skipped")]
        output = {channel: ["".join(child.itertext()) for child in element if child.tag == tag]
                  for channel, tag in (("system_out", "system-out"), ("system_err", "system-err"))}
        status = element.get("status")
        state, reason = _case_state(status, details)
        if any(child.tag not in ("properties", "failure", "error", "skipped", "system-out", "system-err")
               for child in element):
            state, reason = "unknown", "unsupported testcase child structure"
        duration = None
        if element.get("time") is not None:
            try:
                duration = float(element.get("time"))
                if not math.isfinite(duration) or duration < 0:
                    raise ValueError
            except (ValueError, OverflowError):
                duration = None
                gaps.append(_gap("invalid_duration", "test duration is unavailable", test=name))
        if state == "unknown":
            gaps.append(_gap("unknown_case_state", reason, test=name))
        diagnostics = dict(status=status, details=details, **output)
        cases[name] = dict(name=name, state=state, status=status, reason=reason, duration_s=duration,
                           attributes=dict(element.attrib), details=details, **output,
                           fingerprints={"case_sha256": _fingerprint(_element_value(element)),
                                         "diagnostics_sha256": _fingerprint(diagnostics),
                                         "output_sha256": _fingerprint(output)})
    if not cases:
        gaps.append(_gap("empty_suite", "JUnit contains no testcase observations"))
    return {"sha256": hashlib.sha256(raw).hexdigest(), "cases": cases, "case_count": len(cases),
            "state_counts": dict(sorted(collections.Counter(case["state"] for case in cases.values()).items())),
            "declared_counters": declared_counters,
            "counts_consistent": not any(gap["code"] in ("invalid_suite_counter", "suite_counter_mismatch") for gap in gaps),
            "gaps": gaps}


def load_ctest_bundle(directory, expected_head=None):
    if expected_head is not None:
        _sha(expected_head, "expected source head")
    directory = Path(os.path.abspath(directory))
    for part in (directory, *directory.parents):
        info = part.lstat()
        if not stat.S_ISDIR(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise ValueError("bundle directory and ancestors must not be links or reparse points")
    raw = read_bundle_file(directory / "manifest.json", MAX_MANIFEST_BYTES)
    try:
        manifest = json.loads(raw, object_pairs_hook=_unique_object, parse_constant=_reject_constant)
        _bounded_json(manifest)
    except (UnicodeError, RecursionError) as error:
        raise ValueError(f"invalid manifest JSON: {error}") from error
    _validate_manifest(manifest)
    source, collected = manifest["source"], manifest.get("source_at_collection")
    verified = (manifest.get("source_matches_preparation") is True and source == collected and
                source.get("tracked_dirty") is False and source.get("error") is None and
                source.get("head") is not None and source.get("tree") is not None)
    if expected_head is not None and (source.get("head") != expected_head or not verified):
        raise ValueError("expected source head cannot be verified against clean unchanged source evidence")
    reports, gaps, parsed = {}, [], None
    if not verified:
        gaps.append(_gap("unverified_source", "clean unchanged source identity is not confirmed"))
    for kind, filename in (("junit", "ctest.xml"), ("log", "ctest.log")):
        descriptor = manifest["reports"][kind]
        record = dict(descriptor, availability=descriptor["state"], verified=False)
        if descriptor["state"] != "available_unvalidated":
            gaps.append(_gap("unavailable_report", f"{kind} report is {descriptor['state']}", report=kind))
        else:
            data = read_bundle_file(directory / filename, MAX_REPORT_BYTES, missing_ok=True)
            if data is None:
                record["availability"] = "unavailable"
                gaps.append(_gap("unavailable_report", f"{kind} report file is absent", report=kind))
            else:
                digest = hashlib.sha256(data).hexdigest()
                if len(data) != descriptor["bytes"] or digest != descriptor["sha256"]:
                    raise ValueError(f"{kind}: report size or SHA256 does not match the manifest")
                record.update(availability="available", verified=True, actual_sha256=digest, actual_bytes=len(data))
                if kind == "junit":
                    parsed = parse_bundle_junit(data)
                    gaps.extend(parsed["gaps"])
        reports[kind] = record
    if manifest["test"].get("exit_code") is None:
        gaps.append(_gap("unknown_exit_code", "the original test exit code is unavailable"))
    if manifest["test"].get("step_outcome") in (None, "skipped", "cancelled"):
        gaps.append(_gap("step_execution_gap", "the recorded step does not confirm execution"))
    return {"format": "anyps5-ctest-bundle-loaded/1", "directory": directory.as_posix(),
            "manifest": manifest, "manifest_sha256": hashlib.sha256(raw).hexdigest(), "reports": reports,
            "junit": parsed, "cases": parsed["cases"] if parsed is not None else {}, "gaps": gaps,
            "source_consistency_verified": verified,
            "expected_head_verification": "matched" if expected_head is not None else "not_requested"}


def _field(value, path):
    for name in path.split("."):
        if not isinstance(value, dict) or name not in value:
            return None
        value = value[name]
    return value


def ctest_bundle_context(baseline, candidate):
    fields = ("ci.repository", "ci.workflow", "ci.workflow_sha", "ci.job", "matrix", "runner.os",
              "runner.arch", "runner.image_os", "runner.image_version", "toolchain.python",
              "toolchain.winlibs_release", "toolchain.winlibs_zip", "toolchain.winlibs_sha256",
              "toolchain.winlibs_identifiers_are_configured", "test.selection", "test.repeat",
              "test.require_vulkan")
    manifests = [baseline["manifest"], candidate["manifest"]]
    differences, unknowns = [], []
    for field in fields:
        left, right = (_field(manifest, field) for manifest in manifests)
        record = {"field": field, "baseline": left, "candidate": right}
        non_windows = all(_field(manifest, "runner.os") not in (None, "Windows") for manifest in manifests)
        not_applicable = field.startswith("toolchain.winlibs_") and non_windows
        null_matrix = field == "matrix" and all("matrix" in manifest for manifest in manifests)
        if not not_applicable and not null_matrix and (left is None or right is None):
            unknowns.append(record)
        if left != right:
            differences.append(record)
    head = _field(manifests[0], "source.head")
    requested_base = _field(manifests[1], "test.base_sha")
    relation = {"state": "unknown" if head is None or requested_base is None else
                "matches" if head == requested_base else "differs",
                "baseline_source_head": head, "candidate_base_sha": requested_base}
    return {"differences": differences, "unknowns": unknowns, "baseline_source_relation": relation}


def _arm_summary(bundle):
    junit = bundle["junit"]
    return {"directory": bundle["directory"], "manifest_sha256": bundle["manifest_sha256"],
            "manifest": bundle["manifest"], "reports": bundle["reports"],
            "source_consistency_verified": bundle["source_consistency_verified"],
            "expected_head_verification": bundle["expected_head_verification"],
            "case_count": len(bundle["cases"]),
            "junit_summary": {field: junit[field] for field in ("case_count", "state_counts", "counts_consistent", "declared_counters")}
            if junit is not None else None}


def compare_ctest_bundles(baseline, candidate):
    context = ctest_bundle_context(baseline, candidate)
    left, right = baseline["cases"], candidate["cases"]
    cases, transitions = {}, collections.Counter()
    for name in sorted(set(left) | set(right)):
        before, after = left.get(name), right.get(name)
        states = [case["state"] if case is not None else "missing" for case in (before, after)]
        transition = " -> ".join(states)
        transitions[transition] += 1
        record = {"baseline": before, "candidate": after, "transition": transition,
                  "state_equal": before is not None and after is not None and states[0] == states[1]}
        for key, fingerprint in (("case_equal", "case_sha256"), ("diagnostics_equal", "diagnostics_sha256"),
                                 ("output_equal", "output_sha256")):
            record[key] = (before["fingerprints"][fingerprint] == after["fingerprints"][fingerprint]
                           if before is not None and after is not None else None)
        cases[name] = record
    gaps = [dict(gap, side=side) for side, bundle in (("baseline", baseline), ("candidate", candidate))
            for gap in bundle["gaps"]]
    added, missing = sorted(set(right) - set(left)), sorted(set(left) - set(right))
    if added or missing:
        gaps.append(_gap("unpaired_cases", "the observed testcase name sets differ"))
    if context["differences"]:
        gaps.append(_gap("context_differences", "recorded execution contexts differ"))
    if context["unknowns"] or context["baseline_source_relation"]["state"] != "matches":
        gaps.append(_gap("unconfirmed_context", "recorded metadata does not confirm complete comparison context"))
    gaps.append(_gap("unknown_expected_plan", "bundle v1 does not record the expected testcase and attempt inventory"))
    bundles = (baseline, candidate)
    observed_cases = list(left.values()) + list(right.values())
    complete = (bool(left) and bool(right) and not added and not missing and
                all(not bundle["junit"]["gaps"] and
                    bundle["manifest"]["test"].get("step_outcome") in ("success", "failure")
                    for bundle in bundles) and
                all(case["state"] != "unknown" for case in observed_cases))
    execution_unknowns = [record for record in context["unknowns"]
                          if record["field"] not in ("test.repeat", "test.require_vulkan") or
                          not all(record["field"].split(".")[1] in bundle["manifest"]["test"]
                                  for bundle in bundles)]
    execution = (complete and not context["differences"] and not execution_unknowns and
                 context["baseline_source_relation"]["state"] == "matches" and
                 all(bundle["source_consistency_verified"] and
                     bundle["manifest"]["test"].get("repeat") is None for bundle in bundles) and
                 all(case["state"] in ("passed", "failed") for case in observed_cases))
    return {"format": FORMAT, "baseline": _arm_summary(baseline), "candidate": _arm_summary(candidate),
            "context": context, "cases": cases,
            "summary": {"baseline_cases": len(left), "candidate_cases": len(right),
                        "shared_cases": len(set(left) & set(right)), "added_cases": added, "missing_cases": missing,
                        "state_counts": {side: dict(sorted(collections.Counter(case["state"] for case in bundle["cases"].values()).items()))
                                         for side, bundle in (("baseline", baseline), ("candidate", candidate))},
                        "transitions": dict(sorted(transitions.items())), "paired_observations_complete": complete,
                        "observed_execution_complete": execution, "expected_plan_coverage": "unknown",
                        "execution_coverage_complete": False},
            "gaps": gaps, "notes": list(NOTES)}


def render_ctest_bundle_comparison(report):
    summary = report["summary"]
    lines = ["Pinned CTest bundle observations", ""]
    for side in ("baseline", "candidate"):
        arm = report[side]
        manifest = arm["manifest"]
        identity = {"head": manifest["source"].get("head"), "tree": manifest["source"].get("tree"),
                    "run": manifest["ci"].get("run_id"), "attempt": manifest["ci"].get("run_attempt"),
                    "job": manifest["ci"].get("job"), "matrix": manifest.get("matrix")}
        lines.append(f"{side}: " + json.dumps(identity, ensure_ascii=False, sort_keys=True))
        lines.append(f"  cases: {summary[side + '_cases']}; states: " +
                     json.dumps(summary["state_counts"][side], sort_keys=True))
    lines += ["", "Transitions:"]
    lines.extend(f"  {transition}: {count}" for transition, count in summary["transitions"].items())
    lines.append(f"Paired retained observations: {'complete' if summary['paired_observations_complete'] else 'incomplete'}; "
                 "expected-plan execution coverage: not confirmed")
    lines.append(f"Context differences: {len(report['context']['differences'])}; "
                 f"unknown fields: {len(report['context']['unknowns'])}; "
                 f"baseline/source-base relation: {report['context']['baseline_source_relation']['state']}")
    noteworthy = [(name, row) for name, row in report["cases"].items()
                  if row["transition"] != "passed -> passed" or row["diagnostics_equal"] is not True]
    if noteworthy:
        lines += ["", f"Case observations (showing {min(50, len(noteworthy))} of {len(noteworthy)}; JSON retains all):"]
        for name, row in noteworthy[:50]:
            lines.append(f"  {json.dumps(name, ensure_ascii=False)}: {row['transition']}; "
                         f"retained diagnostics equal: {row['diagnostics_equal']}")
    for key in ("differences", "unknowns"):
        rows = report["context"][key]
        if rows:
            lines += ["", f"Context {key} (showing {min(20, len(rows))} of {len(rows)}):"]
            lines.extend("  " + json.dumps(row, ensure_ascii=False, sort_keys=True) for row in rows[:20])
    if report["gaps"]:
        lines += ["", f"Evidence gaps (showing {min(20, len(report['gaps']))} of {len(report['gaps'])}):"]
        lines.extend("  " + json.dumps(gap, ensure_ascii=False, sort_keys=True) for gap in report["gaps"][:20])
    lines.append("")
    lines.extend(report["notes"])
    return "\n".join(lines)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare retained CTest bundle observations without changing test verdicts.")
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--candidate", required=True, type=Path)
    parser.add_argument("--expect-baseline-head")
    parser.add_argument("--expect-candidate-head")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    if (args.expect_baseline_head is None) != (args.expect_candidate_head is None):
        parser.error("expected source heads must be supplied together")
    try:
        baseline = load_ctest_bundle(args.baseline, args.expect_baseline_head)
        candidate = load_ctest_bundle(args.candidate, args.expect_candidate_head)
        report = compare_ctest_bundles(baseline, candidate)
        print(json.dumps(report, ensure_ascii=False, sort_keys=True, allow_nan=False) if args.json else
              render_ctest_bundle_comparison(report))
    except (OSError, ValueError, TypeError, RecursionError) as error:
        print(f"CTest bundle comparison: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
