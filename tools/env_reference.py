import argparse
import re
import sys
from pathlib import Path

PREFIXES = ("APS5_", "ANYPS5_")
DOC = "docs/dev/ENVIRONMENT.md"
ROOTS = ("core",)
EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".py"}
SKIP = {"build", "build-linux", "build-relinker", "3rdparty", ".git"}

READERS = {
    "IsEnvironmentSet": "presence",
    "kibSetting": "integer",
    "MaskFromEnvironment": "integer",
}
NULL_TEST = re.compile(r"[!=]=\s*(nullptr|NULL|0)\b|!\s*(?:std::)?getenv\(")
INTEGER_TEST = re.compile(r"\b(atoi|strtol|strtoul|strtoll|strtoull|stoi|stol|stoul|from_chars|atoi64)\s*\(")


def pattern(prefixes):
    name = "(?:" + "|".join(re.escape(prefix) for prefix in prefixes) + ")[A-Za-z0-9_]+"
    return re.compile(
        r"(?P<getenv>(?:std::)?getenv)\(\s*\"(?P<getenv_name>" + name + r")\"\s*\)"
        r"|IsEnvironmentSet\(\s*\"(?P<presence_name>" + name + r")\"\s*\)"
        r"|MaskFromEnvironment\(\s*\"(?P<mask_name>" + name + r")\"\s*\)"
        r"|kibSetting\(\s*\"(?P<kib_name>" + name + r")\"\s*,\s*(?P<kib_default>[0-9]+)"
        r"|EnvInt\(\s*\"(?P<envint_name>" + name + r")\"\s*,\s*(?P<envint_default>-?[0-9]+)"
        r"|GetEnvironmentVariableA\(\s*\"(?P<win_name>" + name + r")\""
    )


def documented_pattern(prefixes):
    name = "(?:" + "|".join(re.escape(prefix) for prefix in prefixes) + ")[A-Za-z0-9_]+"
    return re.compile(r"`(" + name + r")`")


def kind_of(reader, window):
    if reader == "IsEnvironmentSet":
        return "presence"
    if reader in ("kibSetting", "MaskFromEnvironment", "EnvInt"):
        return "integer"
    if reader == "GetEnvironmentVariableA":
        return "string"
    if NULL_TEST.search(window):
        return "presence"
    if INTEGER_TEST.search(window):
        return "integer"
    return "string"


def default_of(match, window):
    for group in ("kib_default", "envint_default"):
        if match.group(group):
            return match.group(group)
    found = re.search(r"\?[\s\S]*?:\s*([0-9]+)", window)
    return found.group(1) if found else ""


def source_files(root):
    root = Path(root)
    if not root.is_dir():
        return
    for path in sorted(root.rglob("*")):
        if path.suffix not in EXTENSIONS or any(part in SKIP for part in path.parts):
            continue
        yield path


def reads(roots, prefixes=PREFIXES):
    read = pattern(prefixes)
    found = {}
    for root in roots:
        for path in source_files(root):
            try:
                text = path.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError):
                continue
            lines = text.splitlines()
            for number, line in enumerate(lines, 1):
                if not read.search(line):
                    continue
                window = "\n".join(lines[number - 1:number + 2])
                for match in read.finditer(line):
                    name = (match.group("getenv_name") or match.group("presence_name") or
                            match.group("mask_name") or match.group("kib_name") or
                            match.group("envint_name") or match.group("win_name"))
                    if match.group("getenv"):
                        reader = "getenv"
                    elif match.group("presence_name"):
                        reader = "IsEnvironmentSet"
                    elif match.group("mask_name"):
                        reader = "MaskFromEnvironment"
                    elif match.group("envint_name"):
                        reader = "EnvInt"
                    elif match.group("win_name"):
                        reader = "GetEnvironmentVariableA"
                    else:
                        reader = "kibSetting"
                    found.setdefault(name, []).append(
                        (path.as_posix(), number, kind_of(reader, window), default_of(match, window)))
    for sites in found.values():
        sites.sort()
    return found


def documented(path, prefixes=PREFIXES):
    try:
        return set(documented_pattern(prefixes).findall(Path(path).read_text(encoding="utf-8")))
    except (OSError, UnicodeDecodeError):
        return None


def library_of(source):
    parts = Path(source).parts
    if len(parts) >= 4 and parts[0] == "core" and parts[1] == "libs" and parts[2] == "prx":
        return parts[3]
    if len(parts) >= 2 and parts[0] == "core":
        return "core/" + parts[1]
    return parts[0] if parts else source


def list_names(found, out):
    for name in sorted(found):
        for source, number, kind, default in found[name]:
            out.write(f"{name}\t{kind}\t{source}:{number}\t{default}\n")


def table(found, out):
    libraries = {}
    for name, sites in found.items():
        libraries.setdefault(library_of(sites[0][0]), []).append((name, sites[0]))
    for library in sorted(libraries):
        out.write(f"### {library}\n\n")
        out.write("| Name | Kind | Source | Default | Description |\n")
        out.write("|------|------|--------|---------|-------------|\n")
        for name, (source, number, kind, default) in sorted(libraries[library]):
            out.write(f"| `{name}` | {kind} | `{source}` | {default} | |\n")
        out.write("\n")


def check(found, doc, prefixes, out):
    names = documented(doc, prefixes)
    if names is None:
        out.write(f"error: cannot read {doc}\n")
        return 1
    missing = sorted(set(found) - names)
    stale = sorted(names - set(found))
    for name in missing:
        source, number, _, _ = found[name][0]
        out.write(f"error: {name} is read at {source}:{number} but has no row in {doc}\n")
    for name in stale:
        out.write(f"error: {name} has a row in {doc} but the code no longer reads it\n")
    if missing or stale:
        out.write(f"{len(missing)} name(s) undocumented, {len(stale)} stale\n")
        return 1
    out.write(f"{len(found)} name(s), all documented\n")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="list the environment names the code reads, or check them against a reference document")
    parser.add_argument("--check", action="store_true",
                        help="compare the names the code reads with the document and fail on a mismatch")
    parser.add_argument("--table", action="store_true",
                        help="emit the reference table rows, grouped by library")
    parser.add_argument("--doc", default=DOC, help="reference document to check (default: %(default)s)")
    parser.add_argument("--root", action="append", default=None,
                        help="directory to scan (default: %s); repeatable" % ", ".join(ROOTS))
    parser.add_argument("--prefix", action="append", default=None,
                        help="name prefix to scan (default: %s); repeatable" % ", ".join(PREFIXES))
    args = parser.parse_args(argv)
    roots = args.root if args.root else ROOTS
    prefixes = tuple(args.prefix) if args.prefix else PREFIXES
    found = reads(roots, prefixes)
    if args.check:
        return check(found, args.doc, prefixes, sys.stdout)
    if args.table:
        table(found, sys.stdout)
        return 0
    list_names(found, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
