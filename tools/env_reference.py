import argparse
import bisect
import functools
import posixpath
import re
import sys
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nid_names import CPP_TOKEN
from sanitizer_report import cell

PREFIXES = ("APS5_", "ANYPS5_")
DOC = "docs/dev/ENVIRONMENT.md"
ROOTS = ("core",)
EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".py"}
SKIP = {"build", "build-linux", "build-relinker", "3rdparty", ".git"}

INTEGER_READERS = {"kibSetting", "MaskFromEnvironment", "EnvInt", "configured", "environmentCount", "EnvironmentNumber"}
IDENTIFIER_READERS = ("getenv", "IsEnvironmentSet", "MaskFromEnvironment", "kibSetting", "EnvInt", "configured",
                      "PipelineSwitch", "environmentCount", "EnvironmentNumber")
STRING_READERS = {"GetEnvironmentVariableA", "GetEnvironmentVariableW", "addWideString"}
NULL_TEST = re.compile(r"[!=]=\s*(nullptr|NULL|0)\b|!\s*(?:std::)?_?w?getenv\s*\(")
INTEGER_TEST = re.compile(r"\b(atoi|strtol|strtoul|strtoll|strtoull|stoi|stol|stoul|from_chars|atoi64|_wtoi|_wtol|_wtoi64)\s*\(")
GREP = (r'(getenv|_wgetenv|wgetenv|IsEnvironmentSet|kibSetting|MaskFromEnvironment|EnvInt|GetEnvironmentVariable[AW]'
        r'|configured|PipelineSwitch|environmentCount|EnvironmentNumber|addWideString)'
        r'\([[:space:]]*(u|L)?"(APS5|ANYPS5)_[A-Za-z0-9_]+"')
GREP = GREP.replace(r"\(", r"[[:space:]]*\(")


def _name(prefixes):
    return "(?:" + "|".join(re.escape(prefix) for prefix in prefixes) + ")[A-Za-z0-9_]+"


@functools.lru_cache(maxsize=None)
def forms(prefixes=PREFIXES):
    name = _name(prefixes)
    readers = (
        ("getenv", re.compile(r'(?:std::)?getenv\(\s*"(?P<name>' + name + r')"\s*\)')),
        ("_wgetenv", re.compile(r'(?:std::)?_?wgetenv\(\s*L"(?P<name>' + name + r')"\s*\)')),
        ("IsEnvironmentSet", re.compile(r'IsEnvironmentSet\(\s*"(?P<name>' + name + r')"\s*\)')),
        ("MaskFromEnvironment", re.compile(r'MaskFromEnvironment\(\s*"(?P<name>' + name + r')"\s*\)')),
        ("kibSetting", re.compile(r'kibSetting\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>[0-9]+)')),
        ("EnvInt", re.compile(r'EnvInt\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>-?[0-9]+)')),
        ("configured", re.compile(r'\b(?:configured|PipelineSwitch)\(\s*"(?P<name>' + name + r')"'
                                  r'(?:\s*,\s*(?P<default>[0-9]+)[uUlL]*)?')),
        ("environmentCount", re.compile(r'\benvironmentCount\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>[0-9]+)')),
        ("EnvironmentNumber", re.compile(r'\bEnvironmentNumber\(\s*"(?P<name>' + name + r')"\s*\)')),
        ("GetEnvironmentVariableA", re.compile(r'GetEnvironmentVariableA\(\s*"(?P<name>' + name + r')"')),
        ("GetEnvironmentVariableW", re.compile(r'GetEnvironmentVariableW\(\s*L"(?P<name>' + name + r')"')),
        ("addWideString", re.compile(r'\baddWideString\(\s*u"(?P<name>' + name + r')"\s*\)')),
    )
    return tuple((reader, re.compile(form.pattern.replace(r"\(", r"\s*\(", 1), form.flags)) for reader, form in readers)


def line_reads(line, prefixes=PREFIXES):
    return [(reader, match) for reader, form in forms(tuple(prefixes)) for match in form.finditer(line)]


def documented_pattern(prefixes):
    return re.compile(r"`(" + _name(prefixes) + r")`")


def kind_of(reader, window):
    if reader == "IsEnvironmentSet":
        return "presence"
    if reader in INTEGER_READERS:
        return "integer"
    if reader in STRING_READERS:
        return "string"
    if NULL_TEST.search(window):
        return "presence"
    if INTEGER_TEST.search(window):
        return "integer"
    return "string"


def default_of(match, window):
    default = match.groupdict().get("default")
    if default:
        return default
    found = re.search(r"\?[\s\S]*?:\s*([0-9]+)", window)
    return found.group(1) if found else ""


def expression_window(text, code, matches, match, start, end):
    peers = sorted((other.start() for _, other in matches if start <= other.start() < end))
    if len(peers) < 2:
        return text[start:end]
    peer_set = set(peers)
    depths = {}
    commas = []
    depth = 0
    for pos in range(start, end):
        if pos in peer_set:
            depths[pos] = depth
        token = code[pos]
        if token in "([":
            depth += 1
        elif token in ")]":
            depth -= 1
        elif token == ",":
            commas.append((pos, depth))
    for pos, comma_depth in commas:
        left = bisect.bisect_left(peers, pos)
        if not left or left == len(peers):
            continue
        if comma_depth > min(depths[peers[left - 1]], depths[peers[left]]):
            continue
        if pos < match.start():
            start = max(start, pos + 1)
        elif pos >= match.end():
            end = min(end, pos)
    return text[start:end]


def source_files(root):
    root = Path(root)
    if not root.is_dir():
        return
    for path in sorted(root.rglob("*")):
        if path.suffix not in EXTENSIONS or any(part in SKIP for part in path.relative_to(root).parts):
            continue
        yield path


class _NamedRead:
    """A read whose name was reached through a string constant, positioned like the literal read."""

    def __init__(self, match, name):
        self.match = match
        self.name = name

    def start(self):
        return self.match.start()

    def end(self):
        return self.match.end()

    def group(self, key):
        return self.name if key == "name" else self.match.group(key)

    def groupdict(self):
        return {}


@functools.lru_cache(maxsize=None)
def constant_forms(prefixes=PREFIXES):
    name = _name(prefixes)
    return (
        re.compile(r'\b([A-Za-z_]\w*)\s*=\s*(?:u8|u|U|L)?"(?P<name>' + name + r')"\s*;'),
        re.compile(r'(?<![\w:])(?:std::)?(' + "|".join(IDENTIFIER_READERS) + r')\s*\(\s*(?P<ident>[A-Za-z_]\w*)\s*(?=[,)])'),
    )


SHADOW = re.compile(r'(?<![\w.:>])[A-Za-z_][\w:]*(?:\s*[*&]+\s*|\s+)(?:const\s+)?(?P<name>[A-Za-z_]\w*)\s*(?P<after>[=;,)\[{(])')
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)


def literal_start(prefixes=PREFIXES):
    return re.compile(r'=\s*(?:u8|u|U|L)?"(?:' + "|".join(re.escape(prefix) for prefix in prefixes) + ")")


def uncommented(text):
    return CPP_TOKEN.sub(lambda token: " " * len(token.group()) if token.group("comment") else token.group(), text)


def declared_constants(commentless, prefixes=PREFIXES):
    found = {}
    for match in constant_forms(tuple(prefixes))[0].finditer(commentless):
        found.setdefault(match.group(1), set()).add(match.group("name"))
    return found


def shadowed_names(commentless, prefixes=PREFIXES):
    literal = literal_start(prefixes)
    names = set()
    for line in commentless.split("\n"):
        if "constexpr" in line:
            continue
        for match in SHADOW.finditer(line):
            if match.group("after") == "=" and literal.match(line, match.start("after")):
                continue
            names.add(match.group("name"))
    return names


def visible_constants(path, sources, commentless, declared):
    by_name = {}
    for other in sources:
        by_name.setdefault(other.name, []).append(other)
    full = {other.as_posix(): other for other in sources}
    seen, pending, found = set(), [path], {}
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        for ident, names in declared[current].items():
            found.setdefault(ident, set()).update(names)
        for include in INCLUDE.findall(commentless[current]):
            relative = posixpath.normpath(posixpath.join(current.parent.as_posix(), include))
            if relative in full:
                pending.append(full[relative])
            else:
                pending.extend(other for other in by_name.get(PurePosixPath(include).name, [])
                               if other.as_posix().endswith("/" + include) or other.as_posix() == include)
    return found


def identifier_reads(code, constants, shadowed, prefixes=PREFIXES):
    _, reader_form = constant_forms(tuple(prefixes))
    out = []
    for match in reader_form.finditer(code):
        ident = match.group("ident")
        if ident in shadowed:
            continue
        names = constants.get(ident, set())
        if len(names) == 1:
            out.append((match.group(1), _NamedRead(match, next(iter(names)))))
    return out


def reads(roots, prefixes=PREFIXES):
    found = {}
    sources = []
    for root in roots:
        for path in source_files(root):
            try:
                text = path.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError) as error:
                raise ValueError(f"cannot read {path.as_posix()}: {error}") from error
            sources.append((path, text))
    commentless = {path: uncommented(text) for path, text in sources}
    declared = {path: declared_constants(commentless[path], prefixes) for path, _ in sources}
    for path, text in sources:
        constants = visible_constants(path, [item for item, _ in sources], commentless, declared)
        matches = line_reads(text, prefixes)
        code = CPP_TOKEN.sub(lambda token: token.group() if token.group("code") else " " * len(token.group()), text)
        matches += identifier_reads(code, constants, shadowed_names(commentless[path], prefixes), prefixes)
        if not matches:
            continue
        starts = [0] + [match.end() for match in re.finditer("\n", text)]
        for reader, match in matches:
            number = bisect.bisect_right(starts, match.start())
            start = max(code.rfind(token, 0, match.start()) for token in ";{}") + 1
            end = code.find(";", match.end())
            end = end + 1 if end >= 0 else len(text)
            window = expression_window(text, code, matches, match, start, end)
            found.setdefault(match.group("name"), []).append(
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
            cells = (f"`{name}`", kind.replace("\\", "\\\\"), f"`{source}`", default.replace("\\", "\\\\"))
            out.write("| " + " | ".join(cell(value) for value in cells) + " | |\n")
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
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
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
    for root in roots:
        if not Path(root).is_dir():
            print(f"error: scan root {root} is not a directory", file=sys.stderr)
            return 1
    try:
        found = reads(roots, prefixes)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    if args.check:
        return check(found, args.doc, prefixes, sys.stdout)
    if args.table:
        table(found, sys.stdout)
        return 0
    list_names(found, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
