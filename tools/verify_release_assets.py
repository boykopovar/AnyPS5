import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


def uploaded_assets(document):
    if not isinstance(document, dict) or not isinstance(document.get("assets"), list):
        raise ValueError("Expected a JSON object with an assets list")
    assets = {}
    for index, asset in enumerate(document["assets"]):
        if not isinstance(asset, dict) or not isinstance(asset.get("name"), str) or not asset["name"]:
            raise ValueError(f"Missing or invalid name for release asset at index {index}")
        name = asset["name"]
        if name in assets:
            raise ValueError(f"Duplicate release asset name: {name!r}")
        if asset.get("state") != "uploaded":
            raise ValueError(f"Release asset {name!r} is not uploaded (state: {asset.get('state')!r})")
        size = asset.get("size")
        if type(size) is not int or size < 0:
            raise ValueError(f"Release asset {name!r} has missing or invalid size")
        digest = asset.get("digest")
        if not isinstance(digest, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", digest):
            raise ValueError(f"Release asset {name!r} has missing or invalid SHA-256 digest")
        assets[name] = (size, digest)
    return assets


def file_metadata(path):
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as content:
        for chunk in iter(lambda: content.read(1024 * 1024), b""):
            size += len(chunk)
            digest.update(chunk)
    return size, "sha256:" + digest.hexdigest()


def verify(paths, document):
    files = {}
    for path in paths:
        if path.name in files:
            raise ValueError(f"Duplicate local asset name: {path.name!r}")
        if not path.is_file():
            raise ValueError(f"Local asset is not a file: {str(path)!r}")
        files[path.name] = path

    assets = uploaded_assets(document)
    mismatches = []
    for label, names in (("Missing", files.keys() - assets.keys()), ("Unexpected", assets.keys() - files.keys())):
        if names:
            mismatches.append(f"{label} assets: " + ", ".join(repr(name) for name in sorted(names)))
    for name in sorted(files.keys() & assets.keys()):
        local_size, local_digest = file_metadata(files[name])
        remote_size, remote_digest = assets[name]
        if remote_size != local_size:
            mismatches.append(f"{name!r}: size mismatch (local {local_size} bytes, uploaded {remote_size} bytes)")
        elif remote_digest != local_digest:
            mismatches.append(f"{name!r}: SHA-256 mismatch (local {local_digest}, uploaded {remote_digest})")
    if mismatches:
        raise ValueError("\n".join(mismatches))
    return len(files)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Verify local release files against gh release view --json assets on stdin.")
    parser.add_argument("files", type=Path, nargs="+", help="local packaged assets to verify")
    args = parser.parse_args(argv)
    try:
        count = verify(args.files, json.load(sys.stdin.buffer))
    except (OSError, RecursionError, ValueError) as error:
        print(f"Release asset verification failed: {error}", file=sys.stderr)
        return 1
    print(f"Verified {count} release assets by size and SHA-256.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
