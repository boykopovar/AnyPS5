import argparse
import hashlib
import json
import sys
from pathlib import Path

from package_headers import public_headers, resolved_path


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
            data = path.read_bytes()
            entries.append({"path": destination, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    except (OSError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    print(json.dumps({"headers": len(headers), "entries": entries}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
