import argparse
import stat
import sys
from pathlib import Path

from configure_seed import checks, render


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Check an unfiltered configure seed against its source CMake cache without writing")
    parser.add_argument("cache", type=Path)
    parser.add_argument("seed", type=Path)
    args = parser.parse_args(argv)
    try:
        if not stat.S_ISREG(args.cache.stat().st_mode):
            parser.error(f"{args.cache}: cache must be a regular file")
        found = checks(args.cache)
    except UnicodeError as error:
        parser.error(f"{args.cache}: {error}")
    except OSError as error:
        parser.error(str(error))
    try:
        expected = render(found).encode("utf-8")
        try:
            if not stat.S_ISREG(args.seed.stat().st_mode):
                parser.error(f"{args.seed}: seed must be a regular file")
            with args.seed.open("rb") as seed:
                actual = seed.read(len(expected) + 1)
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
