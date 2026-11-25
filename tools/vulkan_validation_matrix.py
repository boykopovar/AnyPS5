import argparse
from io import BytesIO
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import vulkan_validation_gate as gate


SIDES = ("baseline", "candidate")
DEVICE = re.compile(
    r"(?:\[[^\]\r\n]*VulkanDevice\.cpp:\d+ [^\]\r\n]+\] )?"
    r"Selected GPU name=(.+) vendor=(0x[0-9a-fA-F]+|[0-9]+) device=(0x[0-9a-fA-F]+|[0-9]+)"
    r"(?: (?:subgroup|driver)=[0-9]+)?"
)


def string_list(values):
    if not isinstance(values, list) or any(not isinstance(value, str) or not value for value in values):
        raise ValueError("expected a list of nonempty strings")
    return values


def comparison(report):
    builds = report["builds"]
    commits = [builds[side]["commit"] for side in SIDES]
    if not all(isinstance(commit, str) and re.fullmatch(r"[0-9a-f]{40}", commit) for commit in commits):
        raise ValueError("expected full baseline and candidate commit IDs")
    names = [sorted(string_list([test["name"] for test in builds[side]["tests"]])) for side in SIDES]
    if not names[0] or names[0] != names[1] or len(set(names[0])) != len(names[0]):
        raise ValueError("expected matching nonempty test-name sets")
    options = report["options"]
    pattern, targets = options["tests"], sorted(set(string_list(options["targets"])))
    if not isinstance(pattern, str) or not targets:
        raise ValueError("expected a test pattern and build targets")
    return {"baseline_commit": commits[0], "candidate_commit": commits[1],
            "test_pattern": pattern, "test_names": names[0], "targets": targets,
            "cmake_args": string_list(options["cmake_args"]), "runtime": sorted(string_list(options["runtime"]))}


def number(value):
    return int(value, 16 if value.startswith("0x") else 10)


def observed_devices(root):
    if root.tag not in ("testsuite", "testsuites"):
        raise ValueError("expected JUnit testsuite or testsuites")
    observations = {}
    for case in root.iter("testcase"):
        name = case.get("name")
        if not name or name in observations:
            raise ValueError("missing or duplicate testcase name")
        devices = observations[name] = set()
        for channel in ("system-out", "system-err"):
            for line in (case.findtext(channel) or "").splitlines():
                match = DEVICE.fullmatch(line)
                if match:
                    device, vendor, product = match.groups()
                    devices.add((device, number(vendor), number(product)))
    return observations


def device_rows(devices):
    return [{"name": name, "vendor_id": vendor, "device_id": product}
            for name, vendor, product in sorted(devices)]


def gpu_evidence(path, report, test_names):
    selector = report.get("environment", {}).get("ANYPS5_GPU")
    samples, issues, devices, seen = [], [], set(), set()
    repeat = report.get("options", {}).get("repeat")
    if type(repeat) is not int or repeat < 1:
        issues.append("missing or invalid configured repetition count")
    iterations = report.get("iterations", [])
    if not isinstance(iterations, list):
        raise ValueError("expected a list of saved gate iterations")
    for item in iterations:
        side, iteration = item["side"], item["iteration"]
        if side not in SIDES or type(iteration) is not int or iteration < 1 or (side, iteration) in seen:
            raise ValueError("invalid or duplicate iteration identity")
        seen.add((side, iteration))
        sample = {"side": side, "iteration": iteration}
        samples.append(sample)
        saved_tests, saved_problems = item.get("tests"), item.get("problems")
        if (type(item.get("returncode")) is not int or not isinstance(saved_tests, dict)
                or not isinstance(saved_problems, list)):
            sample["error"] = "invalid saved gate iteration fields"
            continue
        try:
            junit = path.parent / side / f"iteration-{iteration}.xml"
            xml = junit.read_bytes()
            observations = observed_devices(ET.fromstring(xml))
            tests, problems = gate.parse_report(BytesIO(xml), test_names)
        except (OSError, ValueError, LookupError, ET.ParseError, gate.GateError) as error:
            sample["error"] = str(error)
            continue
        if problems != saved_problems or json.dumps(tests, sort_keys=True) != json.dumps(saved_tests, sort_keys=True):
            sample["error"] = "retained JUnit results differ from the gate report"
            continue
        sample["missing_tests"] = sorted(test for test in test_names if not observations.get(test))
        sample["unexpected_tests"] = sorted(set(observations) - set(test_names))
        sample["observed_gpus"] = {
            test: device_rows(found) for test, found in sorted(observations.items()) if found
        }
        devices.update(device for found in observations.values() for device in found)
    if type(repeat) is int and repeat > 0:
        for side in SIDES:
            indices = [index for recorded_side, index in seen if recorded_side == side]
            if len(indices) != repeat or max(indices, default=0) > repeat:
                issues.append(f"{side}: expected {repeat} retained iterations, found {len(indices)}")
    if report.get("status") == "complete" and not any("error" in sample for sample in samples):
        summary = gate.compare(iterations)
        if any(report.get(key) != summary[key] for key in ("exit_code", "new_diagnostics", "problems")):
            issues.append("saved gate summary differs from retained iterations")
    if not isinstance(selector, str) or not selector:
        issues.append("missing requested GPU selector")
    elif any(selector.lower() not in device[0].lower() for device in devices):
        issues.append("an observed GPU name does not match the requested selector")
    if len(devices) > 1:
        issues.append("the run records more than one observed GPU identity")
    complete = bool(devices) and not issues and all(
        "error" not in sample and not sample["missing_tests"] and not sample["unexpected_tests"] for sample in samples)
    return {"requested_gpu": selector, "observed_gpus": device_rows(devices),
            "gpu_evidence": "consistent" if complete else "incomplete",
            "gpu_issues": issues, "gpu_samples": samples}


def read_run(path):
    run = {"report": str(path), "gate_status": None, "gate_exit_code": None,
           "requested_gpu": None, "observed_gpus": [], "gpu_evidence": "incomplete"}
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(report, dict):
            raise ValueError("expected a gate report object")
        run.update(gate_status=report.get("status"), gate_exit_code=report.get("exit_code"),
                   gate_error=report.get("error"), gate_problems=report.get("problems"),
                   new_diagnostics=report.get("new_diagnostics"))
        selector = report.get("environment", {}).get("ANYPS5_GPU")
        if isinstance(selector, str):
            run["requested_gpu"] = selector
        comparability = comparison(report)
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        run["input_error"] = str(error)
        return None, run
    try:
        run.update(gpu_evidence(path, report, comparability["test_names"]))
    except (ValueError, TypeError, KeyError, AttributeError) as error:
        run["input_error"] = str(error)
    return comparability, run


def aggregate(paths, expected):
    groups, ungrouped = {}, []
    for path in paths:
        factors, run = read_run(path)
        if factors is None:
            ungrouped.append(run)
        else:
            key = json.dumps(factors, sort_keys=True)
            groups.setdefault(key, {"comparison": factors, "runs": []})["runs"].append(run)
    missing = bool(ungrouped)
    failed = any(gate_failed(run) for run in ungrouped)
    for group in groups.values():
        runs = group["runs"]
        observed_selectors = {run["requested_gpu"] for run in runs if run["gpu_evidence"] == "consistent"}
        group["missing_gpu_selectors"] = sorted(set(expected) - observed_selectors)
        group["gate_failed"] = any(gate_failed(run) for run in runs)
        group["incomplete"] = bool(group["missing_gpu_selectors"]) or any(
            run["gpu_evidence"] != "consistent" or not (gate_failed(run) or gate_clean(run)) for run in runs)
        missing |= group["incomplete"]
        failed |= group["gate_failed"]
    exit_code = 1 if failed else 2 if missing else 0
    return {"version": 1, "exit_code": exit_code, "gate_failed": failed, "incomplete": missing,
            "expected_gpu_selectors": sorted(set(expected)), "groups": list(groups.values()), "ungrouped": ungrouped,
            "limits": "Selection lines in retained JUnit report model and PCI IDs. They do not identify a physical unit, "
                      "verify the driver or authenticate the artifacts. Gate results apply to each run independently."}


def gate_failed(run):
    return run["gate_status"] == "complete" and type(run["gate_exit_code"]) is int and run["gate_exit_code"] == 1


def gate_clean(run):
    return (run["gate_status"] == "complete" and type(run["gate_exit_code"]) is int and run["gate_exit_code"] == 0
            and run["gate_problems"] == [] and run["new_diagnostics"] == {})


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Summarize saved Vulkan validation gate reports and retained GPU evidence as JSON",
        epilog="Exit 0 for complete evidence with no gate failures, 1 if any gate failed, or 2 for incomplete evidence.")
    parser.add_argument("report", nargs="+", type=Path, help="gate report.json, with retained JUnit files alongside it")
    parser.add_argument("--expect-gpu", action="append", default=[], metavar="SELECTOR",
                        help="require evidence for this exact requested selector in each comparison; repeat as needed")
    args = parser.parse_args(argv)
    report = aggregate(args.report, args.expect_gpu)
    print(json.dumps(report, indent=2))
    return report["exit_code"]


if __name__ == "__main__":
    raise SystemExit(main())
