import argparse
import bisect
import functools
import io
import os
import posixpath
import re
import sys
import tokenize
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nid_names import CPP_TOKEN
from sanitizer_report import cell

NON_NEWLINE = re.compile(r"[^\r\n]")
_CPP_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
_NEWLINE_RE = re.compile(r"\n")
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
COMPUTED_INTEGER = r"(?:0[bB][01]+(?:'[01]+)*|" + INTEGER + r")(?:[uU](?:ll|LL|[lL])?|(?:ll|LL|[lL])[uU]?)?"
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


_IDENTIFIER_READER_RE = re.compile(r"\b(?:" + "|".join(IDENTIFIER_READERS) + r")\b")


@functools.lru_cache(maxsize=None)
def _line_read_anchors(prefixes=PREFIXES):
    name = _name(prefixes)
    return {
        "getenv": re.compile(r'getenv\s*\(\s*"' + name + r'"\s*\)'),
        "_wgetenv": re.compile(r'wgetenv\s*\(\s*L"' + name + r'"\s*\)'),
        "CapturedEnvironmentEntry": re.compile(r'\.first\s*==\s*"' + name + r'"'),
    }


def _anchored_matches(reader, form, anchor, line):
    results = []
    if reader == "getenv":
        for hit in anchor.finditer(line):
            pos = hit.start()
            start = pos - 5 if pos >= 5 and line[pos - 5:pos] == "std::" else pos
            if start > 0 and (line[start - 1].isalnum() or line[start - 1] == "_"):
                continue
            match = form.match(line, start)
            if match is not None:
                results.append((reader, match))
        return results
    if reader == "_wgetenv":
        for hit in anchor.finditer(line):
            pos = hit.start()
            start = pos - 1 if pos >= 1 and line[pos - 1] == "_" else pos
            if start >= 5 and line[start - 5:start] == "std::":
                start -= 5
            if start > 0 and (line[start - 1].isalnum() or line[start - 1] == "_"):
                continue
            match = form.match(line, start)
            if match is not None:
                results.append((reader, match))
        return results
    if anchor.search(line) is None:
        return results
    for match in form.finditer(line):
        if match.start() == 0 or not (line[match.start() - 1].isalnum() or line[match.start() - 1] == "_"):
            results.append((reader, match))
    return results


def line_reads(line, prefixes=PREFIXES):
    prefixes = tuple(prefixes)
    anchors = _line_read_anchors(prefixes)
    results = []
    for reader, form in forms(prefixes):
        if not any(token in line for token in READER_TOKENS[reader]):
            continue
        anchor = anchors.get(reader)
        if anchor is not None:
            results.extend(_anchored_matches(reader, form, anchor, line))
            continue
        for match in form.finditer(line):
            if match.start() == 0 or not (line[match.start() - 1].isalnum() or line[match.start() - 1] == "_"):
                results.append((reader, match))
    return results


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
    found = re.search(r"\?[\s\S]*?:\s*([+-]?)\s*(" + INTEGER + ")", window)
    return "".join(found.groups()) if found else ""


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
    paths = []
    for current, dirs, files in os.walk(root):
        dirs[:] = [name for name in dirs if name not in SKIP]
        current_path = Path(current)
        for filename in files:
            if "." in filename:
                path = current_path / filename
                if path.suffix in EXTENSIONS:
                    paths.append(path)
    paths.sort()
    yield from paths


def _blank_preserve_eol(span):
    if "\n" not in span and "\r" not in span:
        return " " * len(span)
    return NON_NEWLINE.sub(" ", span)


def _token_before(text, index, floor):
    scan_start = index
    while scan_start > floor and text[scan_start - 1] in _PRE_TOKEN_CHARS:
        scan_start -= 1
    if scan_start == index:
        return None, None, index
    for token in _CODE_TOKEN.finditer(text, scan_start):
        if token.start() >= index:
            break
        if token.end() > index:
            return token.start(), token.group(), token.end()
        if token.end() == index:
            return token.start(), token.group(), index
    return None, None, index


def _scan_cpp(text, blank_raw_char, blank_string, preserve_eol):
    out = []
    pos = 0
    cursor = 0
    floor = 0
    no_block_close = False
    length = len(text)
    while cursor < length:
        trigger = _CPP_TRIGGER.search(text, cursor)
        if trigger is None:
            break
        index = trigger.start()
        token = trigger.group()
        if token == "//":
            end = text.find("\n", index + 2)
            if end == -1:
                end = length
            out.append(text[pos:index])
            span = text[index:end]
            out.append(_blank_preserve_eol(span) if preserve_eol else " " * (end - index))
            pos = cursor = floor = end
        elif token == "/*":
            if no_block_close:
                cursor = floor = index + 2
                continue
            close = text.find("*/", index + 2)
            if close == -1:
                no_block_close = True
                cursor = floor = index + 2
                continue
            end = close + 2
            out.append(text[pos:index])
            out.append(_blank_preserve_eol(text[index:end]) if preserve_eol else " " * (end - index))
            pos = cursor = floor = end
        elif token == '"':
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            if tok_text in _RAW_PREFIXES:
                raw_match = _RAW_BODY.match(text, index)
                if raw_match is not None:
                    end = raw_match.end()
                    if blank_raw_char:
                        out.append(text[pos:tok_start])
                        out.append(_blank_preserve_eol(text[tok_start:end]) if preserve_eol else " " * (end - tok_start))
                        pos = end
                    cursor = floor = end
                    continue
            str_match = _STRING_BODY.match(text, index)
            if str_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = str_match.end()
                if blank_string:
                    out.append(text[pos:start])
                    out.append(_blank_preserve_eol(text[start:end]) if preserve_eol else " " * (end - start))
                    pos = end
                cursor = floor = end
            else:
                cursor = floor = index + 1
        else:
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            char_match = _CHAR_BODY.match(text, index)
            if char_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = char_match.end()
                if blank_raw_char:
                    out.append(text[pos:start])
                    out.append(_blank_preserve_eol(text[start:end]) if preserve_eol else " " * (end - start))
                    pos = end
                cursor = floor = end
            else:
                cursor = floor = index + 1
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def cpp_reads(text):
    if "//" not in text and "/*" not in text and 'R"' not in text and "'" not in text:
        return text
    return _scan_cpp(text, blank_raw_char=True, blank_string=False, preserve_eol=True)


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
    return _scan_cpp(text, blank_raw_char=True, blank_string=True, preserve_eol=False)


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
                             r'(?:(?P<sign>[+-])\s*(?P<offset>' + COMPUTED_INTEGER + r'))?\s*\)\s*\+\s*"(?P<suffix>[A-Za-z0-9_]*)"')
    loop = re.compile(r'\bfor\s*\(\s*[^;{}]*?\b(?P<slot>[A-Za-z_]\w*)\s*=\s*(?P<start>' + COMPUTED_INTEGER + r')\s*;\s*'
                      r'(?P=slot)\s*<\s*(?P<bound>[^;{}]+?)\s*;\s*(?:(?P=slot)\s*\+\+|\+\+\s*(?P=slot))\s*\)')
    reader = re.compile(r'(?<![\w:])(?:std::)?(?P<reader>' + "|".join(IDENTIFIER_READERS) + r')\s*\(\s*'
                        r'(?P<ident>[A-Za-z_]\w*)\.c_str\s*\(\s*\)')
    return declaration, loop, reader


def _integer(value):
    value = value.replace("'", "").rstrip("uUlL")
    if value[:2] in ("0b", "0B"):
        return int(value[2:], 2)
    if value[:2] in ("0x", "0X"):
        return int(value[2:], 16)
    if len(value) > 1 and value[0] == "0":
        return int(value[1:], 8)
    return int(value)


def _loop_bound(bound, text):
    bound = bound.strip()
    if re.fullmatch(COMPUTED_INTEGER, bound):
        return _integer(bound)
    constant = re.search(r'\b(?:constexpr|const)\s+[A-Za-z_][\w:]*\s+' + re.escape(bound)
                         + r'\s*=\s*(' + COMPUTED_INTEGER + r')\s*;', text)
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
                      + r'\s*=\s*std::array\s*<[^;]*?,\s*(' + COMPUTED_INTEGER + r'|[A-Za-z_]\w*)\s*>', text)
    if not alias:
        return None
    size = alias.group(1)
    if re.fullmatch(COMPUTED_INTEGER, size):
        return _integer(size)
    constant = re.search(r'\b(?:constexpr|const)\s+[A-Za-z_][\w:]*\s+' + re.escape(size)
                         + r'\s*=\s*(' + COMPUTED_INTEGER + r')\s*;', text)
    return _integer(constant.group(1)) if constant else None


@functools.lru_cache(maxsize=None)
def _computed_decl_anchor(prefixes=PREFIXES):
    prefix = "|".join(re.escape(prefix) for prefix in prefixes)
    return re.compile(r'"(?:' + prefix + r')[A-Za-z0-9_]*"\s*\+\s*(?:std::)?to_string\s*\(')


def computed_reads(text, prefixes=PREFIXES):
    if "to_string" not in text or ".c_str" not in text:
        return []
    prefixes = tuple(prefixes)
    if _computed_decl_anchor(prefixes).search(text) is None:
        return []
    declaration, loop, reader = computed_forms(prefixes)
    declarations = list(declaration.finditer(text))
    if not declarations:
        return []
    readers = list(reader.finditer(text))
    if not readers:
        return []
    loops = [(match.start(), match.group("slot"), match.group("start"), match.group("bound"))
             for match in loop.finditer(text)]
    names, wildcards = {}, {}
    for match in declarations:
        expand = max((item for item in loops if item[0] < match.start() and item[1] == match.group("slot")),
                     default=None)
        bound = _loop_bound(expand[3], text) if expand else None
        ident = match.group("ident")
        if bound is not None:
            start = _integer(expand[2])
            offset = _integer(match.group("offset")) if match.group("offset") else 0
            if match.group("sign") == "-":
                offset = -offset
            names.setdefault(ident, set()).update(match.group("prefix") + str(value) + match.group("suffix")
                                                  for value in range(start + offset, bound + offset))
        else:
            wildcards.setdefault(ident, set()).add(match.group("prefix") + "*" + match.group("suffix"))
    found = []
    for match in readers:
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
    return _scan_cpp(text, blank_raw_char=False, blank_string=False, preserve_eol=False)


@functools.lru_cache(maxsize=None)
def _declared_rhs(prefixes=PREFIXES):
    return re.compile(r'=\s*(?:u8|u|U|L)?"(?P<name>' + _name(prefixes) + r')"\s*;')


def declared_constants(commentless, prefixes=PREFIXES):
    if "=" not in commentless or ";" not in commentless or '"' not in commentless:
        return {}
    found = {}
    rhs = _declared_rhs(tuple(prefixes))
    for match in rhs.finditer(commentless):
        end = match.start()
        while end > 0 and commentless[end - 1].isspace():
            end -= 1
        start = end
        while start > 0 and (commentless[start - 1].isalnum() or commentless[start - 1] == "_"):
            start -= 1
        if start < end and (commentless[start].isascii() and (commentless[start].isalpha() or commentless[start] == "_")):
            found.setdefault(commentless[start:end], set()).add(match.group("name"))
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
    all_declared_idents = set()
    for path, raw in raw_sources:
        if quoted_prefixes is None or any(prefix in raw for prefix in quoted_prefixes):
            item = declared_constants(commentless_of(path), prefixes)
            if item:
                all_declared_idents.update(item)
            declared[path] = item
        else:
            declared[path] = {}

    declared_ident_re = (
        re.compile(r"\b(?:" + "|".join(re.escape(ident) for ident in sorted(all_declared_idents)) + r")\b")
        if all_declared_idents else None
    )
    includes_cache = {}
    index = _source_index([path for path, _ in raw_sources]) if declared_ident_re is not None else None

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
        if (declared_ident_re is not None and _IDENTIFIER_READER_RE.search(raw) is not None
                and declared_ident_re.search(raw) is not None):
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
        starts = [0] + [match.end() for match in _NEWLINE_RE.finditer(text)]
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
        for name, (source, _number, kind, default) in sorted(libraries[library]):
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
