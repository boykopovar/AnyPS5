import argparse
import sys
from pathlib import Path

from configure_seed import checks, quote


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Check an unfiltered configure seed against its source CMake cache without writing")
    parser.add_argument("cache", type=Path)
    parser.add_argument("seed", type=Path)
    args = parser.parse_args(argv)
    try:
        found = checks(args.cache)
        expected = "".join(f"set({quote(name)} {quote(value)} CACHE INTERNAL {quote(doc)})\n"
                           for name, (value, doc) in sorted(found.items())).encode("utf-8")
        try:
            actual = args.seed.read_bytes()
        except FileNotFoundError:
            print(f"missing seed: {args.seed}")
            return 1
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    if actual != expected:
        print(f"stale seed: {args.seed}")
        return 1
    print(f"{len(found)} check results match")
    return 0


if __name__ == "__main__":
    sys.exit(main())
