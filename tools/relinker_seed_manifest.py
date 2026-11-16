import argparse
import hashlib
import json
import sys
from pathlib import Path


def describe(path):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"corpus entry is not a regular file: {path}")
    digest = hashlib.sha256()
    size, selector = 0, None
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            if size == 0:
                selector = block[0]
            digest.update(block)
            size += len(block)
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
