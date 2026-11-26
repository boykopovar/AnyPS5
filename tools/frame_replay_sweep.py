import argparse
from collections import Counter
from html import escape
from pathlib import Path
import re
import sys

from frame_replay_compare import ARMS, LIMITS, _environment, _offline_report


def _staged_inputs(report, directory):
    prefix = directory + "/"
    return sorted((entry["path"][len(prefix):], entry["bytes"], entry["sha256"])
                  for entry in report["artifacts"] if entry["path"].startswith(prefix))


def _arm_settings(recipe, name):
    arms = recipe.get("arms")
    arm = arms.get(name) if isinstance(arms, dict) else None
    if not isinstance(arm, dict):
        raise ValueError(f"missing {name} arm recipe")
    if not isinstance(arm.get("commit"), str) or not re.fullmatch(r"[0-9a-f]{40}", arm["commit"]):
        raise ValueError(f"invalid {name} runtime commit")
    result = {key: arm.get(key) for key in ("commit", "replay", "inspect", "library_dirs")}
    shared = _environment(recipe.get("environment", {}), shared=True)
    result["environment"] = {**shared, **_environment(arm.get("environment", {}))}
    return result


def _trial_status(run):
    if run["timed_out"]:
        return "timeout"
    if run["log_limit"]:
        return "log limit"
    if run["returncode"] is None:
        return "exit unavailable"
    if run["returncode"]:
        return f"exit {run['returncode']}"
    return "evidence issue" if run["assessment"]["issues"] else "healthy"


def _markdown_cell(value):
    text = escape(str(value), quote=False)
    return re.sub(r"([\\|`\[\]])", r"\\\1", text)


def _sweep_markdown(cases):
    capture = None
    baseline = None
    baseline_commit = None
    rows = []
    for label, bundle in cases:
        try:
            report = _offline_report(bundle)
            recipe = report["recipe"]
            if recipe.get("format") != "aps5-frame-replay-recipe/1":
                raise ValueError("unsupported saved recipe format")
            case_capture = _staged_inputs(report, "capture")
            baseline_runtime = _staged_inputs(report, "runtime/baseline")
            if not case_capture or not baseline_runtime:
                raise ValueError("bundle has no staged capture or baseline runtime")
            baseline_settings = _arm_settings(recipe, "baseline")
            candidate_settings = _arm_settings(recipe, "candidate")
            limits = tuple(recipe.get(key, default) for key, (_, _, default) in LIMITS.items())
            case_baseline = (baseline_runtime, baseline_settings, limits)
            if capture is None:
                capture, baseline = case_capture, case_baseline
                baseline_commit = baseline_settings["commit"]
            elif case_capture != capture:
                raise ValueError("staged capture differs from the first case")
            elif case_baseline != baseline:
                raise ValueError("staged baseline or baseline settings differ from the first case")
            candidate_runtime = _staged_inputs(report, "runtime/candidate")
            runtime = "absent"
            if candidate_runtime:
                runtime = "same" if candidate_runtime == baseline_runtime else "different"
            before, after = baseline_settings["environment"], candidate_settings["environment"]
            changed = ", ".join(key for key in sorted(before.keys() | after.keys()) if before.get(key) != after.get(key))
            counts = []
            for arm in ARMS:
                observed = report["observations"][arm]
                counts.append(f"{observed['healthy_trials']} / {observed['attempted_trials']}")
            outcomes = Counter(_trial_status(run) for run in report["runs"]
                               if run["phase"] == "trial" and run["arm"] == "candidate")
            outcome = ", ".join(f"{status} ({count})" for status, count in sorted(outcomes.items()))
            state = "evidence issues" if report.get("issues") else "complete"
            if not report["complete"]:
                state = "incomplete"
            rows.append((label, candidate_settings["commit"][:12], runtime, changed or "none recorded",
                         *counts, outcome or "none recorded", state))
        except (OSError, ValueError) as error:
            raise ValueError(f"{label}: {error}") from error

    lines = ["# Frame replay sweep", "",
             f"The cases share the same staged capture and baseline recipe at `{baseline_commit}`.", "",
             "| Case | Candidate commit | Candidate runtime bytes vs. baseline | Changed environment keys | Baseline healthy / attempted | Candidate healthy / attempted | Candidate trial outcomes | Comparison state |",
             "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    lines.extend("| " + " | ".join(map(_markdown_cell, row)) + " |" for row in rows)
    lines.extend(["", "The individual reports retain the trial logs and observed host and GPU context."])
    return "\n".join(lines) + "\n"


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(
        description="Summarize sealed frame replay comparison bundles as a Markdown table.",
        epilog="Supply bundles with the same staged capture and baseline recipe. "
               "Changed environment keys appear without their values.")
    parser.add_argument("--case", nargs=2, metavar=("LABEL", "BUNDLE"), action="append", required=True,
                        help="label a comparison bundle, once for each case")
    args = parser.parse_args(argv)
    labels = [label for label, _ in args.case]
    if (len(labels) < 2 or len(set(labels)) != len(labels)
            or any(not label.strip() or not label.isprintable() for label in labels)):
        parser.error("supply at least two cases with distinct, printable labels")
    try:
        cases = [(label, Path(bundle).absolute()) for label, bundle in args.case]
        if len({bundle.resolve() for _, bundle in cases}) != len(cases):
            parser.error("supply distinct comparison bundles")
        sys.stdout.write(_sweep_markdown(cases))
    except (OSError, ValueError) as error:
        print(f"frame_replay_sweep: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
