import argparse
import json
import sys
from pathlib import Path

from configure_seed import checks


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Compare recognized configure checks in two CMake caches without writing")
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args(argv)
    try:
        before, after = checks(args.baseline), checks(args.candidate)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    def record(value):
        return {"value": value[0], "help": value[1]}
    added = [{"name": name, **record(after[name])} for name in sorted(after.keys() - before.keys())]
    removed = [{"name": name, **record(before[name])} for name in sorted(before.keys() - after.keys())]
    changed = [{"name": name, "before": record(before[name]), "after": record(after[name])}
               for name in sorted(before.keys() & after.keys()) if before[name] != after[name]]
    print(json.dumps({"added": added, "removed": removed, "changed": changed}, ensure_ascii=False, indent=2))
    return int(bool(added or removed or changed))


if __name__ == "__main__":
    sys.exit(main())
