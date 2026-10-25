import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import tomllib
from pathlib import Path, PurePosixPath


SHA256 = re.compile(r"[0-9a-fA-F]{64}\Z")
VERIFY_SUMMARY = re.compile(r"^Verified (\d+) patches: (\d+) ok, (\d+) not-checked, (\d+) mismatch$", re.MULTILINE)


def confined(root, name):
    if not isinstance(name, str) or not name or "\\" in name:
        raise ValueError(f"invalid relative path: {name!r}")
    path = PurePosixPath(name)
    if path.is_absolute() or any(part in (".", "..") for part in name.split("/")):
        raise ValueError(f"invalid relative path: {name!r}")
    resolved = (root / Path(*path.parts)).resolve()
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


def load_catalog(directory, title, version):
    data = json.loads(confined(directory, "index.json").read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"format", "entries"} or type(data["format"]) is not int or data["format"] != 1 or not isinstance(data["entries"], list):
        raise ValueError("catalog must have format 1 and an entries array")
    entries = []
    for number, entry in enumerate(data["entries"], 1):
        if not isinstance(entry, dict) or (entry.get("titleId"), entry.get("contentVersion")) != (title, version):
            continue
        entries.append((number, entry))
    return entries


def validate_entry(directory, entry):
    if set(entry) != {"titleId", "contentVersion", "patch", "targets"}:
        raise ValueError("invalid fields")
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
    return patch, {name: digest.lower() for name, digest in targets.items()}


def main(argv=None):
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
    args = parser.parse_args(argv)
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
            print(f"No recommendation: no catalog entry for {title} {version}.")
            return 1
        relinker_errors = False
        for number, entry in entries:
            label = f"Catalog entry {number} ({entry.get('patch')!r})"
            try:
                patch, targets = validate_entry(args.catalog.resolve(), entry)
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
                result = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout_seconds)
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
            matches.append((number, entry, patch, targets))
        if len(matches) == 1:
            number, entry, patch, targets = matches[0]
            print(f"Recommended: {patch} (catalog entry {number})")
            for name, digest in targets.items():
                print(f"  {name} SHA-256 {digest}")
            conversion = ["--patches", str(patch)]
            if args.skip_sce_module:
                conversion.append("--skip-sce-module")
            if args.sce_module_path:
                conversion.extend(("--sce-module-path", str(args.sce_module_path)))
            for module_dir in args.module_dir:
                conversion.extend(("--module-dir", module_dir))
            for executable in args.executable:
                conversion.extend(("--executable", executable))
            print(f"Use these options when converting:\n  {command_line(conversion)}")
            return 0
        if len(matches) > 1:
            print("No recommendation: multiple verified catalog entries match.")
            return 1
        if relinker_errors:
            print("No recommendation: relinker errors occurred; see catalog entries above.", file=sys.stderr)
            return 2
        print("No recommendation: no catalog entry passed all checks.")
        return 1
    except (OSError, ValueError, json.JSONDecodeError, tomllib.TOMLDecodeError) as error:
        print(f"Patch advisor: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
