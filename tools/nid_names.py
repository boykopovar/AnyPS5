#!/usr/bin/env python3
import argparse
import shutil
import hashlib
import json
import os
import re
import sys
import tempfile
import urllib.request
from base64 import b64encode
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
DB_URL = ("https://raw.githubusercontent.com/zecoxao/sce_symbols"
          "/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv")
DEFAULT_CACHE = Path(tempfile.gettempdir()) / "anyps5-nid-db" / "aerolib.csv"

SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
                0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
CPP_TOKEN = re.compile(
    r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)'
    r'|(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*')"
    r"|(?P<code>(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S)")
DEFINITION = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:->\s*[^;{]+)?(?:try\s*)?\{")
NID_POSTFIX = "_nid_postfix"
ASM_FUNCTION_MACROS = {"APS5_ASM_FUNCTION", "APS5_SIGNAL_ASM_FUNCTION"}
_DISAMBIG_SUFFIX = re.compile(r"_nid_disambig[0-9]+$")
_NID_VALUE = re.compile(r"[A-Za-z0-9+\-]{11}")
_NID_LITERAL = re.compile(r'"[A-Za-z0-9+\-]{11}"')
_IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_IDENT_W = re.compile(r"[A-Za-z_]\w*")
_IDENT_OR_GT = re.compile(r"[A-Za-z_]\w*|>")
_POSTFIX_IDENT = re.compile(r"[A-Za-z_]\w*_nid_postfix")
_DIRECT_INIT_ARG = re.compile(r"[0-9.]|(?:u8|u|U|L)?R?[\"']")
_IDENT_LITERAL = re.compile(r'"([A-Za-z_][A-Za-z0-9_]*)"')
_NON_NEWLINE = re.compile(r"[^\n]+")
_NON_CODE_TRIGGER = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|["\']')
_SKIP_SCOPE_TRIGGER = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|[{}#"\']')
_ALIAS_TRIGGER = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|["\']|\bAPS5_EXPORT\b')
_ASM_TRIGGER = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|["\']|\b(?:APS5_ASM_FUNCTION|APS5_SIGNAL_ASM_FUNCTION)\b')
_STRING_TAIL = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_TAIL = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_RAW_HEAD = re.compile(r'"(?P<delim>[^\s()\\]{0,16})\(')
_NUM_TAIL = re.compile(r"[A-Za-z0-9_.']*")
_NUMERIC_START = re.compile(r"[0-9]|\.[0-9]")
_ASCII_IDENT = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_")
_NUM_PREFIX_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")


def _join_continuations(text):
    if "\\\n" in text:
        text = text.replace("\\\n", "")
    if "\\\r\n" in text:
        text = text.replace("\\\r\n", "")
    return text


def _numeric_token_end(text, pos, floor):
    if pos <= floor or text[pos - 1] not in _NUM_PREFIX_CHARS:
        return None
    newline = text.rfind("\n", floor, pos)
    scan_start = newline + 1 if newline >= 0 else floor
    for match in CPP_TOKEN.finditer(text, scan_start, pos + 1):
        if match.start() < pos < match.end():
            if match.lastgroup == "code" and _NUMERIC_START.match(text, match.start()) is not None:
                return _NUM_TAIL.match(text, pos).end()
            return None
        if match.start() >= pos:
            break
    return None


def _is_standalone_prefix(text, prefix_pos, floor):
    if prefix_pos < floor:
        return False
    if prefix_pos == floor:
        return True
    prev = text[prefix_pos - 1]
    if prev in _ASCII_IDENT:
        return False
    if prev in ".'" and _numeric_token_end(text, prefix_pos, floor) is not None:
        return False
    return True


def _literal_prefix_start(text, quote_pos, floor):
    if quote_pos >= 2 and text[quote_pos - 2:quote_pos] == "u8":
        if _is_standalone_prefix(text, quote_pos - 2, floor):
            return quote_pos - 2
    if quote_pos >= 1 and text[quote_pos - 1] in "uUL":
        if _is_standalone_prefix(text, quote_pos - 1, floor):
            return quote_pos - 1
    return quote_pos


def _raw_prefix_start(text, quote_pos, floor):
    if quote_pos >= 1 and text[quote_pos - 1] == "R":
        start = _literal_prefix_start(text, quote_pos - 1, floor)
        if start < quote_pos - 1 or _is_standalone_prefix(text, quote_pos - 1, floor):
            return start
    return None


def _consume_quote(text, start, floor):
    if text[start] == '"':
        raw_start = _raw_prefix_start(text, start, floor)
        if raw_start is not None:
            head = _RAW_HEAD.match(text, start)
            if head is not None:
                closing = ")" + head.group("delim") + '"'
                close_pos = text.find(closing, head.end())
                if close_pos >= 0:
                    return raw_start, close_pos + len(closing), True
        string_match = _STRING_TAIL.match(text, start)
        if string_match is not None:
            return _literal_prefix_start(text, start, floor), string_match.end(), True
        return start, start + 1, False
    num_end = _numeric_token_end(text, start, floor)
    if num_end is not None:
        return start, num_end, False
    char_match = _CHAR_TAIL.match(text, start)
    if char_match is not None:
        return _literal_prefix_start(text, start, floor), char_match.end(), True
    return start, start + 1, False


def _blank_span(token):
    if "\n" not in token:
        return " " * len(token)
    return _NON_NEWLINE.sub(lambda part: " " * len(part.group()), token)


def _blank_non_code(text):
    if "/" not in text and '"' not in text and "'" not in text:
        return text
    out = []
    pos = 0
    last = 0
    length = len(text)
    search = _NON_CODE_TRIGGER.search
    while pos < length:
        match = search(text, pos)
        if match is None:
            break
        start, end = match.start(), match.end()
        if text[start] == "/":
            out.append(text[last:start])
            out.append(_blank_span(text[start:end]))
            pos = last = end
        else:
            lit_start, lit_end, is_literal = _consume_quote(text, start, last)
            if is_literal:
                out.append(text[last:lit_start])
                out.append(_blank_span(text[lit_start:lit_end]))
                pos = last = lit_end
            else:
                pos = lit_end
    if last == 0:
        return text
    out.append(text[last:])
    return "".join(out)


def _skip_ignored_scope(text, pos):
    length = len(text)
    depth = 1
    floor = pos
    search = _SKIP_SCOPE_TRIGGER.search
    while depth > 0 and pos < length:
        match = search(text, pos)
        if match is None:
            return length
        start, end = match.start(), match.end()
        ch = text[start]
        if ch == "{":
            depth += 1
            pos = end
        elif ch == "}":
            depth -= 1
            pos = end
        elif ch == "/":
            pos = floor = end
        elif ch == "#":
            if not text[text.rfind("\n", 0, start) + 1:start].strip():
                line_end = text.find("\n", end)
                pos = floor = length if line_end < 0 else line_end
            else:
                pos = end
        else:
            _, lit_end, is_literal = _consume_quote(text, start, floor)
            pos = lit_end
            if is_literal:
                floor = lit_end
    return pos


def compute_nid(symbol):
    digest = hashlib.sha1(symbol.encode() + SUFFIX).digest()
    return b64encode(digest[:8][::-1], altchars=b"+-").decode("ascii").rstrip("=")


def strip_nid_postfix(name):
    return _DISAMBIG_SUFFIX.sub("", name.removesuffix(NID_POSTFIX))


def read_db(path):
    db = {}
    with open(path, encoding="utf-8-sig", errors="replace") as handle:
        for line in handle:
            parts = line.strip().split()
            if len(parts) >= 2 and not parts[0].startswith("#"):
                nid, name = (part.split("#", 1)[0] for part in parts[:2])
                if nid and name:
                    previous = db.setdefault(nid, name)
                    if previous != name and compute_nid(name) == nid and compute_nid(previous) != nid:
                        db[nid] = name
    return db


DOWNLOAD_TIMEOUT = 30


def load_db(path, *, offline=False):
    if not offline and not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        print("downloading NID database to %s ..." % path, file=sys.stderr)
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".nid-db-", suffix=".tmp", delete=False) as download:
            temporary = Path(download.name)
        try:
            with urllib.request.urlopen(DB_URL, timeout=DOWNLOAD_TIMEOUT) as response, temporary.open("wb") as target:
                shutil.copyfileobj(response, target)
                expected = response.headers.get("Content-Length")
                if expected is not None and target.tell() < int(expected):
                    raise OSError("incomplete NID database download")
            db = read_db(temporary)
            if not any(_NID_VALUE.fullmatch(nid) for nid in db):
                raise ValueError(f"{path}: downloaded database contains no valid NID records")
            temporary.replace(path)
            return db
        finally:
            temporary.unlink(missing_ok=True)
    db = read_db(path)
    if not any(_NID_VALUE.fullmatch(nid) for nid in db):
        raise ValueError(f"{path}: database contains no valid NID records")
    return db


def source_files(root):
    found = []
    for current, dirs, files in os.walk(Path(root)):
        dirs[:] = [name for name in dirs if name.lower() not in {"test", "tests"}]
        for name in files:
            if name.endswith(".cpp"):
                found.append(Path(current) / name)
    found.sort()
    return iter(found)


def _source_aliases_joined(text):
    if "APS5_EXPORT" not in text:
        return
    pos = 0
    floor = 0
    length = len(text)
    search = _ALIAS_TRIGGER.search
    while pos < length:
        match = search(text, pos)
        if match is None:
            break
        start, end = match.start(), match.end()
        ch = text[start]
        if ch == "/":
            pos = floor = end
            continue
        if ch in "\"'":
            _, lit_end, is_literal = _consume_quote(text, start, floor)
            pos = lit_end
            if is_literal:
                floor = lit_end
            continue
        pos = end
        tokens = []
        for following in CPP_TOKEN.finditer(text, end):
            if following.group("comment") is not None:
                continue
            tokens.append(following.group())
            if len(tokens) == 5:
                break
        if len(tokens) < 5:
            continue
        opening, literal, comma, name, closing = tokens
        if opening != "(" or comma != "," or closing != ")":
            continue
        if _NID_LITERAL.fullmatch(literal) is None:
            continue
        if _IDENT.fullmatch(name):
            yield literal[1:-1], name


def source_aliases(text):
    return _source_aliases_joined(_join_continuations(text))


def _source_definitions_joined(text):
    if "APS5_VABI" not in text:
        return iter(())
    return DEFINITION.finditer(_blank_non_code(text))


def source_definitions(text):
    return _source_definitions_joined(_join_continuations(text))


def _source_assembly_definitions_joined(text):
    if not any(macro in text for macro in ASM_FUNCTION_MACROS):
        return
    pos = 0
    floor = 0
    length = len(text)
    search = _ASM_TRIGGER.search
    while pos < length:
        match = search(text, pos)
        if match is None:
            break
        start, end = match.start(), match.end()
        ch = text[start]
        if ch == "/":
            pos = floor = end
            continue
        if ch in "\"'":
            _, lit_end, is_literal = _consume_quote(text, start, floor)
            pos = lit_end
            if is_literal:
                floor = lit_end
            continue
        pos = end
        tokens = []
        for following in CPP_TOKEN.finditer(text, end):
            if following.group("comment") is not None:
                continue
            tokens.append(following)
            if len(tokens) == 3:
                break
        if len(tokens) < 3:
            continue
        opening, literal, closing = tokens
        if opening.group() != "(" or closing.group() != ")" or literal.group("string") is None:
            continue
        parsed = _IDENT_LITERAL.fullmatch(literal.group("string"))
        if parsed is not None:
            yield parsed.group(1)


def source_assembly_definitions(text):
    return _source_assembly_definitions_joined(_join_continuations(text))


def data_declarations(tokens, c_linkage):
    direct = tokens[:2] in (["extern", '"C"'], ["extern", '"C++"'])
    if direct:
        c_linkage = tokens[1] == '"C"'
        tokens = tokens[2:]
    if not c_linkage or not tokens or tokens[0] in {"using", "typedef"}:
        return []
    header = tokens[:next((index for index, token in enumerate(tokens) if token in {"=", "{"}), len(tokens))]
    if "static" in header:
        return []
    external = direct or "extern" in header
    names, groups = [], []
    name, initialized, local = None, False, False
    base = header[:next((index for index, token in enumerate(header) if token in {"*", "&", ","}), len(header))]
    base_const, constexpr, start = "const" in base, "constexpr" in base, 0
    pairs = {"(": ")", "[": "]", "{": "}", "<": ">"}
    angle_ends, end = [None] * len(tokens), None
    for index in range(len(tokens) - 1, -1, -1):
        if tokens[index] in {">", "=", "{"}:
            end = tokens[index]
        angle_ends[index] = end
    for index, token in enumerate(tokens):
        if token == "," and not groups:
            if name is not None and not local and (initialized or not external):
                names.append(name)
            name, initialized, local, start = None, False, False, index + 1
        elif token == "=" and not groups:
            initialized = True
        elif not initialized and token.endswith(NID_POSTFIX) and _POSTFIX_IDENT.fullmatch(token):
            prefix = tokens[start:index]
            unqualified = prefix[:]
            while unqualified and unqualified[-1] in {"const", "volatile"}:
                unqualified.pop()
            pointer = len(groups) == 1 and unqualified[-2:] == ["(", "*"]
            following = tokens[index + 1] if index + 1 < len(tokens) else None
            previous = tokens[index - 1] if index else None
            argument_index = index + 2
            while argument_index < len(tokens) and tokens[argument_index] in {"+", "-", "!", "~", "("}:
                argument_index += 1
            argument = tokens[argument_index] if argument_index < len(tokens) else ""
            direct_init = following == "(" and (_DIRECT_INIT_ARG.match(argument) or argument in {"true", "false", "nullptr"})
            if (not groups or pointer) and (following != "(" or direct_init) and previous not in {"struct", "class", "union", "enum"}:
                name = token
                pointers = [offset for offset, item in enumerate(prefix) if item in {"*", "&"}]
                local = not external and (constexpr or ("const" in prefix[pointers[-1] + 1:] if pointers else base_const))
                if direct_init:
                    initialized = True
        template = False
        if token == "<" and index > 0 and _IDENT_OR_GT.fullmatch(tokens[index - 1]) and \
                index + 1 < len(tokens) and tokens[index + 1] not in {"<", "="}:
            template = angle_ends[index] == ">"
        if token in pairs and (token != "<" or template):
            if token == "{" and not groups:
                initialized = True
            groups.append(pairs[token])
        elif groups and token == groups[-1]:
            groups.pop()
    if name is not None and not local and (initialized or not external):
        names.append(name)
    return names


def _source_data_definitions_joined(text):
    if NID_POSTFIX not in text or "extern" not in text:
        return
    scopes, groups, statement = [False], [], []
    initializer, has_postfix = False, False
    pairs = {"(": ")", "[": "]", "{": "}"}
    pos, length = 0, len(text)
    search_token = CPP_TOKEN.search
    while pos < length:
        match = search_token(text, pos)
        if match is None:
            break
        pos = match.end()
        if match.group("comment") is not None:
            continue
        token = match.group()
        if token == "#" and not text[text.rfind("\n", 0, match.start()) + 1:match.start()].strip():
            end = text.find("\n", pos)
            pos = length if end < 0 else end
            continue
        if token == "{" and not groups:
            if statement in (["extern", '"C"'], ["extern", '"C++"']):
                scopes.append(statement[1] == '"C"')
                statement, initializer, has_postfix = [], False, False
                continue
            if statement[:1] == ["namespace"] or statement[:2] == ["inline", "namespace"]:
                if statement[-1] != "namespace":
                    scopes.append(scopes[-1])
                    statement, initializer, has_postfix = [], False, False
                    continue
            elif initializer or (statement and ((_IDENT_W.fullmatch(statement[-1]) and ")" not in statement) or statement[-1] == "]") and
                    statement[-1] not in {"noexcept", "override", "final"} and
                    statement[0] not in {"struct", "class", "union", "enum"}) or \
                    (has_postfix and data_declarations(statement + ["{", "}"], scopes[-1])):
                statement.append(token)
                groups.append("}")
                continue
            statement, initializer, has_postfix = [], False, False
            pos = _skip_ignored_scope(text, pos)
        elif token == "}" and not groups:
            if len(scopes) > 1:
                scopes.pop()
            statement, initializer, has_postfix = [], False, False
        elif token == ";" and not groups:
            if has_postfix:
                yield from data_declarations(statement, scopes[-1])
            statement, initializer, has_postfix = [], False, False
        else:
            statement.append(token)
            if not has_postfix and token.endswith(NID_POSTFIX):
                has_postfix = True
            if token == "=" and not groups:
                initializer = True
            if token in pairs:
                groups.append(pairs[token])
            elif groups and token == groups[-1]:
                groups.pop()


def source_data_definitions(text):
    return _source_data_definitions_joined(_join_continuations(text))


def collect_unknowns():
    found = {}
    for path in source_files(PRX):
        text = path.read_text(encoding="utf-8")
        for nid, name in source_aliases(text):
            if "unknown" in name.lower():
                found.setdefault(nid, "%s:%s" % (path.relative_to(ROOT).as_posix(), name))
    return found


def collect_real_names():
    names = set()
    for path in source_files(ROOT / "core" / "libs"):
        text = _join_continuations(path.read_text(encoding="utf-8"))
        for match in _source_definitions_joined(text):
            name = match.group(1)
            names.add(name)
            names.add(strip_nid_postfix(name))
        for name in _source_assembly_definitions_joined(text):
            names.add(name)
            names.add(strip_nid_postfix(name))
        for name in _source_data_definitions_joined(text):
            names.add(name)
            names.add(strip_nid_postfix(name))
    return names


def source_locations(nids):
    matches = []
    for path in source_files(PRX):
        text = _join_continuations(path.read_text(encoding="utf-8"))
        exports = set(_source_aliases_joined(text))
        names = [match.group(1) for match in _source_definitions_joined(text)]
        names.extend(_source_assembly_definitions_joined(text))
        names.extend(_source_data_definitions_joined(text))
        for name in names:
            canonical = strip_nid_postfix(name)
            if not canonical.endswith(("_nid_no_patch", "_nid_no_patch_cut")):
                exports.add((compute_nid(canonical), name))
        for nid, name in sorted(exports):
            if nid in nids:
                matches.append({"nid": nid, "name": name, "path": path.relative_to(ROOT).as_posix()})
    return matches


def option_name(argument, options):
    if not argument.startswith("-") or argument == "-":
        return None
    name = argument.split("=", 1)[0]
    if name in options:
        return name
    if not name.startswith("--"):
        return None
    matches = [option for option in options if option.startswith(name)]
    if len(matches) == 1:
        return matches[0]
    return name if matches else None


def nid_arguments(arguments, options):
    normalized = []
    collecting = False
    collecting_option = "--nid"
    for index, argument in enumerate(arguments):
        if argument == "--":
            normalized.extend(arguments[index:])
            break
        name = option_name(argument, options)
        if name in ("--nid", "--name", "--symbol") and "=" not in argument:
            collecting = True
            collecting_option = "--name" if name in ("--name", "--symbol") else "--nid"
            continue
        if collecting and name is None:
            normalized.append(collecting_option + "=" + argument)
            continue
        collecting = False
        normalized.append(argument)
    return normalized


def symbol_file(path):
    if path == "-":
        text = sys.stdin.buffer.read().decode("utf-8-sig") if hasattr(sys.stdin, "buffer") else sys.stdin.read().removeprefix("\ufeff")
    else:
        try:
            text = Path(path).read_text(encoding="utf-8-sig")
        except UnicodeDecodeError as error:
            raise ValueError(f"{path}: {error}") from error
    return [line for raw in text.split("\n") if (line := raw.removesuffix("\r")).strip()]


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(
        description="Suggest real names for Unknown NID stubs")
    parser.add_argument("--db", default=os.environ.get("ANYPS5_NID_DB"),
                        help="NID database CSV; defaults to $ANYPS5_NID_DB, then to a copy downloaded to a cache dir")
    parser.add_argument("--nid", action="append", default=None,
                        help="resolve only these NIDs instead of scanning the tree")
    parser.add_argument("--name", "--symbol", dest="name", action="append", default=None,
                        help="compute NIDs for these symbol names")
    parser.add_argument("--from-file", action="append", default=[], metavar="PATH",
                        help="append symbol names from a UTF-8 file, one nonempty line per name; repeat or use - for stdin")
    parser.add_argument("--where", action="store_true",
                        help="find source exports matching requested NIDs or symbol names")
    parser.add_argument("--json", action="store_true",
                        help="emit machine-readable JSON")
    parser.add_argument("--offline", action="store_true", help="require an existing database; never download a missing cache")
    options = [option for action in parser._actions for option in action.option_strings]
    arguments = sys.argv[1:] if argv is None else argv
    args = parser.parse_args(nid_arguments(arguments, options))
    requested_options = {option_name(argument, options) for argument in arguments}
    requested_nids = "--nid" in requested_options
    if requested_nids and requested_options & {"--name", "--symbol"}:
        parser.error("--name/--symbol cannot be combined with --nid")
    if requested_nids and "--from-file" in requested_options:
        parser.error("--from-file cannot be combined with --nid")
    if args.where:
        if not requested_options & {"--nid", "--name", "--symbol", "--from-file"}:
            parser.error("--where requires --nid, --name, or --from-file")
        try:
            names = [*(args.name or []), *(name for path in args.from_file for name in symbol_file(path))]
            nids = {nid.split("#", 1)[0] for nid in args.nid or []}
            nids.update(compute_nid(name) for name in names)
            if not nids:
                parser.error("--where requires at least one NID or symbol name")
            for nid in nids:
                if re.fullmatch(r"[A-Za-z0-9+\-]{11}", nid) is None:
                    parser.error(f"invalid NID {nid!r}: expected 11 characters from A-Z, a-z, 0-9, + or -")
            rows = source_locations(nids)
        except (OSError, UnicodeError) as error:
            parser.error(str(error))
        if args.json:
            print(json.dumps({"locations": rows}, indent=1))
        else:
            print("%-12s %-40s %s" % ("NID", "SOURCE NAME", "PATH"))
            for row in rows:
                print("%-12s %-40s %s" % (row["nid"], row["name"], row["path"]))
        return 0
    if requested_options & {"--name", "--symbol", "--from-file"}:
        try:
            names = [*(args.name or []), *(name for path in args.from_file for name in symbol_file(path))]
        except (OSError, ValueError) as error:
            parser.error(str(error))
        rows = [{"name": name, "nid": compute_nid(name)} for name in names]
        if args.json:
            print(json.dumps({"symbols": rows}, indent=1))
        else:
            print("%-40s %s" % ("NAME", "NID"))
            for row in rows:
                print("%-40s %s" % (row["name"], row["nid"]))
        return 0
    args.nid = [nid.split("#", 1)[0] for nid in args.nid or []]
    for nid in args.nid:
        if re.fullmatch(r"[A-Za-z0-9+\-]{11}", nid) is None:
            parser.error(f"invalid NID {nid!r}: expected 11 characters from A-Z, a-z, 0-9, + or -")

    db_path = Path(args.db) if args.db else DEFAULT_CACHE
    if args.offline and not (requested_nids and not args.nid) and not db_path.is_file():
        parser.error(f"offline database is not a file: {db_path}")
    try:
        db = {} if requested_nids and not args.nid else load_db(db_path, offline=args.offline)
        if requested_nids:
            unknowns = {nid: "<cli>" for nid in (args.nid or [])}
        else:
            unknowns = collect_unknowns()
        real_names = collect_real_names() if unknowns else set()
    except (OSError, ValueError) as error:
        parser.error(str(error))

    rows = []
    failures = 0
    for nid in sorted(unknowns):
        suggestion = db.get(nid, "")
        status = "unknown"
        if suggestion:
            if compute_nid(suggestion) != nid:
                status = "MISMATCH"
                failures += 1
            elif suggestion in real_names:
                status = "name-in-use"
            else:
                status = "rename-ready"
        rows.append({"nid": nid, "stub": unknowns[nid],
                     "suggestion": suggestion, "status": status})

    if args.json:
        print(json.dumps({"db_entries": len(db), "rows": rows}, indent=1))
    else:
        print("%-12s %-40s %-12s %s" % ("NID", "STUB", "STATUS", "SUGGESTION"))
        for row in rows:
            print("%-12s %-40s %-12s %s" % (
                row["nid"], row["stub"][:40], row["status"], row["suggestion"]))
        print("%d unknowns, %d suggestions (%d names already in use)" % (
            len(rows), sum(1 for r in rows if r["suggestion"]),
            sum(1 for r in rows if r["status"] == "name-in-use")))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
