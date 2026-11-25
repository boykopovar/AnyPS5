import argparse
import hashlib
import json
import re
import stat
import sys
import tarfile
import zipfile
import zlib
from pathlib import Path


RUNTIME_MEMBER = re.compile(r"libs/([^/\\\x00-\x1f]+\.(?:prx|dll|so(?:\.[0-9]+)*))", re.IGNORECASE)


def inventory(members, open_member, platform):
    files = {}
    names = set()
    for member, path, size, regular in members:
        match = RUNTIME_MEMBER.fullmatch(path)
        if not match:
            continue
        name = match[1]
        key = name.casefold() if platform == "windows" else name
        if key in names:
            raise ValueError(f"Duplicate runtime member: {path}")
        if not regular or size <= 0:
            raise ValueError(f"Expected a nonempty regular runtime member: {path}")
        names.add(key)
        with open_member(member) as source:
            digest = hashlib.sha256()
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        files[name] = digest.hexdigest()
    if not any(name.lower().endswith(".prx") for name in files):
        raise ValueError("Archive contains no libraries under libs/*.prx")
    return dict(sorted(files.items()))


def read_archive(path, platform):
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            members = ((entry, entry.orig_filename, entry.file_size,
                        not entry.is_dir() and entry.orig_filename == entry.filename
                        and stat.S_IFMT(entry.external_attr >> 16) in (0, stat.S_IFREG))
                       for entry in archive.infolist())
            return inventory(members, archive.open, platform)
    if path.name.endswith(".tar.gz"):
        with tarfile.open(path, "r:gz") as archive:
            members = ((entry, entry.name, entry.size, entry.isfile()) for entry in archive)
            return inventory(members, archive.extractfile, platform)
    raise ValueError("Expected a .zip or .tar.gz native runtime archive")


def main(argv=None):
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Write a JSON inventory of the runtime libraries in an AnyPS5 release archive")
    parser.add_argument("archive", type=Path, help="release ZIP or tar.gz containing libs/*.prx")
    parser.add_argument("--tag", required=True, help="release tag to record")
    parser.add_argument("--commit", required=True, help="full release commit SHA to record")
    parser.add_argument("--platform", required=True, choices=("linux", "windows"))
    args = parser.parse_args(argv)
    if not re.fullmatch(r"v[0-9A-Za-z][0-9A-Za-z._-]*", args.tag) or args.tag.endswith("."):
        parser.error("Invalid release tag")
    if not re.fullmatch(r"[0-9a-f]{40}", args.commit):
        parser.error("Release commit must be a full lowercase SHA")
    try:
        files = read_archive(args.archive, args.platform)
    except (OSError, ValueError, RuntimeError, EOFError, tarfile.TarError, zipfile.BadZipFile, zlib.error) as error:
        parser.error(str(error))
    print(json.dumps({"schemaVersion": 1, "tag": args.tag, "commit": args.commit, "platform": args.platform, "files": files}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
