import argparse
import json
import os
import re
import stat
import sys
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
    paths = (source, body, full)
    for index, first in enumerate(paths):
        for second in paths[index + 1:]:
            if first.resolve() == second.resolve() or (first.exists() and second.exists() and first.samefile(second)):
                raise ValueError("Release note input and outputs must be distinct files")
    payload = json.loads(source.read_text(encoding="utf-8-sig"))
    if not isinstance(payload, dict) or "body" not in payload:
        raise ValueError("Generated release notes response must be a JSON object with a body field")
    notes = payload["body"]
    if not isinstance(notes, str):
        raise ValueError("Generated release notes body must be a string")
    body_text = release_body(notes)
    if len(body_text) > BODY_LIMIT:
        raise ValueError(f"Release notes body exceeds {BODY_LIMIT} characters after removing What's Changed")
    for destination in (body, full):
        if destination.is_dir():
            raise IsADirectoryError(str(destination))
        if os.name == "nt" and destination.exists() and destination.stat().st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
            raise PermissionError(f"Release note destination is read-only: {destination}")
    body.parent.mkdir(parents=True, exist_ok=True)
    full.parent.mkdir(parents=True, exist_ok=True)
    full.write_bytes(notes.encode("utf-8"))
    body.write_bytes(body_text.encode("utf-8"))


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Prepare size-safe GitHub release notes")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--body", type=Path, required=True)
    parser.add_argument("--full", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        prepare_notes(args.input, args.body, args.full)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    main()
