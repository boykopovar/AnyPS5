import argparse
import bisect
import functools
import io
import posixpath
import re
import sys
import tokenize
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nid_names import CPP_TOKEN
from sanitizer_report import cell

NON_NEWLINE = re.compile(r"[^\r\n]")
READER_TOKENS = {
    "getenv": ("getenv",),
    "_wgetenv": ("wgetenv",),
    "IsEnvironmentSet": ("IsEnvironmentSet",),
    "MaskFromEnvironment": ("MaskFromEnvironment",),
    "kibSetting": ("kibSetting",),
    "EnvInt": ("EnvInt",),
    "configured": ("configured", "PipelineSwitch"),
    "environmentCount": ("environmentCount",),
    "EnvironmentNumber": ("EnvironmentNumber",),
    "GetEnvironmentVariableA": ("GetEnvironmentVariableA",),
    "GetEnvironmentVariableW": ("GetEnvironmentVariableW",),
    "addWideString": ("addWideString",),
    "MebibytesFromEnvironment": ("MebibytesFromEnvironment",),
    "EnvironmentPath": ("EnvironmentPath",),
    "CapturedEnvironmentEntry": (".first",),
}
ALL_READER_TOKENS = tuple(dict.fromkeys(token for tokens in READER_TOKENS.values() for token in tokens))

PREFIXES = ("APS5_", "ANYPS5_")
DOC = "docs/dev/ENVIRONMENT.md"
ROOTS = ("core",)
EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp", ".py"}
SKIP = {"build", "build-linux", "build-relinker", "3rdparty", ".git"}

INTEGER_READERS = {"kibSetting", "MaskFromEnvironment", "EnvInt", "configured", "environmentCount", "EnvironmentNumber",
                   "MebibytesFromEnvironment"}
IDENTIFIER_READERS = ("getenv", "IsEnvironmentSet", "MaskFromEnvironment", "kibSetting", "EnvInt", "configured",
                      "PipelineSwitch", "environmentCount", "EnvironmentNumber", "MebibytesFromEnvironment")
STRING_READERS = {"GetEnvironmentVariableA", "GetEnvironmentVariableW", "addWideString"}
NULL_TEST = re.compile(r"[!=]=\s*(nullptr|NULL|0)\b|!\s*(?:std::)?_?w?getenv\s*\(")
INTEGER_TEST = re.compile(r"\b(atoi|strtol|strtoul|strtoll|strtoull|stoi|stol|stoul|from_chars|atoi64|_wtoi|_wtol|_wtoi64)\s*\(")
INTEGER = r"(?:0[xX][0-9A-Fa-f]+(?:'[0-9A-Fa-f]+)*|[0-9]+(?:'[0-9]+)*)"
GREP = (r'(getenv|_wgetenv|wgetenv|IsEnvironmentSet|kibSetting|MaskFromEnvironment|EnvInt|GetEnvironmentVariable[AW]'
        r'|configured|PipelineSwitch|environmentCount|EnvironmentNumber|MebibytesFromEnvironment|addWideString)'
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
        ("kibSetting", re.compile(r'kibSetting\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>' + INTEGER + r')')),
        ("EnvInt", re.compile(r'EnvInt\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>-?' + INTEGER + r')')),
        ("configured", re.compile(r'\b(?:configured|PipelineSwitch)\(\s*"(?P<name>' + name + r')"'
                                  r'(?:\s*,\s*(?P<default>' + INTEGER + r')[uUlL]*)?')),
        ("environmentCount", re.compile(r'\benvironmentCount\(\s*"(?P<name>' + name + r')"\s*,\s*(?P<default>' + INTEGER + r')')),
        ("EnvironmentNumber", re.compile(r'\bEnvironmentNumber\(\s*"(?P<name>' + name + r')"\s*\)')),
        ("GetEnvironmentVariableA", re.compile(r'GetEnvironmentVariableA\(\s*"(?P<name>' + name + r')"')),
        ("GetEnvironmentVariableW", re.compile(r'GetEnvironmentVariableW\(\s*L"(?P<name>' + name + r')"')),
        ("addWideString", re.compile(r'\baddWideString\(\s*u"(?P<name>' + name + r')"\s*\)')),
        ("MebibytesFromEnvironment", re.compile(r'\bMebibytesFromEnvironment\(\s*"(?P<name>' + name + r')"'
                                                r'\s*,\s*(?P<default>' + INTEGER + r')')),
        ("EnvironmentPath", re.compile(r'\bEnvironmentPath\(\s*L"(?P<name>' + name + r')"\s*,\s*"(?P=name)"\s*\)')),
        ("CapturedEnvironmentEntry", re.compile(r'[A-Za-z_]\w*(?:\s*\[\s*[^\]]*\s*\])?\s*\.first\s*==\s*'
                                                r'"(?P<name>' + name + r')"')),
    )
    return tuple((reader, re.compile(form.pattern.replace(r"\(", r"\s*\(", 1), form.flags)) for reader, form in readers)


def line_reads(line, prefixes=PREFIXES):
    return [(reader, match) for reader, form in forms(tuple(prefixes))
            if any(token in line for token in READER_TOKENS[reader])
            for match in form.finditer(line)
            if match.start() == 0 or not (line[match.start() - 1].isalnum() or line[match.start() - 1] == "_")]


def documented_pattern(prefixes):
    return re.compile(r"^\|[ \t]*`(" + _name(prefixes) + r")`[ \t]*\|", re.M)


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
    found = re.search(r"\?[\s\S]*?:\s*(" + INTEGER + ")", window)
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


def cpp_reads(text):
    if "//" not in text and "/*" not in text and 'R"' not in text and "'" not in text:
        return text
    return CPP_TOKEN.sub(lambda match: NON_NEWLINE.sub(" ", match.group())
                         if match.lastgroup in ("comment", "raw", "char") else match.group(), text)


def python_reads(text):
    offsets = [0] + [index + 1 for index, char in enumerate(text) if char == "\n"]
    result = list(text)
    for token in tokenize.generate_tokens(io.StringIO(text).readline):
        if token.type == tokenize.COMMENT:
            start = offsets[token.start[0] - 1] + token.start[1]
            end = offsets[token.end[0] - 1] + token.end[1]
            result[start:end] = " " * (end - start)
    return "".join(result)


def _blank_non_code(text):
    if '"' not in text and "'" not in text and "//" not in text and "/*" not in text:
        return text
    return CPP_TOKEN.sub(lambda token: token.group() if token.group("code") else " " * len(token.group()), text)


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


@functools.lru_cache(maxsize=None)
def computed_forms(prefixes=PREFIXES):
    prefix = "|".join(re.escape(prefix) for prefix in prefixes)
    declaration = re.compile(r'\b(?P<ident>[A-Za-z_]\w*)\s*=\s*(?:u8|u|U|L)?"(?P<prefix>(?:' + prefix + r')[A-Za-z0-9_]*)"'
                             r'\s*\+\s*(?:std::)?to_string\s*\(\s*(?P<slot>[A-Za-z_]\w*)\s*'
                             r'(?:(?P<sign>[+-])\s*(?P<offset>' + INTEGER + r'))?\s*\)\s*\+\s*"(?P<suffix>[A-Za-z0-9_]*)"')
    loop = re.compile(r'\bfor\s*\(\s*[^;{}]*?\b(?P<slot>[A-Za-z_]\w*)\s*=\s*(?P<start>' + INTEGER + r')\s*;\s*'
                      r'(?P=slot)\s*<\s*(?P<bound>[^;{}]+?)\s*;\s*(?:(?P=slot)\s*\+\+|\+\+\s*(?P=slot))\s*\)')
    reader = re.compile(r'(?<![\w:])(?:std::)?(?P<reader>' + "|".join(IDENTIFIER_READERS) + r')\s*\(\s*'
                        r'(?P<ident>[A-Za-z_]\w*)\.c_str\s*\(\s*\)')
    return declaration, loop, reader


def _integer(value):
    value = value.replace("'", "")
    if value[:2] in ("0x", "0X"):
        return int(value[2:], 16)
    if len(value) > 1 and value[0] == "0":
        return int(value[1:], 8)
    return int(value)


def _loop_bound(bound, text):
    bound = bound.strip()
    if re.fullmatch(INTEGER, bound):
        return _integer(bound)
    constant = re.search(r'\b(?:constexpr|const)\s+[A-Za-z_][\w:]*\s+' + re.escape(bound)
                         + r'\s*=\s*(' + INTEGER + r')\b', text)
    if constant:
        return _integer(constant.group(1))
    sized = re.fullmatch(r'(?:std::)?size\s*\(\s*([A-Za-z_]\w*)\s*\)|([A-Za-z_]\w*)\.size\s*\(\s*\)', bound)
    if not sized:
        return None
    array = sized.group(1) or sized.group(2)
    declared = re.search(r'\b([A-Za-z_]\w*)\s+' + re.escape(array) + r'\s*;', text)
    if not declared:
        return None
    alias = re.search(r'\busing\s+' + re.escape(declared.group(1))
                      + r'\s*=\s*std::array\s*<[^;]*?,\s*(' + INTEGER + r'|[A-Za-z_]\w*)\s*>', text)
    if not alias:
        return None
    size = alias.group(1)
    if re.fullmatch(INTEGER, size):
        return _integer(size)
    constant = re.search(r'\b(?:constexpr|const)\s+[A-Za-z_][\w:]*\s+' + re.escape(size)
                         + r'\s*=\s*(' + INTEGER + r')\b', text)
    return _integer(constant.group(1)) if constant else None


def computed_reads(text, prefixes=PREFIXES):
    declaration, loop, reader = computed_forms(tuple(prefixes))
    loops = [(match.start(), match.group("slot"), match.group("start"), match.group("bound"))
             for match in loop.finditer(text)]
    names, wildcards = {}, {}
    for match in declaration.finditer(text):
        expand = max((item for item in loops if item[0] < match.start() and item[1] == match.group("slot")),
                     default=None)
        bound = _loop_bound(expand[3], text) if expand else None
        ident = match.group("ident")
        if bound is not None and bound > 0:
            start = _integer(expand[2])
            offset = _integer(match.group("offset")) if match.group("offset") else 0
            if match.group("sign") == "-":
                offset = -offset
            names.setdefault(ident, set()).update(match.group("prefix") + str(value) + match.group("suffix")
                                                  for value in range(start + offset, start + offset + bound))
        else:
            wildcards.setdefault(ident, set()).add(match.group("prefix") + "*" + match.group("suffix"))
    found = []
    for match in reader.finditer(text):
        ident = match.group("ident")
        for name in sorted(names.get(ident, set()) | wildcards.get(ident, set())):
            found.append((match.group("reader"), _NamedRead(match, name)))
    return found


SHADOW = re.compile(r'(?<![\w.:>])[A-Za-z_][\w:]*(?:\s*[*&]+\s*|\s+)(?:const\s+)?(?P<name>[A-Za-z_]\w*)\s*(?P<after>[=;,)\[{(])')
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)


def literal_start(prefixes=PREFIXES):
    return re.compile(r'=\s*(?:u8|u|U|L)?"(?:' + "|".join(re.escape(prefix) for prefix in prefixes) + ")")


def uncommented(text):
    if "//" not in text and "/*" not in text:
        return text
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


def _source_index(sources):
    by_name = {}
    full = {}
    for other in sources:
        posix = other.as_posix()
        full[posix] = other
        by_name.setdefault(other.name, []).append((other, posix))
    return by_name, full


def _resolve_includes(path, commentless_text, index):
    by_name, full = index
    parent = path.parent.as_posix()
    resolved = []
    for include in INCLUDE.findall(commentless_text):
        relative = posixpath.normpath(posixpath.join(parent, include))
        if relative in full:
            resolved.append(full[relative])
        else:
            suffix = "/" + include
            resolved.extend(other for other, posix in by_name.get(PurePosixPath(include).name, ())
                            if posix.endswith(suffix) or posix == include)
    return tuple(resolved)


def _visible_constants(path, includes_of, declared):
    seen, pending, found = set(), [path], {}
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        for ident, names in declared[current].items():
            found.setdefault(ident, set()).update(names)
        pending.extend(includes_of(current))
    return found


def visible_constants(path, sources, commentless, declared):
    index = _source_index(sources)
    return _visible_constants(path, lambda current: _resolve_includes(current, commentless[current], index), declared)


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
    prefixes = tuple(prefixes)
    raw_sources = []
    for root in roots:
        for path in source_files(root):
            try:
                raw = path.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError) as error:
                raise ValueError(f"cannot read {path.as_posix()}: {error}") from error
            raw_sources.append((path, raw))
    raw_by_path = dict(raw_sources)
    cleaned = {}
    uncommented_cache = {}

    def text_of(path):
        text = cleaned.get(path)
        if text is None:
            raw = raw_by_path[path]
            try:
                text = python_reads(raw) if path.suffix == ".py" else cpp_reads(raw)
            except (tokenize.TokenError, IndentationError, SyntaxError) as error:
                raise ValueError(f"cannot tokenize {path.as_posix()}: {error}") from error
            cleaned[path] = text
        return text

    def commentless_of(path):
        text = uncommented_cache.get(path)
        if text is None:
            text = text_of(path) if path.suffix != ".py" else uncommented(text_of(path))
            uncommented_cache[path] = text
        return text

    quoted_prefixes = (tuple('"' + prefix for prefix in prefixes)
                       if prefixes and all(prefix and not prefix[0].isspace() for prefix in prefixes) else None)
    declared = {}
    has_declared = False
    for path, raw in raw_sources:
        if quoted_prefixes is None or any(prefix in raw for prefix in quoted_prefixes):
            item = declared_constants(commentless_of(path), prefixes)
            if item:
                has_declared = True
            declared[path] = item
        else:
            declared[path] = {}

    includes_cache = {}
    index = _source_index([path for path, _ in raw_sources]) if has_declared else None

    def includes_of(path):
        targets = includes_cache.get(path)
        if targets is None:
            raw = raw_by_path[path]
            targets = _resolve_includes(path, commentless_of(path), index) if "#" in raw and "include" in raw else ()
            includes_cache[path] = targets
        return targets

    for path, raw in raw_sources:
        has_literal = ((quoted_prefixes is None or any(prefix in raw for prefix in quoted_prefixes))
                       and any(token in raw for token in ALL_READER_TOKENS))
        matches = line_reads(text_of(path), prefixes) if has_literal else []
        if has_literal and "to_string" in raw:
            matches += computed_reads(text_of(path), prefixes)
        code = None
        if has_declared and any(reader in raw for reader in IDENTIFIER_READERS):
            constants = _visible_constants(path, includes_of, declared)
            if constants:
                text = text_of(path)
                code = _blank_non_code(text)
                matches += identifier_reads(code, constants, shadowed_names(commentless_of(path), prefixes), prefixes)
        if not matches:
            continue
        text = text_of(path)
        if code is None:
            code = _blank_non_code(text)
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
    covered = set()
    missing = []
    undetermined = []
    for name in sorted(found):
        if "*" in name:
            pattern = re.compile(re.escape(name).replace(r"\*", "[A-Za-z0-9_]+") + r"\Z")
            covered |= {candidate for candidate in names if pattern.match(candidate)}
            undetermined.append(name)
        elif name not in names:
            missing.append(name)
    stale = sorted(names - set(found) - covered)
    for name in missing:
        source, number, _, _ = found[name][0]
        out.write(f"error: {name} is read at {source}:{number} but has no row in {doc}\n")
    for name in undetermined:
        source, number, _, _ = found[name][0]
        out.write(f"error: {name} is read at {source}:{number} but the names it can take are not known, "
                  f"so {doc} cannot be checked for it\n")
    for name in stale:
        out.write(f"error: {name} has a row in {doc} but the code no longer reads it\n")
    if missing or stale or undetermined:
        summary = f"{len(missing)} name(s) undocumented, {len(stale)} stale"
        if undetermined:
            summary += f", {len(undetermined)} undetermined"
        out.write(summary + "\n")
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
