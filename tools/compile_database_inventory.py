import argparse
import json
import sys
from pathlib import Path

from check_compile import compilation_database


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Group the raw records of a saved compilation database without invoking a compiler or build tool")
    parser.add_argument("database", type=Path)
    args = parser.parse_args(argv)
    try:
        entries = compilation_database(args.database.read_text(encoding="utf-8-sig"), args.database)
        units = {}
        for item in entries:
            record = {"arguments": item["arguments"]} if "arguments" in item else {"command": item["command"]}
            if "output" in item:
                record["output"] = item["output"]
            units.setdefault((item["directory"], item["file"]), []).append(record)
    except (OSError, ValueError, RecursionError) as error:
        parser.error(str(error))
    print(json.dumps({"entries": len(entries), "units": [
        {"directory": directory, "file": file, "variants": len(records), "records": records}
        for (directory, file), records in sorted(units.items())]}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
