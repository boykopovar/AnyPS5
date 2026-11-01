import argparse
import json
import sys
import tomllib
from pathlib import Path

import suggest_patches


def unique_json_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def validate(directory):
    try:
        entries = suggest_patches.read_catalog(directory, object_pairs_hook=unique_json_keys)
    except RecursionError as error:
        raise ValueError("index.json is nested too deeply") from error
    seen_ids = {}
    errors = 0
    for number, entry in enumerate(entries, 1):
        patch_name = entry.get("patch") if isinstance(entry, dict) else None
        label = f"Catalog entry {number} ({patch_name!r})" if isinstance(patch_name, str) else f"Catalog entry {number}"
        try:
            if not isinstance(entry, dict):
                raise ValueError("entry must be an object")
            for field in ("titleId", "contentVersion"):
                suggest_patches.single_line(entry.get(field), field)
            patch, targets, metadata = suggest_patches.validate_entry(directory, entry)
            patch_data = tomllib.loads(patch.read_text(encoding="utf-8"))
            if type(patch_data.get("format")) is not int or patch_data["format"] != 1:
                raise ValueError("patch file must have format 1")
            for name in targets:
                suggest_patches.relative_path(name)
            identifier = metadata.get("id")
            if identifier is not None:
                key = (entry["titleId"], entry["contentVersion"], identifier)
                for previous, previous_targets in seen_ids.get(key, []):
                    if not any(name in previous_targets and previous_targets[name] != digest for name, digest in targets.items()):
                        raise ValueError(f"duplicate id {identifier!r} for {key[0]} {key[1]} may match entry {previous}")
                seen_ids.setdefault(key, []).append((number, targets))
        except (OSError, ValueError, tomllib.TOMLDecodeError) as error:
            print(f"{label}: invalid: {error}", file=sys.stderr)
            errors += 1
        except RecursionError:
            print(f"{label}: invalid: entry or patch file is nested too deeply", file=sys.stderr)
            errors += 1
    if errors:
        print(f"Catalog validation: {errors} invalid of {len(entries)} entries.", file=sys.stderr)
        return 2
    print(f"Catalog validation: {len(entries)} entries valid.")
    return 0


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Validate every entry in a local patch catalog without game files")
    parser.add_argument("--catalog", required=True, type=Path, help="directory containing index.json and patch files")
    args = parser.parse_args(argv)
    try:
        try:
            catalog = args.catalog.resolve()
        except RuntimeError as error:
            raise ValueError(f"catalog path cannot be resolved: {error}") from error
        return validate(catalog)
    except (OSError, ValueError, json.JSONDecodeError, tomllib.TOMLDecodeError) as error:
        print(f"Catalog validation: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
