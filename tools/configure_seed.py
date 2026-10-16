import argparse
import re
import sys
from pathlib import Path

CHECK = re.compile(r"^//(Have (include|includes|function|symbol|library) |Test |Result of (TRY_COMPILE|TRY_RUN|TEST_BIG_ENDIAN|check_))")
ENTRY = re.compile(r"^([^#/:][^:]*):([A-Z]+)=(.*)$")


def checks(cache):
    found, doc = {}, ""
    for line in Path(cache).read_text(encoding="utf-8").splitlines():
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


def main():
    parser = argparse.ArgumentParser(description="Write the configure check results of a CMake cache as an initial cache file")
    parser.add_argument("cache", help="CMakeCache.txt of a configured tree")
    parser.add_argument("seed", help="file to pass to cmake -C")
    args = parser.parse_args()
    found = checks(args.cache)
    Path(args.seed).write_text("".join(f"set({name} {quote(value)} CACHE INTERNAL {quote(doc)})\n"
                                       for name, (value, doc) in sorted(found.items())), encoding="utf-8")
    print(f"{len(found)} check results")
    return 0


if __name__ == "__main__":
    sys.exit(main())
