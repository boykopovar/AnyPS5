import argparse
import json
import sys
from pathlib import Path

from nid_names import compute_nid
from stubgen import parse_db


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Audit the hash consistency of a local NID/name database using the existing stub generator's first-record policy")
    parser.add_argument("database", type=Path)
    args = parser.parse_args(argv)
    try:
        database = parse_db(args.database)
        entries = [{"nid": nid, "name": name, "computed_nid": compute_nid(name), "matches": nid == compute_nid(name)}
                   for nid, name in sorted(database.items())]
    except (OSError, ValueError) as error:
        parser.error(str(error))
    mismatches = sum(not item["matches"] for item in entries)
    print(json.dumps({"entries": entries, "matching": len(entries) - mismatches, "mismatching": mismatches}, ensure_ascii=False, indent=2))
    return int(mismatches != 0)


if __name__ == "__main__":
    sys.exit(main())
