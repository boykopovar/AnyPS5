import argparse
import functools
import json
import os
import re
from collections import Counter
from pathlib import Path

import progress

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
SAFE = {"GuestCopyIn_nid_no_patch": (0, 1), "GuestCopyOut_nid_no_patch": (0, 1), "CopyGuestString_nid_no_patch": (0,),
        "CopyGuestPath_nid_no_patch": (0,), "CopyGuestEnvironment_nid_no_patch": (0,)}
UNEVALUATED = ("sizeof", "alignof", "decltype", "noexcept")
RAW_CALLS = {
    "memcpy": (0, 1), "memmove": (0, 1), "memset": (0,), "memcmp": (0, 1),
    "strcpy": (0, 1), "strncpy": (0, 1), "strlen": (0,), "strnlen": (0,), "strcmp": (0, 1), "strncmp": (0, 1),
    "snprintf": (0, 2), "sprintf": (0, 1), "copy": (0, 1, 2), "copy_n": (0, 2), "fill": (0, 1), "fill_n": (0,),
}
PARAMETER = re.compile(r"([A-Za-z_][\w:<>,\s]*?(?:[*&]+\s*(?:const\b\s*)?)+)\s*(\w+)\s*$")
RAW_CALL = re.compile(rf"\b(?:std::|::)?({'|'.join(RAW_CALLS)})\s*\(")
QUIET_CALL = re.compile(rf"\b({'|'.join((*SAFE, *UNEVALUATED))})\s*\(")
CAST = re.compile(r"^(?:\(\s*[\w\s:*<>]+\)(?=\s*\S)|(?:reinterpret|static|const)_cast\s*<[^>]*>|\()\s*")
CAST_TYPE = r"<(?:[^<>]|<(?:[^<>]|<[^<>]*>)*>)*>"

_SAFE_CALL = re.compile(rf"\b({'|'.join(SAFE)})\s*\(")
_UNEVALUATED_CALL = re.compile(rf"\b({'|'.join(UNEVALUATED)})\s*\(")
_ANON_NAMESPACE_AT = re.compile(r"namespace\s*\{")
_DEFINITION_PATTERN = r"\bAPS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{"
_DEFINITION_AT = re.compile(r"APS5_VABI\s+(\w+)\s*\([^;{]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{")
_WORD_CHAR = re.compile(r"\w")
_STATIC_KEYWORD = re.compile(r"\bstatic\b")
_IDENTIFIER = re.compile(r"\w+")
_LEADING_NAME = re.compile(r"\(?\s*(\w+)\b")
_OPERAND = rf"(?<![.>:])(?:\b(?:reinterpret|static|const)_cast\s*{CAST_TYPE}\s*\(\s*(?P<cast>\w+)\s*\)|\b(?P<direct>\w+)\b)"
_ACCESS_PATTERNS = (
    ("member", re.compile(rf"{_OPERAND}\s*->")),
    ("index", re.compile(rf"{_OPERAND}\s*\[")),
    ("dereference", re.compile(rf"(?:^|[=(,;{{}}!&|?:+\-]|\breturn)\s*\*\s*{_OPERAND}")),
)
_SIMPLE_ACCESS_PATTERNS = (
    ("member", "->", re.compile(r"(?<![.>:])\b(?P<direct>\w+)\b\s*->")),
    ("index", "[", re.compile(r"(?<![.>:])\b(?P<direct>\w+)\b\s*\[")),
    ("dereference", "*", re.compile(r"(?:^|[=(,;{}!&|?:+\-]|\breturn)\s*\*\s*(?<![.>:])\b(?P<direct>\w+)\b")),
)
_DEREF_PATTERN = _ACCESS_PATTERNS[2][1]
_SIMPLE_DEREF_PATTERN = _SIMPLE_ACCESS_PATTERNS[2][2]
_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))


def split_top_level(text, angles=True):
    opening, closing_brackets = ("(<[{", ")>]}") if angles else ("([{", ")]}")
    parts, depth, start = [], 0, 0
    for index, character in enumerate(text):
        if character in opening:
            depth += 1
        elif character in closing_brackets:
            depth -= 1
        elif character == "," and depth == 0:
            parts.append((start, text[start:index]))
            start = index + 1
    parts.append((start, text[start:]))
    return parts


def closing(text, open_paren):
    depth = 0
    for index in range(open_paren, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    return len(text) - 1


def pointer_parameters(signature):
    if "*" not in signature:
        return []
    names = []
    for _, part in split_top_level(signature):
        part = part.split("=")[0].strip()
        if "(" in part or "&" in part:
            continue
        match = PARAMETER.match(part)
        if match and "*" in match.group(1):
            names.append(match.group(2))
    return names


def quiet(body, safe=True):
    if "sizeof" not in body and "alignof" not in body and "decltype" not in body and "noexcept" not in body and (not safe or "Guest" not in body):
        return body
    pattern = QUIET_CALL if safe else _UNEVALUATED_CALL
    first = pattern.search(body)
    if first is None:
        return body
    blanked = list(body)
    for match in pattern.finditer(body, first.start()):
        name = match.group(1)
        open_paren = match.end() - 1
        end = closing(body, open_paren)
        spans = [(open_paren + 1, end)]
        if safe and name in SAFE:
            arguments = split_top_level(body[open_paren + 1:end], angles=False)
            spans = [(open_paren + 1 + offset, open_paren + 1 + offset + len(text))
                     for position, (offset, text) in enumerate(arguments) if position in SAFE[name]]
        for start, stop in spans:
            for index in range(start, stop):
                if blanked[index] != "\n":
                    blanked[index] = " "
    return "".join(blanked)


def _argument_names(argument):
    argument = argument.strip()
    found = set()
    while True:
        match = _LEADING_NAME.match(argument)
        if match is not None:
            found.add(match.group(1))
        stripped = CAST.sub("", argument, count=1)
        if stripped == argument:
            return found
        argument = stripped


def _scan_pattern_for_names(body, kind, pattern, names, found_by_name):
    pos = 0
    while True:
        match = pattern.search(body, pos)
        if match is None:
            break
        cast_name = match.group("cast")
        matched_name = cast_name if cast_name is not None else match.group("direct")
        if matched_name in names:
            found_by_name[matched_name].append((match.end() - 1, kind))
        if cast_name is not None:
            inner = body[match.start():match.start("cast")]
            if kind == "member":
                nested = "->" in inner
            elif kind == "index":
                nested = "[" in inner
            else:
                angle = inner.find("<")
                nested = angle != -1 and "*" in inner[angle:]
            pos = match.start() + 1 if nested else match.end()
        elif kind == "dereference" and matched_name == "return":
            pos = match.start("direct")
        else:
            pos = match.end()


def _scan_simple_pattern_for_names(body, kind, pattern, names, found_by_name):
    if kind != "dereference":
        for match in pattern.finditer(body):
            matched_name = match.group(1)
            if matched_name in names:
                found_by_name[matched_name].append((match.end() - 1, kind))
        return
    pos = 0
    while True:
        match = pattern.search(body, pos)
        if match is None:
            break
        matched_name = match.group(1)
        if matched_name in names:
            found_by_name[matched_name].append((match.end() - 1, kind))
        pos = match.start(1) if matched_name == "return" else match.end()


def _all_accesses(body, names):
    found_by_name = {name: [] for name in names}
    if "_cast" in body:
        for (kind, pattern), (_, needle, _) in zip(_ACCESS_PATTERNS, _SIMPLE_ACCESS_PATTERNS):
            if needle in body:
                _scan_pattern_for_names(body, kind, pattern, names, found_by_name)
    else:
        for kind, needle, pattern in _SIMPLE_ACCESS_PATTERNS:
            if needle in body:
                _scan_simple_pattern_for_names(body, kind, pattern, names, found_by_name)
    if "(" in body:
        for match in RAW_CALL.finditer(body):
            open_paren = match.end() - 1
            arguments = split_top_level(body[open_paren + 1:closing(body, open_paren)], angles=False)
            for position in RAW_CALLS[match.group(1)]:
                if position < len(arguments):
                    offset, argument = arguments[position]
                    matched = _argument_names(argument) & names
                    if matched:
                        hit_pos = open_paren + 1 + offset
                        for name in matched:
                            found_by_name[name].append((hit_pos, "library call"))
    return found_by_name


def _all_checked_argument_dereferences(body, names):
    found_by_name = {name: [] for name in names}
    for match in _SAFE_CALL.finditer(body):
        open_paren = match.end() - 1
        arguments = split_top_level(body[open_paren + 1:closing(body, open_paren)], angles=False)
        for position in SAFE[match.group(1)]:
            if position < len(arguments):
                offset, argument = arguments[position]
                if "*" not in argument:
                    continue
                active = {name for name in names if name in argument}
                if not active:
                    continue
                cleaned = quiet(argument)
                per_arg = {name: [] for name in active}
                if "_cast" in cleaned:
                    _scan_pattern_for_names(cleaned, "dereference", _DEREF_PATTERN, active, per_arg)
                else:
                    _scan_simple_pattern_for_names(cleaned, "dereference", _SIMPLE_DEREF_PATTERN, active, per_arg)
                base = open_paren + 1 + offset
                for name, hits in per_arg.items():
                    if hits:
                        found_by_name[name].extend((base + index, kind) for index, kind in hits)
    return found_by_name


def checked_argument_dereferences(body, name):
    if _IDENTIFIER.fullmatch(name):
        return _all_checked_argument_dereferences(body, {name})[name]
    found = []
    for match in _SAFE_CALL.finditer(body):
        open_paren = match.end() - 1
        arguments = split_top_level(body[open_paren + 1:closing(body, open_paren)], angles=False)
        for position in SAFE[match.group(1)]:
            if position < len(arguments):
                offset, argument = arguments[position]
                found.extend((open_paren + 1 + offset + index, kind)
                             for index, kind in accesses(quiet(argument), name) if kind == "dereference")
    return found


@functools.lru_cache(maxsize=None)
def parameter_patterns(name):
    word = re.escape(name)
    operand = rf"(?<![.>:])(?:\b{word}\b|\b(?:reinterpret|static|const)_cast\s*{CAST_TYPE}\s*\(\s*{word}\s*\))"
    found = tuple((kind, re.compile(pattern)) for kind, pattern in (
        ("member", rf"{operand}\s*->"), ("index", rf"{operand}\s*\["),
        ("dereference", rf"(?:^|[=(,;{{}}!&|?:+\-]|\breturn)\s*\*\s*{operand}")))
    return re.compile(rf"\(?\s*{word}\b"), found


def names_parameter(argument, name):
    if _IDENTIFIER.fullmatch(name):
        return name in _argument_names(argument)
    argument = argument.strip()
    pattern = parameter_patterns(name)[0]
    while True:
        if pattern.match(argument):
            return True
        stripped = CAST.sub("", argument, count=1)
        if stripped == argument:
            return False
        argument = stripped


def accesses(body, name):
    if _IDENTIFIER.fullmatch(name):
        return sorted(_all_accesses(body, {name})[name])
    found = []
    for kind, pattern in parameter_patterns(name)[1]:
        found += [(match.end() - 1, kind) for match in pattern.finditer(body)]
    for match in RAW_CALL.finditer(body):
        open_paren = match.end() - 1
        arguments = split_top_level(body[open_paren + 1:closing(body, open_paren)], angles=False)
        for position in RAW_CALLS[match.group(1)]:
            if position < len(arguments) and names_parameter(arguments[position][1], name):
                found.append((open_paren + 1 + arguments[position][0], "library call"))
    return sorted(found)


def splice_lines(text):
    if "\r" in text:
        text = text.replace("\r\n", "\n")
    if "\\\n" not in text and not text.endswith("\\"):
        return text
    lines = []
    pending = ""
    joined = 0
    for line in text.split("\n"):
        if line.endswith("\\"):
            pending += line[:-1]
            joined += 1
            continue
        lines.append(pending + line + "\n" * joined)
        pending = ""
        joined = 0
    if joined:
        lines.append(pending + "\n" * joined)
    return "\n".join(lines)


def keep_lines(match):
    if match.group("code") is not None:
        return match.group()
    if match.group("comment") is None:
        return "LITERAL" + "\n" * match.group().count("\n")
    return re.sub(r"[^\n]", " ", match.group())


def _token_before(text, index, floor):
    scan_start = index
    while scan_start > floor and text[scan_start - 1] in _PRE_TOKEN_CHARS:
        scan_start -= 1
    if scan_start == index:
        return None, None, index
    last_start = index
    last_token = None
    for token in _CODE_TOKEN.finditer(text, scan_start):
        if token.start() >= index:
            break
        if token.end() > index:
            return token.start(), token.group(), token.end()
        last_start = token.start()
        last_token = token.group()
        if token.end() == index:
            return last_start, last_token, index
    return None, None, index


def _blank_comment(text, start, end):
    if "\n" not in text[start:end]:
        return " " * (end - start)
    return "\n".join(" " * len(line) for line in text[start:end].split("\n"))


def source_code(text):
    text = splice_lines(text)
    out = []
    pos = 0
    cursor = 0
    floor = 0
    no_block_close = False
    length = len(text)
    while cursor < length:
        trigger = _NON_CODE_TRIGGER.search(text, cursor)
        if trigger is None:
            break
        index = trigger.start()
        token = trigger.group()
        if token == "//":
            end = text.find("\n", index + 2)
            if end == -1:
                end = length
            out.append(text[pos:index])
            out.append(" " * (end - index))
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
            out.append(_blank_comment(text, index, end))
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
                    out.append(text[pos:tok_start])
                    out.append("LITERAL" + "\n" * text.count("\n", index, end))
                    pos = cursor = floor = end
                    continue
            str_match = _STRING_BODY.match(text, index)
            if str_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = str_match.end()
                out.append(text[pos:start])
                out.append("LITERAL" + "\n" * text.count("\n", index, end))
                pos = cursor = floor = end
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
                out.append(text[pos:start])
                out.append("LITERAL" + "\n" * text.count("\n", index, end))
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def anonymous_spans(text):
    spans = []
    pos = 0
    while True:
        index = text.find("namespace", pos)
        if index == -1:
            return spans
        if index == 0 or _WORD_CHAR.match(text, index - 1) is None:
            match = _ANON_NAMESPACE_AT.match(text, index)
            if match is not None:
                brace = match.end() - 1
                spans.append((brace, progress.body_end(text, brace)))
                pos = match.end()
                continue
        pos = index + 9


def internal_linkage(text, start, spans):
    for open_brace, close in spans:
        if open_brace < start < close:
            return True
    boundary = text.rfind(";", 0, start)
    pos = text.rfind("{", boundary + 1, start)
    if pos > boundary:
        boundary = pos
    pos = text.rfind("}", boundary + 1, start)
    if pos > boundary:
        boundary = pos
    if "static" not in text[boundary + 1:start]:
        return False
    return _STATIC_KEYWORD.search(text, boundary + 1, start) is not None


def _iter_definitions(text):
    if progress.DEFINITION.pattern != _DEFINITION_PATTERN:
        yield from progress.DEFINITION.finditer(text)
        return
    pos = 0
    while True:
        index = text.find("APS5_VABI", pos)
        if index == -1:
            return
        if index == 0 or _WORD_CHAR.match(text, index - 1) is None:
            match = _DEFINITION_AT.match(text, index)
            if match is not None:
                yield match
                pos = match.end()
                continue
        pos = index + 9


def audit_text(text):
    results = []
    spans = anonymous_spans(text)
    line_pos = 0
    line_base = 1
    for match in _iter_definitions(text):
        if internal_linkage(text, match.start(), spans):
            continue
        open_paren = text.index("(", match.start(1))
        close_paren = text.rindex(")", 0, match.end())
        parameters = pointer_parameters(text[open_paren + 1:close_paren])
        if not parameters:
            continue
        body_start = match.end() - 1
        original_body = text[body_start:progress.body_end(text, body_start) + 1]
        active = [parameter for parameter in parameters if parameter in original_body]
        if not active:
            continue
        active_set = set(active)
        body = quiet(original_body)
        by_param = _all_accesses(body, active_set)
        if "Guest" in original_body and _SAFE_CALL.search(original_body) is not None:
            evaluated_body = quiet(original_body, safe=False)
            checked = _all_checked_argument_dereferences(evaluated_body, active_set)
        else:
            checked = None
        line_base += text.count("\n", line_pos, body_start)
        line_pos = body_start
        for parameter in active:
            hits = by_param[parameter]
            if checked is not None and checked[parameter]:
                hits = sorted(hits + checked[parameter])
            elif len(hits) > 1:
                hits = sorted(hits)
            if hits:
                line = line_base + original_body.count("\n", 0, hits[0][0])
                results.append({"function": match.group(1), "parameter": parameter, "line": line, "kinds": sorted({kind for _, kind in hits})})
    return results


def _library_sources(library):
    sources = []
    for root, dirs, files in os.walk(library):
        dirs[:] = [name for name in dirs if name != "tests"]
        root_path = Path(root)
        for name in files:
            if name.endswith(".cpp"):
                sources.append(root_path / name)
    sources.sort()
    return sources


def audit(libraries):
    report = []
    for library in libraries:
        for source in _library_sources(library):
            raw = source.read_text(encoding="utf-8", errors="ignore")
            if "APS5_VABI" not in raw and "\\\n" not in raw and "\\\r\n" not in raw:
                continue
            text = source_code(raw)
            for entry in audit_text(text):
                report.append({"library": library.name, "file": source.relative_to(ROOT).as_posix(), **entry})
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description="List exported functions that access a guest pointer parameter directly instead of through GuestCopyIn, GuestCopyOut or the CopyGuest* helpers. A report, not a gate: it always exits 0 and does not follow a pointer passed to another function. Definitions with internal linkage are callbacks rather than exports and are left out.")
    parser.add_argument("libraries", nargs="*", help="library directories under core/libs/prx (default: all)")
    parser.add_argument("--json", action="store_true", help="print the findings as JSON")
    args = parser.parse_args(argv)
    available = {path.name for path in PRX.iterdir() if path.is_dir() and not path.is_symlink()}
    names = args.libraries or sorted(available)
    missing = [name for name in names if name not in available]
    if missing:
        parser.error(f"not a library under core/libs/prx: {', '.join(missing)}")
    report = audit([PRX / name for name in names])
    if args.json:
        print(json.dumps(report, indent=2))
        return 0
    for entry in report:
        print(f"{entry['file']}:{entry['line']}: {entry['function']}: parameter {entry['parameter']} ({', '.join(entry['kinds'])})")
    counts = Counter(entry["library"] for entry in report)
    print(f"{len(report)} direct guest pointer accesses in {len({(e['file'], e['function']) for e in report})} exported functions")
    for library, count in counts.most_common():
        print(f"  {count:5} {library}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
