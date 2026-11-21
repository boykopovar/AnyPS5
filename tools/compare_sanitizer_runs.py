import argparse
import html
import json
import re
import sys
from pathlib import Path

OUTCOMES = {"passed", "skipped", "failed"}
GROUPS = (
    ("new_unexpected", "New unexpected failures"),
    ("persistent_unexpected", "Persistent unexpected failures"),
    ("known_failures", "Current known failures"),
    ("recovered", "Previously failing tests now passed"),
    ("skipped_failures", "Previously failing tests now skipped"),
    ("missing_failures", "Previously failing tests absent from this run"),
)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise ValueError(f"invalid JSON constant: {value}")


def load_report(path):
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_object,
                          parse_constant=reject_constant)
        if not isinstance(data, dict) or not isinstance(data.get("title"), str):
            raise ValueError("expected a sanitizer_report.py --json object with a title")
        if type(data.get("unexpected_failure")) is not bool:
            raise ValueError("unexpected_failure must be a boolean")
        if not isinstance(data.get("tests"), list) or not data["tests"]:
            raise ValueError("tests must be a nonempty list")
        tests = {}
        for test in data["tests"]:
            if not isinstance(test, dict) or not isinstance(test.get("name"), str) or not test["name"].strip():
                raise ValueError("each test must have a nonempty name")
            name = test["name"]
            if name in tests:
                raise ValueError(f"test is listed more than once: {name}")
            if not isinstance(test.get("outcome"), str) or test["outcome"] not in OUTCOMES:
                raise ValueError(f"{name}: outcome must be passed, skipped or failed")
            for field in ("known_reason", "report"):
                if field not in test or (test[field] is not None and not isinstance(test[field], str)):
                    raise ValueError(f"{name}: {field} must be a string or null")
            if test["known_reason"] is not None and not test["known_reason"].strip():
                raise ValueError(f"{name}: known_reason cannot be empty")
            expected = test["outcome"] == "failed" and test["known_reason"] is None
            if type(test.get("unexpected")) is not bool or test["unexpected"] != expected:
                raise ValueError(f"{name}: unexpected disagrees with outcome and known_reason")
            tests[name] = test
        if data["unexpected_failure"] != any(test["unexpected"] for test in tests.values()):
            raise ValueError("unexpected_failure disagrees with test outcomes")
        return data["title"], tests
    except (ValueError, RuntimeError) as error:
        raise ValueError(f"{path}: {error}") from None


def compare(baseline, current):
    previous_title, before = baseline
    current_title, after = current
    groups = {key: [] for key, _ in GROUPS}
    for name in sorted(set(before) | set(after)):
        old, new = before.get(name), after.get(name)
        group = None
        if new and new["unexpected"]:
            group = "persistent_unexpected" if old and old["unexpected"] else "new_unexpected"
        elif new and new["outcome"] == "failed":
            group = "known_failures"
        elif old and old["outcome"] == "failed":
            group = "missing_failures" if new is None else "recovered" if new["outcome"] == "passed" else "skipped_failures"
        if group:
            context = new if new and new["outcome"] == "failed" else old
            groups[group].append({"name": name, "before": old["outcome"] if old else None,
                                  "after": new["outcome"] if new else None,
                                  "known_reason": context["known_reason"], "report": context["report"]})
    return {"format": 1, "baseline": previous_title, "current": current_title,
            "new_unexpected_failure": bool(groups["new_unexpected"]), **groups}


def cell(value):
    text = html.escape(" ".join(value.split()), quote=False)
    return re.sub(r"([\\`*_{}\[\]()#+.!|>-])", r"\\\1", text)


def markdown(comparison):
    lines = ["### Sanitizer comparison", "",
             f"{cell(comparison['baseline'])} to {cell(comparison['current'])}.", "",
             "; ".join(f"{len(comparison[key])} {label.lower()}" for key, label in GROUPS) + ".", ""]
    for key, label in GROUPS:
        if not comparison[key]:
            continue
        lines += [f"#### {label}", "", "| Test | Before | After | Details |", "| - | - | - | - |"]
        for test in comparison[key]:
            details = test["known_reason"] if test["known_reason"] is not None else test["report"] or ""
            lines.append("| " + " | ".join(cell(value) for value in (test["name"], test["before"] or "absent",
                                                                   test["after"] or "absent", details)) + " |")
        lines += [""]
    return "\n".join(lines)


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare two sanitizer_report.py --json reports.")
    parser.add_argument("baseline", type=Path, help="JSON report of the earlier run")
    parser.add_argument("current", type=Path, help="JSON report of the current run")
    parser.add_argument("--json", action="store_true", help="write a structured comparison instead of Markdown")
    args = parser.parse_args(argv)
    try:
        comparison = compare(load_report(args.baseline), load_report(args.current))
    except (OSError, ValueError, RuntimeError) as error:
        print(f"compare_sanitizer_runs: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(json.dumps(comparison, ensure_ascii=False, allow_nan=False, indent=2) + "\n" if args.json else markdown(comparison))
    return 1 if comparison["new_unexpected_failure"] else 0


if __name__ == "__main__":
    sys.exit(main())
