import argparse
import json
import sys
from pathlib import Path

import progress


def comparison_owners(groups):
    direct = {name: group["shared_sources"] for name, group in groups.items() if "shared_sources" in group}
    terminal = {}
    for name in direct:
        path = set()
        owner = name
        while owner in direct and owner not in terminal:
            if owner in path:
                raise ValueError(f"shared-source cycle at {owner!r}")
            path.add(owner)
            owner = direct[owner]
        owner = terminal.get(owner, owner)
        for wrapper in path:
            terminal[wrapper] = owner
    return {name: owner for name, owner in terminal.items() if owner in groups}


def compare(base, head):
    result = {}
    states = ("done", "todo", "inert")
    for section in ("libraries", "shaders"):
        before = {group["name"] for group in base[section]["groups"]}
        after = {group["name"]: group for group in head[section]["groups"]}
        owners = comparison_owners(after)
        groups = {}
        for name in sorted(before | after.keys()):
            presence = "added" if name not in before else "removed" if name not in after else "both"
            groups[name] = {"name": name, "presence": presence}
            for state in states:
                groups[name][f"{state}_names"] = {"added": [], "removed": []}
        for state in states:
            old = progress.names(base[section], state, owners)
            new = progress.names(head[section], state, owners)
            for action, changes in (("added", new - old), ("removed", old - new)):
                for name, symbol in sorted(changes):
                    groups[name][f"{state}_names"][action].append(symbol)
        result[section] = {"baseline_percent": base[section]["percent"], "candidate_percent": head[section]["percent"],
                           "change_percentage_points": round(head[section]["percent"] - base[section]["percent"], 2),
                           "groups": list(groups.values())}
    return result


def unique_json_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def read_snapshot(path):
    try:
        snapshot = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_json_keys)
        progress.validate_snapshot(snapshot)
    except (ValueError, RecursionError) as error:
        raise ValueError(f"{path}: {error}") from error
    return snapshot


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare progress snapshot symbol sets as JSON; coverage is not game compatibility")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(argv)
    try:
        snapshots = [read_snapshot(path) for path in (args.baseline, args.candidate)]
        text = json.dumps(compare(*snapshots), ensure_ascii=False, allow_nan=False, indent=2)
    except (OSError, ValueError, RecursionError) as error:
        parser.error(str(error))
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
