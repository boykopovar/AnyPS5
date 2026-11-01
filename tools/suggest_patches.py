import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import tomllib
from collections import Counter
from datetime import date
from pathlib import Path, PurePosixPath, PureWindowsPath
from urllib.parse import urlsplit


SHA256 = re.compile(r"[0-9a-fA-F]{64}\Z")
VERIFY_SUMMARY = re.compile(r"^Verified (\d+) patches: (\d+) ok, (\d+) not-checked, (\d+) mismatch$", re.MULTILINE)
PATCH_ID = re.compile(r"[a-z0-9][a-z0-9_-]*\Z")
TEST_DATE = re.compile(r"\d{4}-\d{2}-\d{2}\Z")
ENTRY_FIELDS = {"titleId", "contentVersion", "patch", "targets", "id", "name", "category", "description", "tested", "notes", "contributors", "source", "revision"}
CATEGORIES = {"compatibility", "fps", "graphics", "memory", "other"}
TEST_STAGES = {"verification", "conversion", "launch", "gameplay"}
TEST_RESULTS = {"passed", "failed", "partial"}


def relative_path(name):
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError(f"invalid relative path: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or PureWindowsPath(name).drive or any(part in (".", "..") for part in name.split("/")):
        raise ValueError(f"invalid relative path: {name!r}")
    return Path(*path.parts)


def confined(root, name):
    path = relative_path(name)
    try:
        resolved = (root / path).resolve()
    except RuntimeError as error:
        raise ValueError(f"invalid relative path: {name!r}: {error}") from error
    if not resolved.is_relative_to(root.resolve()) or not resolved.is_file():
        raise ValueError(f"file missing or outside directory: {name}")
    return resolved


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command_line(arguments):
    return subprocess.list2cmdline(arguments) if os.name == "nt" else shlex.join(arguments)


def conversion_options(args, patch):
    options = ["--patches", str(patch)]
    if args.skip_sce_module:
        options.append("--skip-sce-module")
    if args.sce_module_path:
        options.extend(("--sce-module-path", str(args.sce_module_path)))
    for module_dir in args.module_dir:
        options.extend(("--module-dir", module_dir))
    for executable in args.executable:
        options.extend(("--executable", executable))
    return command_line(options)


def read_catalog(directory, object_pairs_hook=None):
    data = json.loads(confined(directory, "index.json").read_text(encoding="utf-8"), object_pairs_hook=object_pairs_hook)
    if not isinstance(data, dict) or set(data) != {"format", "entries"} or type(data["format"]) is not int or data["format"] != 1 or not isinstance(data["entries"], list):
        raise ValueError("catalog must have format 1 and an entries array")
    return data["entries"]


def load_catalog(directory, title, version):
    entries = []
    for number, entry in enumerate(read_catalog(directory), 1):
        if not isinstance(entry, dict) or (entry.get("titleId"), entry.get("contentVersion")) != (title, version):
            continue
        entries.append((number, entry))
    return entries


def single_line(value, field):
    if not isinstance(value, str) or not value.strip() or not value.isprintable():
        raise ValueError(f"{field} must be nonempty single-line text")
    return value


def validate_metadata(entry):
    metadata = {key: entry[key] for key in ENTRY_FIELDS - {"titleId", "contentVersion", "patch", "targets"} if key in entry}
    if "id" in metadata and (not isinstance(metadata["id"], str) or not PATCH_ID.fullmatch(metadata["id"])):
        raise ValueError("id must match [a-z0-9][a-z0-9_-]*")
    for field in ("name", "description", "notes"):
        if field in metadata:
            single_line(metadata[field], field)
    if "category" in metadata and (not isinstance(metadata["category"], str) or metadata["category"] not in CATEGORIES):
        raise ValueError("invalid category")
    if "source" in metadata:
        source = single_line(metadata["source"], "source")
        url = urlsplit(source)
        if url.scheme != "https" or not url.hostname or any(character.isspace() for character in source):
            raise ValueError("source must be an HTTPS URL")
    if "revision" in metadata and (type(metadata["revision"]) is not int or metadata["revision"] <= 0):
        raise ValueError("revision must be a positive integer")
    if "contributors" in metadata:
        contributors = metadata["contributors"]
        if not isinstance(contributors, list) or not contributors:
            raise ValueError("contributors must be a nonempty array")
        for contributor in contributors:
            single_line(contributor, "contributor")
        if len({contributor.casefold() for contributor in contributors}) != len(contributors):
            raise ValueError("duplicate contributor")
    if "tested" in metadata:
        records = metadata["tested"]
        if not isinstance(records, list) or not records:
            raise ValueError("tested must be a nonempty array")
        for record in records:
            if not isinstance(record, dict) or not {"stage", "result", "evidence"} <= set(record) or set(record) - {"stage", "result", "evidence", "environment", "date"}:
                raise ValueError("invalid tested record fields")
            if not isinstance(record["stage"], str) or record["stage"] not in TEST_STAGES or not isinstance(record["result"], str) or record["result"] not in TEST_RESULTS:
                raise ValueError("invalid tested stage or result")
            single_line(record["evidence"], "tested evidence")
            if "environment" in record:
                single_line(record["environment"], "tested environment")
            if "date" in record:
                value = record["date"]
                if not isinstance(value, str) or not TEST_DATE.fullmatch(value):
                    raise ValueError("tested date must be YYYY-MM-DD")
                try:
                    date.fromisoformat(value)
                except ValueError as error:
                    raise ValueError("tested date must be a valid calendar date") from error
    return metadata


def print_metadata(metadata):
    for field, label in (("id", "ID"), ("name", "Name"), ("category", "Category"),
                         ("description", "Description"), ("revision", "Revision"),
                         ("notes", "Notes"), ("source", "Source")):
        if field in metadata:
            print(f"  {label}: {metadata[field]}")
    for contributor in metadata.get("contributors", []):
        print(f"  Contributor: {contributor}")
    for record in metadata.get("tested", []):
        detail = f"{record['stage']} {record['result']}"
        if "environment" in record:
            detail += f" on {record['environment']}"
        if "date" in record:
            detail += f" ({record['date']})"
        print(f"  Tested: {detail}: {record['evidence']}")


def validate_entry(directory, entry):
    if not {"titleId", "contentVersion", "patch", "targets"} <= set(entry) or set(entry) - ENTRY_FIELDS:
        raise ValueError("invalid fields")
    metadata = validate_metadata(entry)
    patch = confined(directory, entry["patch"])
    targets = entry["targets"]
    if not isinstance(targets, dict) or not targets:
        raise ValueError("targets must be a nonempty object")
    if any(not isinstance(name, str) or not isinstance(digest, str) or not SHA256.fullmatch(digest) for name, digest in targets.items()):
        raise ValueError("invalid target digest")
    parsed = tomllib.loads(patch.read_text(encoding="utf-8"))
    patches = parsed.get("patch")
    if not isinstance(patches, list) or not patches or any(not isinstance(item, dict) or not isinstance(item.get("target"), str) for item in patches):
        raise ValueError("patch file has no valid targets")
    if set(targets) != {item["target"] for item in patches}:
        raise ValueError("target hashes must cover exactly the patch targets")
    return patch, {name: digest.lower() for name, digest in targets.items()}, metadata


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Recommend a local patch only after metadata, hashes and relinker verification match")
    parser.add_argument("--catalog", required=True, type=Path, help="directory containing index.json and patch files")
    parser.add_argument("--param-json", required=True, type=Path, help="title metadata file")
    parser.add_argument("--input", required=True, type=Path, help="unwrapped main ELF")
    parser.add_argument("--relinker", required=True, type=Path, help="relinker executable to run")
    parser.add_argument("--sce-module-path", type=Path, help="parent directory of bundled modules")
    parser.add_argument("--module-dir", action="append", default=[], help="module directory relative to the module parent; repeatable")
    parser.add_argument("--executable", action="append", default=[], help="additional title executable relative to the module parent; repeatable")
    parser.add_argument("--skip-sce-module", action="store_true", help="skip bundled modules during verification")
    parser.add_argument("--timeout-seconds", type=int, default=120, help="maximum seconds per relinker verification (default: 120)")
    parser.add_argument("--list-verified", action="store_true", help="list every verified catalog entry without choosing one")
    args = parser.parse_args(argv)
    prefix = "No verified catalog entries" if args.list_verified else "No recommendation"
    try:
        metadata = json.loads(args.param_json.read_text(encoding="utf-8"))
        if not isinstance(metadata, dict):
            raise ValueError("param.json must be an object")
        title = metadata.get("titleId")
        version = metadata.get("contentVersion")
        if not isinstance(title, str) or not title or not isinstance(version, str) or not version:
            raise ValueError("param.json needs nonempty titleId and contentVersion")
        if not args.input.is_file() or not args.relinker.is_file():
            raise ValueError("input and relinker must be files")
        if args.skip_sce_module and args.sce_module_path:
            raise ValueError("--skip-sce-module conflicts with --sce-module-path")
        if args.timeout_seconds <= 0:
            raise ValueError("timeout-seconds must be positive")
        root = (args.sce_module_path or args.input.parent).resolve()
        if not root.is_dir():
            raise ValueError("module parent must be a directory")
        for executable in args.executable:
            confined(root, executable)
        matches = []
        entries = load_catalog(args.catalog.resolve(), title, version)
        if not entries:
            print(f"{prefix}: no catalog entry for {title} {version}.")
            return 1
        relinker_errors = False
        for number, entry in entries:
            label = f"Catalog entry {number} ({entry.get('patch')!r})"
            try:
                patch, targets, catalog_metadata = validate_entry(args.catalog.resolve(), entry)
            except (OSError, ValueError, tomllib.TOMLDecodeError) as error:
                print(f"{label}: rejected: {error}")
                continue
            errors = []
            for name, expected in targets.items():
                try:
                    source = args.input.resolve() if name == args.input.name else confined(root, name)
                    actual = sha256(source)
                    if actual != expected:
                        errors.append(f"{name}: SHA-256 mismatch ({actual})")
                except (OSError, ValueError) as error:
                    errors.append(f"{name}: {error}")
            if errors:
                print(f"{label}: rejected: {'; '.join(errors)}")
                continue
            command = [str(args.relinker.resolve()), "--verify-patches", str(patch)]
            if args.skip_sce_module:
                command.append("--skip-sce-module")
            if args.sce_module_path:
                command.extend(("--sce-module-path", str(args.sce_module_path)))
            for module_dir in args.module_dir:
                command.extend(("--module-dir", module_dir))
            for executable in args.executable:
                command.extend(("--executable", executable))
            command.append(str(args.input))
            try:
                result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace",
                                        timeout=args.timeout_seconds)
            except subprocess.TimeoutExpired:
                print(f"{label}: rejected: relinker verification exceeded {args.timeout_seconds} seconds")
                continue
            summary = VERIFY_SUMMARY.search(result.stdout)
            verified = tuple(map(int, summary.groups())) if summary else None
            if result.returncode:
                detail = (result.stdout + result.stderr).strip()
                if result.returncode == 1 and verified and verified[3] > 0:
                    print(f"{label}: rejected by relinker: {detail}")
                    continue
                if result.returncode == 2:
                    print(f"{label}: relinker error (exit 2): {detail}")
                    relinker_errors = True
                    continue
                raise ValueError(f"{label}: relinker error (exit {result.returncode}): {detail}")
            if not verified:
                raise ValueError(f"{label}: relinker produced no verification summary")
            if verified[0] == 0 or verified[1] != verified[0] or verified[2] or verified[3]:
                print(f"{label}: rejected: relinker did not confirm every patch")
                continue
            matches.append((number, entry, patch, targets, catalog_metadata))
        id_counts = Counter(catalog_metadata["id"] for _, _, _, _, catalog_metadata in matches if "id" in catalog_metadata)
        if any(count > 1 for count in id_counts.values()):
            unique = []
            for match in matches:
                number, entry, patch, targets, catalog_metadata = match
                identifier = catalog_metadata.get("id")
                if identifier is not None and id_counts[identifier] > 1:
                    print(f"Catalog entry {number} ({entry['patch']!r}): rejected: duplicate verified id {identifier!r}")
                else:
                    unique.append(match)
            matches = unique
        if args.list_verified and matches:
            print(f"Verified catalog entries: {len(matches)}")
            print("Choose one patch file; listed entries are not combined.")
            for number, entry, patch, targets, catalog_metadata in matches:
                print(f"Catalog entry {number}: {patch}")
                print_metadata(catalog_metadata)
                for name, digest in targets.items():
                    print(f"  {name} SHA-256 {digest}")
                print(f"Use these options when converting:\n  {conversion_options(args, patch)}")
            return 0
        if len(matches) == 1:
            number, entry, patch, targets, catalog_metadata = matches[0]
            print(f"Recommended: {patch} (catalog entry {number})")
            print_metadata(catalog_metadata)
            for name, digest in targets.items():
                print(f"  {name} SHA-256 {digest}")
            print(f"Use these options when converting:\n  {conversion_options(args, patch)}")
            return 0
        if len(matches) > 1:
            print("No recommendation: multiple verified catalog entries match; use --list-verified to see them.")
            return 1
        if relinker_errors:
            print(f"{prefix}: relinker errors occurred; see catalog entries above.", file=sys.stderr)
            return 2
        print(f"{prefix}: no catalog entry passed all checks.")
        return 1
    except (OSError, ValueError, json.JSONDecodeError, tomllib.TOMLDecodeError) as error:
        print(f"Patch advisor: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
