import argparse
import hashlib
import json
import os
import stat
import sys
from pathlib import Path


def describe(path):
    initial = path.lstat()
    if not stat.S_ISREG(initial.st_mode):
        raise ValueError(f"corpus entry is not a regular file: {path}")
    digest = hashlib.sha256()
    size, selector = 0, None
    with path.open("rb") as source:
        before = os.fstat(source.fileno())
        if not stat.S_ISREG(before.st_mode) or not os.path.samestat(initial, before):
            raise ValueError(f"corpus entry changed before reading: {path}")
        while size < before.st_size:
            block = source.read(min(1024 * 1024, before.st_size - size))
            if not block:
                break
            if size == 0:
                selector = block[0]
            digest.update(block)
            size += len(block)
        after = os.fstat(source.fileno())
    if (size != before.st_size or (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns)
            or (os.name != "nt" and before.st_ctime_ns != after.st_ctime_ns)
            or not os.path.samestat(before, path.lstat())):
        raise ValueError(f"corpus entry changed while reading: {path}")
    return {"name": path.name, "selector": selector, "size": size, "sha256": digest.hexdigest()}


def main(argv=None):
    parser = argparse.ArgumentParser(description="Describe the files of a flat relinker fuzz corpus as JSON")
    parser.add_argument("corpus", type=Path)
    args = parser.parse_args(argv)
    try:
        seeds = [describe(path) for path in sorted(args.corpus.iterdir(), key=lambda path: path.name)]
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps({"seeds": seeds}, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
