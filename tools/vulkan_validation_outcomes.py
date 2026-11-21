import collections
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


STATES = ("passed", "failed", "error", "skipped", "disabled", "notrun")
SIDES = ("baseline", "candidate")


def load_expectations(path):
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate outcome expectation key: {key}")
            result[key] = value
        return result

    raw = Path(path).read_bytes()
    value = json.loads(raw, object_pairs_hook=unique_object)
    if not isinstance(value, dict) or set(value) != {"version", "tests"}:
        raise ValueError("outcome expectations require only version and tests")
    if type(value["version"]) is not int or value["version"] != 1 or not isinstance(value["tests"], dict):
        raise ValueError("outcome expectations require version 1 and a tests object")
    for name, sides in value["tests"].items():
        if not name or not isinstance(sides, dict) or set(sides) != set(SIDES):
            raise ValueError(f"{name!r}: outcome expectation requires baseline and candidate")
        for side, claim in sides.items():
            if not isinstance(claim, dict) or not {"state"} <= set(claim) <= {"state", "marker"}:
                raise ValueError(f"{name!r} {side}: expected state and optional literal marker")
            if claim["state"] not in STATES:
                raise ValueError(f"{name!r} {side}: unsupported expected state")
            marker = claim.get("marker")
            if "marker" in claim and (not isinstance(marker, str) or not marker.strip()):
                raise ValueError(f"{name!r} {side}: marker must be a nonempty literal string")
            if claim["state"] in ("failed", "error") and marker is None:
                raise ValueError(f"{name!r} {side}: a failed/error claim requires a literal marker")
    return {"version": 1, "path": str(Path(path).resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
            "tests": value["tests"]}


def read_outcomes(path, declarations=None):
    raw = Path(path).read_bytes()
    root = ET.fromstring(raw)
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError(f"{path}: expected JUnit testsuite or testsuites")
    cases = {}
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name or name in cases:
            raise ValueError(f"{path}: missing or duplicate testcase name")
        details = [{"kind": child.tag, "message": child.get("message"), "type": child.get("type"),
                    "text": "".join(child.itertext())}
                   for child in case if child.tag in ("failure", "error", "skipped")]
        kinds = {detail["kind"] for detail in details}
        status = case.get("status")
        state, reason = "unknown", None
        if status not in (None, "run", "passed", "fail", "failed", "error", "skipped", "disabled", "notrun"):
            reason = "unsupported JUnit status"
        elif len(kinds) > 1:
            reason = "conflicting JUnit outcome elements"
        elif status == "disabled" and not kinds.difference({"skipped"}):
            state = "disabled"
        elif kinds:
            kind = next(iter(kinds))
            compatible = {"failure": (None, "run", "fail", "failed"),
                          "error": (None, "run", "fail", "failed", "error"),
                          "skipped": (None, "run", "skipped", "notrun")}
            if status not in compatible[kind]:
                reason = "JUnit status contradicts outcome element"
            else:
                state = {"failure": "failed", "error": "error", "skipped": "skipped"}[kind]
                if kind == "failure" and status == "fail":
                    messages = {detail["message"] for detail in details}
                    if messages == {"Timeout"}:
                        state = "error"
                    elif messages.difference({None, "", "Failed"}):
                        state, reason = "unknown", "unrecognized CTest failure termination"
        elif status == "notrun":
            state = "notrun"
        elif status in (None, "run", "passed"):
            state = "passed"
        else:
            reason = "JUnit status lacks its outcome element"
        observation = {"state": state, "junit_status": status, "details": details, "reason": reason}
        claim = (declarations or {}).get(name)
        if claim is not None:
            text = [case.findtext(channel) or "" for channel in ("system-out", "system-err")]
            text += [detail[key] or "" for detail in details for key in ("message", "type", "text")]
            marker = claim.get("marker")
            state_match = state == claim["state"]
            marker_match = None if marker is None else any(marker in part for part in text)
            result = ("unavailable" if state == "unknown" else
                      "matched" if state_match and marker_match is not False else "mismatched")
            observation["expectation"] = {"claim": dict(claim), "state_match": state_match,
                                           "marker_match": marker_match, "result": result}
        cases[name] = observation
    return {"version": 1, "junit": str(Path(path).resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
            "cases": cases}


def summarize_outcomes(iterations, selected, repeat, declarations=None):
    if type(repeat) is not int or repeat <= 0:
        raise ValueError("outcome evidence requires a positive repetition count")
    declarations = declarations or {}
    selected = set(selected)
    indexed = collections.defaultdict(list)
    unexpected = set()
    for run in iterations:
        indexed[(run["side"], run["iteration"])].append(run)
        unexpected.update(set(run.get("outcomes", {}).get("cases", {})) - selected)
    duplicates = [{"side": side, "iteration": iteration} for (side, iteration), runs in sorted(indexed.items())
                  if len(runs) != 1 or side not in SIDES or iteration not in range(1, repeat + 1)]
    complete = bool(selected) and not unexpected and not duplicates
    coverage = complete
    transitions, expectation_counts = collections.Counter(), collections.Counter()
    cases, missing = {}, []
    for name in sorted(selected | unexpected | set(declarations)):
        pairs, states, detail_states, claims = [], {side: [] for side in SIDES}, {side: [] for side in SIDES}, []
        for iteration in range(1, repeat + 1):
            pair = {"iteration": iteration}
            observations = {}
            for side in SIDES:
                runs = indexed[(side, iteration)]
                observed = runs[0].get("outcomes", {}).get("cases", {}).get(name) if len(runs) == 1 else None
                observations[side] = observed
                state = observed["state"] if observed else "missing"
                pair[side] = state
                if observed is None:
                    missing.append({"test": name, "side": side, "iteration": iteration})
                else:
                    states[side].append(state)
                    detail_states[side].append(json.dumps(observed.get("details", []), sort_keys=True))
                if state in ("unknown", "missing"):
                    complete = False
                if state not in ("passed", "failed"):
                    coverage = False
                if name in declarations:
                    match = observed.get("expectation", {}).get("result", "unavailable") if observed else "unavailable"
                    pair[side + "_expectation"] = match
                    claims.append(match)
            transition = pair["baseline"] + " -> " + pair["candidate"]
            transitions[transition] += 1
            pair["transition"] = transition
            pair["details_equal"] = (observations["baseline"]["details"] == observations["candidate"]["details"]
                                     if all(observations.values()) else None)
            pairs.append(pair)
        record = {"pairs": pairs, "state_variation": {side: len(set(states[side])) > 1 for side in SIDES},
                  "detail_variation": {side: len(set(detail_states[side])) > 1 for side in SIDES}}
        if name in declarations:
            result = "mismatched" if "mismatched" in claims else "unavailable" if "unavailable" in claims else "matched"
            record["expectation"] = result
            expectation_counts[result] += 1
        cases[name] = record
    return {"version": 1, "observations_complete": complete, "execution_coverage_complete": coverage and complete,
            "transitions": dict(sorted(transitions.items())), "cases": cases,
            "missing_observations": missing, "unexpected_tests": sorted(unexpected), "invalid_iterations": duplicates,
            "state_variation_tests": [name for name, case in cases.items() if any(case["state_variation"].values())],
            "expectations": {"declared": len(declarations), "results": dict(sorted(expectation_counts.items()))}}


def format_outcomes(evidence, strict_exit):
    lines = [f"Outcome observations (strict gate exit {strict_exit}, unchanged):"]
    lines.extend(f"  {count} paired observations: {transition}" for transition, count in evidence["transitions"].items())
    complete = "complete" if evidence["observations_complete"] else "incomplete"
    coverage = "complete" if evidence["execution_coverage_complete"] else "incomplete"
    lines += [f"  observations: {complete}; selected-case execution coverage: {coverage}",
              f"  tests with observed state variation: {len(evidence['state_variation_tests'])}"]
    claims = evidence["expectations"]
    if claims["declared"]:
        lines.append("  declared expectations: " + ", ".join(f"{count} {result}" for result, count in claims["results"].items()))
    return "\n".join(lines)
