import argparse
import hashlib
import json
import os
import stat
import sys
from pathlib import Path

from package_headers import public_headers, resolved_path


def input_stamp(info):
    changed = info.st_ctime_ns if os.name == "posix" else None
    return info.st_size, info.st_mtime_ns, changed


def hash_input(path):
    if not stat.S_ISREG(path.stat().st_mode):
        raise ValueError(f"{path}: header package input must be a regular file")
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as source:
        before = os.fstat(source.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise ValueError(f"{path}: header package input must be a regular file")
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
        after = os.fstat(source.fileno())
    if size != before.st_size or input_stamp(before) != input_stamp(after):
        raise ValueError(f"{path}: header package input changed while hashing")
    return size, digest.hexdigest()


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Inventory the public header package inputs and content hashes without creating archives")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve()
        libs = resolved_path(root / "core/libs")
        headers = public_headers(root)
        license_file = root / "LICENSE"
        if not license_file.is_file() or license_file.stat().st_size == 0:
            raise ValueError("Missing project license")
        inputs = [(path, "include/anyps5/" + path.relative_to(libs).as_posix()) for path in headers]
        inputs.append((license_file, "LICENSE"))
        entries = []
        for path, destination in sorted(inputs, key=lambda entry: entry[1]):
            size, digest = hash_input(path)
            if destination == "LICENSE" and size == 0:
                raise ValueError("Missing project license")
            entries.append({"path": destination, "size": size, "sha256": digest})
    except (OSError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    print(json.dumps({"headers": len(headers), "entries": entries}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
