import argparse
import json
import sys
from pathlib import Path

import progress


def compare(base, head):
    result = {}
    for section in ("libraries", "shaders"):
        before = {group["name"]: group for group in base[section]["groups"]}
        after = {group["name"]: group for group in head[section]["groups"]}
        groups = []
        for name in sorted(before.keys() | after.keys()):
            a, b = before.get(name), after.get(name)
            row = {"name": name, "presence": "added" if a is None else "removed" if b is None else "both"}
            for state in ("done_names", "todo_names", "inert_names"):
                old, new = set((a or {}).get(state, [])), set((b or {}).get(state, []))
                row[state] = {"added": sorted(new - old), "removed": sorted(old - new)}
            groups.append(row)
        result[section] = {"baseline_percent": base[section]["percent"], "candidate_percent": head[section]["percent"],
                           "change_percentage_points": head[section]["percent"] - base[section]["percent"], "groups": groups}
    return result


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare progress snapshot symbol sets as JSON; coverage is not game compatibility")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(argv)
    try:
        snapshots = [json.loads(path.read_text(encoding="utf-8-sig")) for path in (args.baseline, args.candidate)]
        for snapshot in snapshots:
            progress.validate_snapshot(snapshot)
        text = json.dumps(compare(*snapshots), ensure_ascii=False, allow_nan=False, indent=2)
    except (OSError, ValueError, RecursionError) as error:
        parser.error(str(error))
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
