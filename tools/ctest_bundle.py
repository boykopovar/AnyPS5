import argparse
import datetime
import hashlib
import json
import os
import re
import stat
import subprocess
import sys
import tempfile
from pathlib import Path


FORMAT = "anyps5-ctest-bundle/1"
MAX_REPORT_BYTES = 32 * 1024 * 1024
MAX_MANIFEST_BYTES = 64 * 1024


def source_identity(root):
    root = Path(root).resolve()
    result = {"directory": root.as_posix(), "head": None, "tree": None,
              "tracked_dirty": None, "error": None}
    try:
        for key, revision in (("head", "HEAD"), ("tree", "HEAD^{tree}")):
            command = subprocess.run(
                ["git", "-C", str(root), "rev-parse", "--verify", revision],
                capture_output=True, text=True, timeout=10, check=True)
            value = command.stdout.strip()
            if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", value):
                raise ValueError("invalid Git object identity")
            result[key] = value
        command = subprocess.run(
            ["git", "-C", str(root), "diff", "--quiet", "HEAD", "--"],
            capture_output=True, timeout=10)
        if command.returncode not in (0, 1):
            raise ValueError("Git tracked-state lookup failed")
        result["tracked_dirty"] = command.returncode == 1
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        result["error"] = type(error).__name__
    return result


def write_manifest(directory, manifest):
    data = (json.dumps(manifest, indent=2, sort_keys=True, allow_nan=False) + "\n").encode("utf-8")
    if len(data) > MAX_MANIFEST_BYTES:
        raise ValueError("manifest exceeds size limit")
    with tempfile.NamedTemporaryFile(dir=directory, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    try:
        os.replace(temporary, Path(directory) / "manifest.json")
    finally:
        temporary.unlink(missing_ok=True)


def prepare(directory, environ=None, github_output=None):
    environ = os.environ if environ is None else environ
    directory = Path(os.path.abspath(directory))
    if any(part.is_symlink() or (part.exists() and
           getattr(part.stat(), "st_file_attributes", 0) & 0x400)
           for part in (directory, *directory.parents)):
        raise ValueError("bundle directory cannot contain links or reparse points")
    if any(character in directory.as_posix() for character in "\r\n"):
        raise ValueError("bundle directory cannot contain line breaks")
    if github_output is not None:
        output = Path(github_output)
        if (output.resolve() == directory.resolve() or directory.resolve() in output.resolve().parents or
                any(part.is_symlink() or (part.exists() and
                    getattr(part.stat(), "st_file_attributes", 0) & 0x400)
                    for part in (output, *output.absolute().parents)) or
                (output.exists() and (not output.is_file() or output.stat().st_nlink != 1))):
            raise ValueError("GitHub output must be a separate regular file")
    values = {}
    fields = {
        "ci": {"repository": "GITHUB_REPOSITORY", "workflow": "GITHUB_WORKFLOW",
               "workflow_ref": "GITHUB_WORKFLOW_REF", "workflow_sha": "GITHUB_WORKFLOW_SHA",
               "run_id": "GITHUB_RUN_ID", "run_attempt": "GITHUB_RUN_ATTEMPT",
               "job": "GITHUB_JOB", "event_name": "GITHUB_EVENT_NAME", "event_sha": "GITHUB_SHA"},
        "runner": {"os": "RUNNER_OS", "arch": "RUNNER_ARCH", "name": "RUNNER_NAME",
                   "image_os": "ImageOS", "image_version": "ImageVersion"},
        "toolchain": {"winlibs_release": "WINLIBS_RELEASE", "winlibs_zip": "WINLIBS_ZIP",
                      "winlibs_sha256": "WINLIBS_SHA256"},
        "test": {"selection": "CTEST_SELECTION", "repeat": "CTEST_REPEAT",
                 "build_directory": "CTEST_BUILD_DIRECTORY", "require_vulkan": "CTEST_REQUIRE_VULKAN",
                 "base_sha": "CTEST_BASE_SHA", "pr_heads": "CTEST_PR_HEADS",
                 "pr_numbers": "CTEST_PR_NUMBERS"}}
    for section, names in fields.items():
        values[section] = {}
        for name, variable in names.items():
            value = environ.get(variable) or None
            limit = 8192 if variable in ("CTEST_PR_HEADS", "CTEST_PR_NUMBERS") else 1024
            if value is not None and (len(value) > limit or any(c in value for c in "\r\n\x00")):
                raise ValueError("invalid metadata field: " + variable)
            values[section][name] = value
    test = values["test"]
    for field in ("pr_heads", "pr_numbers"):
        test[field] = test[field].split() if test[field] is not None else []
    if (len(test["pr_heads"]) != len(test["pr_numbers"]) or
            any(not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", value)
                for value in test["pr_heads"] + ([test["base_sha"]] if test["base_sha"] else [])) or
            any(not re.fullmatch(r"[1-9][0-9]*", value) for value in test["pr_numbers"])):
        raise ValueError("invalid pinned pull request identities")
    matrix_text = environ.get("CTEST_MATRIX_JSON") or "null"
    if len(matrix_text.encode("utf-8")) > 8192:
        raise ValueError("matrix exceeds size limit")
    matrix = json.loads(matrix_text)
    if matrix is not None and not isinstance(matrix, dict):
        raise ValueError("matrix must be an object or null")
    matrix_key = json.dumps([values["ci"], matrix], sort_keys=True, allow_nan=False).encode("utf-8")
    suffix = hashlib.sha256(matrix_key).hexdigest()[:16]
    name = "-".join(str(values["ci"][key] or "local") for key in ("run_id", "run_attempt", "job"))
    artifact_name = "ctest-" + re.sub(r"[^A-Za-z0-9_-]", "-", name)[:100] + "-" + suffix
    values["toolchain"]["python"] = sys.version.split()[0]
    values["toolchain"]["winlibs_identifiers_are_configured"] = True
    if values["runner"]["os"] != "Windows":
        for field in ("winlibs_release", "winlibs_zip", "winlibs_sha256"):
            values["toolchain"][field] = None
    test.update(step_outcome=None, exit_code=None)
    manifest = dict(values, format=FORMAT, phase="prepared", matrix=matrix,
                    directory=directory.as_posix(), artifact_name=artifact_name,
                    prepared_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    source=source_identity(Path.cwd()), reports={})
    directory.mkdir(parents=True, exist_ok=False)
    write_manifest(directory, manifest)
    if github_output is not None:
        with open(github_output, "a", encoding="utf-8", newline="\n") as stream:
            stream.write("directory=" + directory.as_posix() + "\n")
            stream.write("artifact_directory=" + (directory / "published").as_posix() + "\n")
            stream.write("artifact_name=" + artifact_name + "\n")
    return manifest


def report_file(directory, filename, limit=MAX_REPORT_BYTES, destination=None):
    if filename not in ("ctest.xml", "ctest.log"):
        raise ValueError("unsupported report filename")
    path = Path(directory) / filename
    result = {"path": filename, "state": "missing", "bytes": None, "sha256": None, "reason": None}
    try:
        info = path.lstat()
    except FileNotFoundError:
        return result
    result["bytes"] = info.st_size
    if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or
            getattr(info, "st_file_attributes", 0) & 0x400):
        result.update(state="omitted", reason="not a regular unlinked file")
        return result
    if info.st_size > limit:
        result.update(state="omitted", reason="size limit exceeded")
        return result
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_NOFOLLOW", 0))
    with os.fdopen(descriptor, "rb") as stream:
        opened = os.fstat(stream.fileno())
        if (opened.st_dev, opened.st_ino, opened.st_size) != (info.st_dev, info.st_ino, info.st_size):
            raise ValueError("report changed while opening")
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
    if len(data) != info.st_size or after.st_mtime_ns != info.st_mtime_ns:
        raise ValueError("report changed while collecting")
    if destination is not None:
        (Path(destination) / filename).write_bytes(data)
    result.update(state="available_unvalidated", sha256=hashlib.sha256(data).hexdigest())
    return result


def finish(directory, outcome):
    if outcome not in ("success", "failure", "skipped", "cancelled"):
        raise ValueError("invalid test step outcome")
    directory = Path(os.path.abspath(directory))
    if any(part.is_symlink() or (part.exists() and
           getattr(part.stat(), "st_file_attributes", 0) & 0x400)
           for part in (directory, *directory.parents)):
        raise ValueError("bundle directory cannot contain links or reparse points")
    path = directory / "manifest.json"
    info = path.lstat()
    if (not stat.S_ISREG(info.st_mode) or info.st_nlink != 1 or
            getattr(info, "st_file_attributes", 0) & 0x400):
        raise ValueError("manifest must be a regular unlinked file")
    with path.open("rb") as stream:
        data = stream.read(MAX_MANIFEST_BYTES + 1)
    if len(data) > MAX_MANIFEST_BYTES:
        raise ValueError("manifest exceeds size limit")
    manifest = json.loads(data)
    if (not isinstance(manifest, dict) or manifest.get("format") != FORMAT or manifest.get("phase") != "prepared" or
            manifest.get("directory") != directory.as_posix()):
        raise ValueError("invalid prepared bundle")
    source = manifest["source"]
    if source["directory"] != Path.cwd().resolve().as_posix():
        raise ValueError("collection must use the preparation source directory")
    current = source_identity(Path.cwd())
    manifest["source_at_collection"] = current
    manifest["source_matches_preparation"] = None
    if source.get("error") is None and current["error"] is None:
        if source != current:
            manifest["source_matches_preparation"] = False
        elif not source["tracked_dirty"]:
            manifest["source_matches_preparation"] = True
    published = directory / "published"
    published.mkdir(exist_ok=False)
    manifest["reports"] = {
        "junit": report_file(directory, "ctest.xml", destination=published),
        "log": report_file(directory, "ctest.log", destination=published)}
    manifest["test"]["step_outcome"] = outcome
    manifest["phase"] = "collected"
    manifest["collected_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    write_manifest(published, manifest)
    return manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description="Publish raw CTest reports with pinned CI provenance.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    preparation = subparsers.add_parser("prepare")
    preparation.add_argument("--directory", required=True, type=Path)
    preparation.add_argument("--github-output", type=Path)
    collection = subparsers.add_parser("finish")
    collection.add_argument("--directory", required=True, type=Path)
    collection.add_argument("--outcome", required=True, choices=("success", "failure", "skipped", "cancelled"))
    args = parser.parse_args(argv)
    try:
        if args.command == "prepare":
            manifest = prepare(args.directory, github_output=args.github_output)
        else:
            manifest = finish(args.directory, args.outcome)
        print(json.dumps(manifest, sort_keys=True, allow_nan=False))
    except (OSError, ValueError, KeyError, TypeError, RecursionError) as error:
        print("CTest bundle: " + str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
