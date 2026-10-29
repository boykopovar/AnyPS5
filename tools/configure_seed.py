import argparse
import fnmatch
import os
import re
import secrets
import stat
import sys
from pathlib import Path

CHECK = re.compile(r"^//(Have (include|includes|function|symbol|library) |Test |Result of (TRY_COMPILE|TRY_RUN|TEST_BIG_ENDIAN|check_)|CHECK_TYPE_SIZE: )")
ENTRY = re.compile(r"^([^#/:][^:]*):([A-Z]+)=(.*)$")


def checks(cache):
    found, doc = {}, []
    for line in Path(cache).read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("//"):
            doc.append(line)
            continue
        match = ENTRY.match(line)
        if match and match.group(2) == "INTERNAL" and doc and CHECK.match(doc[0]):
            help_text = "".join("\n" + line[4:] if line.startswith("//\\n") else line[2:] for line in doc)
            found[match.group(1)] = (match.group(3), help_text)
        doc = []
    architecture_sizes = {name for name, (value, doc) in found.items()
                          if value == "0" and doc.startswith("CHECK_TYPE_SIZE: ")}
    if architecture_sizes:
        return {name: entry for name, entry in found.items()
                if name not in architecture_sizes and not entry[1].startswith("Result of TRY_COMPILE")}
    return found


def quote(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$") + '"'


def write_seed(path, text):
    destination = path.resolve()
    metadata = destination.stat() if destination.exists() else None
    if metadata is not None and os.name == "nt" and metadata.st_file_attributes & stat.FILE_ATTRIBUTE_READONLY:
        raise PermissionError(f"Seed output is read-only: {path}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        candidate = destination.parent / (".configure-seed-" + secrets.token_hex(16))
        with candidate.open("x", encoding="utf-8", newline="\n") as output:
            temporary = candidate
            if metadata is not None:
                temporary.chmod(stat.S_IMODE(metadata.st_mode))
            output.write(text)
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


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
        write_seed(seed, "".join(f"set({quote(name)} {quote(value)} CACHE INTERNAL {quote(doc)})\n"
                                 for name, (value, doc) in sorted(found.items())))
    except OSError as error:
        parser.error(f"{seed}: {error}")
    print(f"{len(found)} check results")
    return 0


if __name__ == "__main__":
    sys.exit(main())
