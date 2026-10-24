import argparse
import fnmatch
import re
import sys
from pathlib import Path

CHECK = re.compile(r"^//(Have (include|includes|function|symbol|library) |Test |Result of (TRY_COMPILE|TRY_RUN|TEST_BIG_ENDIAN|check_))")
ENTRY = re.compile(r"^([^#/:][^:]*):([A-Z]+)=(.*)$")


def checks(cache):
    found, doc = {}, ""
    for line in Path(cache).read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("//"):
            doc = line
            continue
        match = ENTRY.match(line)
        if match and match.group(2) == "INTERNAL" and CHECK.match(doc):
            found[match.group(1)] = (match.group(3), doc[2:])
        doc = ""
    return found


def quote(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$") + '"'


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Write the configure check results of a CMake cache as an initial cache file")
    parser.add_argument("cache", help="CMakeCache.txt of a configured tree")
    parser.add_argument("seed", help="file to pass to cmake -C")
    parser.add_argument("--include", action="append", default=[], metavar="GLOB",
                        help="seed matching check names only; repeat to include several patterns")
    parser.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                        help="omit matching check names after inclusion; patterns are case-sensitive")
    args = parser.parse_args(argv)
    cache, seed = Path(args.cache), Path(args.seed)
    if cache.resolve() == seed.resolve() or (cache.exists() and seed.exists() and cache.samefile(seed)):
        parser.error("seed output would overwrite the input cache")
    try:
        results = checks(cache)
    except (OSError, UnicodeDecodeError) as error:
        parser.error(f"{cache}: {error}")
    found = {name: entry for name, entry in results.items()
             if (not args.include or any(fnmatch.fnmatchcase(name, pattern) for pattern in args.include))
             and not any(fnmatch.fnmatchcase(name, pattern) for pattern in args.exclude)}
    try:
        seed.parent.mkdir(parents=True, exist_ok=True)
        seed.write_text("".join(f"set({quote(name)} {quote(value)} CACHE INTERNAL {quote(doc)})\n"
                               for name, (value, doc) in sorted(found.items())), encoding="utf-8")
    except OSError as error:
        parser.error(f"{seed}: {error}")
    print(f"{len(found)} check results")
    return 0


if __name__ == "__main__":
    sys.exit(main())
