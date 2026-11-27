import argparse
import json
import os
import re
import secrets
import stat
import sys
from pathlib import Path

BODY_LIMIT = 125000


def resolve_output_path(path):
    path = Path(path)
    try:
        resolved = Path(os.path.realpath(path))
    except OSError as error:
        raise ValueError(str(error)) from error
    if resolved.is_symlink():
        try:
            resolved.stat()
        except OSError as error:
            raise ValueError(f"{path}: release note output is a symbolic link loop: {error}") from error
        raise ValueError(f"{path}: release note output is a symbolic link loop")
    return resolved


def release_body(notes, limit=BODY_LIMIT):
    if len(notes) <= limit:
        return notes
    section = None
    fence = ""
    for line in re.finditer(r"[^\r\n]*(?:\r\n|\r|\n|$)", notes):
        content = line.group().rstrip("\r\n")
        marker = re.match(r" {0,3}(`{3,}|~{3,})(.*)", content)
        if marker:
            run, info = marker.groups()
            if fence:
                if run.startswith(fence) and not info.strip(" \t"):
                    fence = ""
            elif run[0] == "~" or "`" not in info:
                fence = run
        elif not fence:
            if section is None:
                if content.rstrip() == "## What's Changed":
                    section = line.start()
            elif content.startswith(("# ", "## ", "**Full Changelog**:")):
                return notes[:section] + notes[line.start():]
    return notes[:section]


def unique_release_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Generated release notes response has a duplicate JSON key {key!r}")
        result[key] = value
    return result


def prepare_notes(source, body, full, check=False, *, marker=None):
    paths = (source, body, full)
    for index, first in enumerate(paths):
        for second in paths[index + 1:]:
            if (os.path.normcase(os.path.realpath(first)) == os.path.normcase(os.path.realpath(second)) or
                    (first.exists() and second.exists() and first.samefile(second))):
                raise ValueError("Release note input and outputs must be distinct files")
    if not stat.S_ISREG(source.stat().st_mode):
        raise ValueError(f"{source}: release note input must be a regular file")
    try:
        payload = json.loads(source.read_text(encoding="utf-8-sig"), object_pairs_hook=unique_release_object)
    except RecursionError as error:
        raise ValueError("Generated release notes response is too deeply nested") from error
    if not isinstance(payload, dict) or "body" not in payload:
        raise ValueError("Generated release notes response must be a JSON object with a body field")
    notes = payload["body"]
    if not isinstance(notes, str):
        raise ValueError("Generated release notes body must be a string")
    suffix = f"\n{marker}\n" if marker is not None else ""
    body_text = release_body(notes, BODY_LIMIT - len(suffix)) + suffix
    if len(body_text) > BODY_LIMIT:
        raise ValueError(f"Release notes body exceeds {BODY_LIMIT} characters after removing What's Changed")
    outputs = ((body, body_text.encode("utf-8")), (full, notes.encode("utf-8")))
    if check:
        try:
            for destination, expected in outputs:
                info = destination.stat()
                if not stat.S_ISREG(info.st_mode):
                    raise ValueError(f"{destination}: release note output must be a regular file")
                if info.st_size != len(expected):
                    return False
                with destination.open("rb") as output:
                    if output.read(len(expected) + 1) != expected:
                        return False
            return True
        except FileNotFoundError:
            return False
    destinations = []
    for destination, expected in outputs:
        target = resolve_output_path(destination)
        destinations.append((target, expected))
        try:
            info = target.stat()
        except FileNotFoundError:
            continue
        if stat.S_ISDIR(info.st_mode):
            raise IsADirectoryError(str(destination))
        if not stat.S_ISREG(info.st_mode):
            raise ValueError(f"{destination}: release note output must be a regular file")
        if os.name == "nt" and info.st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
            raise PermissionError(f"Release note destination is read-only: {destination}")
    staged = []
    try:
        for target, expected in reversed(destinations):
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.parent / (".release-notes-" + secrets.token_hex(16))
            with temporary.open("xb") as output:
                staged.append((temporary, target))
                if target.exists():
                    temporary.chmod(stat.S_IMODE(target.stat().st_mode))
                output.write(expected)
        promoted = []
        backed_up = []
        try:
            for temporary, target in staged:
                if target.exists():
                    backup = target.parent / (".release-notes-" + secrets.token_hex(16))
                    os.replace(target, backup)
                    backed_up.append((target, backup))
                os.replace(temporary, target)
                promoted.append(target)
        except OSError as promotion_error:
            rollback_errors = []
            for target in reversed(promoted):
                try:
                    target.unlink(missing_ok=True)
                except OSError as error:
                    rollback_errors.append(error)
            for target, backup in reversed(backed_up):
                try:
                    os.replace(backup, target)
                except OSError as error:
                    rollback_errors.append(error)
            if rollback_errors:
                details = "; ".join(str(error) for error in rollback_errors)
                raise RuntimeError(f"Release note promotion failed: {promotion_error}; rollback failed: {details}") from promotion_error
            raise
        for _, backup in backed_up:
            backup.unlink(missing_ok=True)
    finally:
        for temporary, _ in staged:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Prepare size-safe GitHub release notes")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--body", type=Path, required=True)
    parser.add_argument("--full", type=Path, required=True)
    parser.add_argument("--marker", help="append a workflow marker within the release body limit")
    parser.add_argument("--check", action="store_true", help="fail with status 1 when outputs differ; never create or modify files")
    args = parser.parse_args(argv)
    try:
        matched = prepare_notes(args.input, args.body, args.full, args.check, marker=args.marker)
        if args.check and not matched:
            print("Release note outputs are missing or stale", file=sys.stderr)
            return 1
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    sys.exit(main())
