import argparse
import json
import os
import re
import stat
from pathlib import Path

BODY_LIMIT = 125000


def release_body(notes):
    if len(notes) <= BODY_LIMIT:
        return notes
    return re.sub(
        r"^## What's Changed[^\S\r\n]*\r?\n.*?(?=^#{1,2} |^\*\*Full Changelog\*\*:|\Z)",
        "",
        notes,
        count=1,
        flags=re.MULTILINE | re.DOTALL,
    )


def prepare_notes(source, body, full):
    notes = json.loads(source.read_text(encoding="utf-8"))["body"]
    if not isinstance(notes, str):
        raise ValueError("Generated release notes body must be a string")
    for destination in (body, full):
        if destination.is_dir():
            raise IsADirectoryError(str(destination))
        if os.name == "nt" and destination.exists() and destination.stat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
            raise PermissionError(f"Release note destination is read-only: {destination}")
    full.parent.mkdir(parents=True, exist_ok=True)
    full.write_text(notes, encoding="utf-8", newline="")
    body.write_text(release_body(notes), encoding="utf-8", newline="")


def main():
    parser = argparse.ArgumentParser(description="Prepare size-safe GitHub release notes")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--body", type=Path, required=True)
    parser.add_argument("--full", type=Path, required=True)
    args = parser.parse_args()
    prepare_notes(args.input, args.body, args.full)


if __name__ == "__main__":
    main()
