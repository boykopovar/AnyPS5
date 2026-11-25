#!/usr/bin/env python3
"""Report the console system library exports an AnyPS5 library does not provide.

Reads the export NIDs of the console library from its ELF, a genstub style
export table or a file of NIDs, intersects them with the pinned NID name database and
with what the AnyPS5 library already defines, and writes the missing NIDs in
the format tools/stubgen.py --nids takes.
"""
import argparse
from bisect import bisect_right
import os
import re
import sys
from pathlib import Path

import import_audit
import nid_names
import progress

ROOT = Path(__file__).resolve().parent.parent
PRX = ROOT / "core" / "libs" / "prx"
POSTFIXES = ("_nid_postfix", "_nid_no_patch_cut", "_nid_no_patch")
DISAMBIG = "_nid_disambig"
NID_PATTERN = "[A-Za-z0-9+\\-]{11}"
NID_RE = re.compile(NID_PATTERN)
QUOTED_NID_RE = re.compile(r'"(' + NID_PATTERN + r')"')
VABI = re.compile(r"\bAPS5_VABI\s+(\w+)\s*\(")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]*\([ \t]*(\w+)[ \t]*\)([^\n]*)", re.M)
DIRECTIVE_TAIL = r'(?://[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|[^\n])*'
DIRECTIVE_TAIL_RE = re.compile(DIRECTIVE_TAIL)
DIRECTIVE_TOKEN = re.compile(r'(?P<directive>^[ \t]*#' + DIRECTIVE_TAIL + r')|' + nid_names.CPP_TOKEN.pattern, re.M)
NON_DEFINE_DIRECTIVE_TOKEN = re.compile(r'(?P<directive>^[ \t]*#(?![ \t]*define\b)' + DIRECTIVE_TAIL + r')|' + nid_names.CPP_TOKEN.pattern, re.M)
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
MODULE_PATTERN = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_.-]*\Z")
_MANGLED_NESTED = re.compile(r"^_ZN[KVrRO]*(?:[0-9]|St[0-9]|S[absiod])")
_SOURCE_SUFFIXES = frozenset((".cpp", ".c", ".cc", ".h", ".hpp", ".hh"))
_TEST_DIRS = frozenset(("test", "tests"))
_SPLICE_RE = re.compile(r"\\\r?\n")
_NON_NEWLINE = re.compile(r"[^\n]")
_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_DIRECTIVE_TRIGGER = re.compile(r'^[ \t]*#|//|/\*|"|\'', re.M)
_NON_DEFINE_TRIGGER = re.compile(r'^[ \t]*#(?![ \t]*define\b)|//|/\*|"|\'', re.M)
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
_ASM_MACROS = tuple(sorted(nid_names.ASM_FUNCTION_MACROS))
_DATA_NID_RE = re.compile(r"\b[A-Za-z_]\w*_nid_postfix\b")
_WS_RE = re.compile(r"\s*")
_DIRECT_INIT_WORDS = frozenset(("true", "false", "nullptr"))


def _blank_span(span):
    if "\n" not in span:
        return " " * len(span)
    return "\n".join(" " * len(line) for line in span.split("\n"))


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


def _mask_fast(text, trigger_re):
    out = []
    pos = 0
    cursor = 0
    floor = 0
    no_block_close = False
    length = len(text)
    while cursor < length:
        trigger = trigger_re.search(text, cursor)
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
            out.append(_blank_span(text[index:end]))
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
                    out.append(_blank_span(text[tok_start:end]))
                    pos = cursor = floor = end
                    continue
            str_match = _STRING_BODY.match(text, index)
            if str_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = str_match.end()
                out.append(text[pos:start])
                out.append(_blank_span(text[start:end]))
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
        elif token == "'":
            tok_start, tok_text, tok_end = _token_before(text, index, floor)
            if tok_end > index:
                cursor = floor = tok_end
                continue
            char_match = _CHAR_BODY.match(text, index)
            if char_match is not None:
                start = tok_start if tok_text in _STRING_PREFIXES else index
                end = char_match.end()
                out.append(text[pos:start])
                out.append(_blank_span(text[start:end]))
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
        else:
            tail = DIRECTIVE_TAIL_RE.match(text, trigger.end())
            end = tail.end() if tail is not None else trigger.end()
            out.append(text[pos:index])
            out.append(_blank_span(text[index:end]))
            pos = cursor = floor = end
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def is_mangled(name):
    return (name.startswith("_Z") and len(name) > 2 and name[2].isdigit()) or bool(_MANGLED_NESTED.match(name))


def helper_reason(name):
    if is_mangled(name):
        return "is a C++ mangled name"
    if IDENTIFIER.match(name) is None:
        return "is not a valid C identifier"
    return None


def is_safe_module(name):
    if not name or name in (".", ".."):
        return False
    if "/" in name or "\\" in name:
        return False
    if ".." in name:
        return False
    return MODULE_PATTERN.match(name) is not None


class GapError(Exception):
    pass


def strip_token(raw):
    text = raw.lstrip("\ufeff").strip()
    if not text:
        return ""
    if text.startswith("#"):
        return ""
    if "#" in text:
        text = text.split("#", 1)[0].strip()
    if not text:
        return ""
    return text.split()[0]


def parse_nids_file(path):
    nids = []
    seen = set()
    try:
        content = Path(path).read_text(encoding="utf-8-sig", errors="replace")
    except OSError as error:
        raise GapError(f"{path}: cannot read NID file: {error.strerror or error}") from error
    for number, line in enumerate(content.split("\n"), 1):
        token = strip_token(line)
        if not token:
            continue
        if NID_RE.fullmatch(token) is None:
            raise GapError(f"{path}:{number}: invalid NID {token!r}")
        if token not in seen:
            seen.add(token)
            nids.append(token)
    return nids


def parse_export_table(path):
    try:
        text = Path(path).read_text(encoding="utf-8-sig", errors="replace")
    except OSError as error:
        raise GapError(f"{path}: cannot read export table: {error.strerror or error}") from error
    text = joined_lines(text)
    if "sprx_dlsym" not in text:
        return []
    nids = []
    seen = set()
    tokens = []
    directive_end = 0
    for token in nid_names.CPP_TOKEN.finditer(text):
        if token.start() < directive_end or token.group("comment") is not None:
            continue
        if (token.group("code") is not None and token.group() == "#"
                and not text[text.rfind("\n", 0, token.start()) + 1:token.start()].strip()):
            end = text.find("\n", token.end())
            directive_end = len(text) if end < 0 else end
            continue
        tokens.append(token)
    for index, token in enumerate(tokens[:-4]):
        if token.group("code") is None or token.group() != "sprx_dlsym":
            continue
        opening, handle, comma, literal = tokens[index + 1:index + 5]
        if (opening.group() != "(" or handle.group() != "__handle" or comma.group() != ","
                or literal.group("string") is None):
            continue
        match = QUOTED_NID_RE.fullmatch(literal.group())
        if match is None:
            continue
        nid = match.group(1)
        if nid not in seen:
            seen.add(nid)
            nids.append(nid)
    return nids


def mask_code(text, token=nid_names.CPP_TOKEN):
    if token is nid_names.CPP_TOKEN:
        return _mask_fast(text, _NON_CODE_TRIGGER)
    if token is DIRECTIVE_TOKEN:
        return _mask_fast(text, _DIRECTIVE_TRIGGER)
    if token is NON_DEFINE_DIRECTIVE_TOKEN:
        return _mask_fast(text, _NON_DEFINE_TRIGGER)

    def blank(match):
        if match.group("code") is not None:
            return match.group()
        return _NON_NEWLINE.sub(" ", match.group())

    return token.sub(blank, text)


def mask_directives(text):
    if "#" not in text:
        return text
    lines = []
    continued = False
    for line in text.splitlines(keepends=True):
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip("\r\n").endswith("\\")
        if directive:
            if line.endswith("\n"):
                lines.append(" " * (len(line) - 1) + "\n")
            else:
                lines.append(" " * len(line))
        else:
            lines.append(line)
    return "".join(lines)


def joined_lines(text):
    if "\\\r\n" in text:
        text = text.replace("\\\r\n", "")
    if "\\\n" in text:
        text = text.replace("\\\n", "")
    return text


def base_of(name):
    for postfix in POSTFIXES:
        if name.endswith(postfix) and len(name) > len(postfix):
            return name[: -len(postfix)], postfix
    return name, None


def strip_disambig(name):
    pos = name.rfind(DISAMBIG)
    if pos == -1:
        return name
    tail = name[pos + len(DISAMBIG):]
    if tail and tail.isascii() and tail.isdigit():
        return name[:pos]
    return name


def source_texts(module_dir):
    paths = []
    for root, dirs, files in os.walk(module_dir):
        dirs[:] = [name for name in dirs if name.lower() not in _TEST_DIRS]
        root_path = Path(root)
        for filename in files:
            if "." in filename:
                path = root_path / filename
                if path.suffix.lower() in _SOURCE_SUFFIXES:
                    paths.append(path)
    paths.sort()
    texts = []
    for path in paths:
        try:
            texts.append((path, path.read_text(encoding="utf-8-sig", errors="replace")))
        except OSError as error:
            raise GapError(f"{path}: cannot read source: {error.strerror or error}") from error
    return texts


def same_file(left, right):
    left, right = Path(left), Path(right)
    return left.resolve() == right.resolve() or (left.exists() and right.exists() and left.samefile(right))


def _macro_definitions_joined(joined_texts):
    macros = {}
    for joined in joined_texts:
        if "#" not in joined or "define" not in joined or "APS5_VABI" not in joined:
            continue
        masked = _mask_fast(joined, _NON_DEFINE_TRIGGER)
        for match in DEFINE.finditer(masked):
            macro, param, body = match.group(1), match.group(2), match.group(3)
            if "APS5_VABI" in body and any(definition.group(1) == param for definition in nid_names.DEFINITION.finditer(body)):
                macros.setdefault(macro, param)
    return macros


def macro_definitions(texts):
    return _macro_definitions_joined([joined_lines(text) for _, text in texts])


def record_provided(provided, name, origin=None):
    base, postfix = base_of(name)
    prefix = f"{origin} " if origin else ""
    if postfix == "_nid_no_patch_cut":
        provided.setdefault(base, f"{prefix}literally as {base} under {name}")
        return
    if postfix == "_nid_no_patch":
        provided.setdefault(name, f"{prefix}literally as {name}")
        return
    if postfix is not None and postfix != "_nid_postfix":
        return
    base = strip_disambig(base)
    nid = nid_names.compute_nid(base)
    if postfix is None:
        provided.setdefault(nid, f"{prefix}under its own name {name}")
    else:
        provided.setdefault(nid, f"{prefix}under {name} for {base}")


def _may_have_data_definition(joined):
    if nid_names.NID_POSTFIX not in joined or "extern" not in joined or '"C"' not in joined:
        return False
    length = len(joined)
    for match in _DATA_NID_RE.finditer(joined):
        pos = _WS_RE.match(joined, match.end()).end()
        if pos >= length or joined[pos] != "(":
            return True
        inner_pos = _WS_RE.match(joined, pos + 1).end()
        if inner_pos >= length:
            return True
        ch = joined[inner_pos]
        if ch in "+-!~(0123456789.\"'/#":
            return True
        word_match = _CODE_TOKEN.match(joined, inner_pos)
        if word_match is None:
            return True
        word = word_match.group()
        if word in _DIRECT_INIT_WORDS or (word in _RAW_PREFIXES or word in _STRING_PREFIXES):
            return True
    return False


def collect_source_exports(texts):
    joined_texts = [joined_lines(text) for _, text in texts]
    macros = _macro_definitions_joined(joined_texts)
    sorted_macros = sorted(macros)
    provided = {}
    masked_cache = {}
    for index, ((_, text), joined) in enumerate(zip(texts, joined_texts)):
        if "APS5_VABI" in joined:
            masked = mask_directives(_mask_fast(joined, _DIRECTIVE_TRIGGER))
            masked_cache[index] = masked
            for match in nid_names.DEFINITION.finditer(masked):
                record_provided(provided, match.group(1))
        if any(macro in joined for macro in _ASM_MACROS):
            for name in nid_names.source_assembly_definitions(text):
                record_provided(provided, name, "defined in assembly")
        if _may_have_data_definition(joined):
            for name in nid_names.source_data_definitions(text):
                record_provided(provided, name, "defined as data")
        if "APS5_EXPORT" in joined:
            for nid, helper in nid_names.source_aliases(text):
                if nid not in provided:
                    provided[nid] = f"bound with APS5_EXPORT as {helper}"
    for macro in sorted_macros:
        call = re.compile(r"\b" + re.escape(macro) + r"\s*\(\s*(?:::)?(?:\w+::)*(\w+)\s*\)")
        for index, joined in enumerate(joined_texts):
            if macro not in joined:
                continue
            masked = masked_cache.get(index)
            if masked is None:
                masked = mask_directives(_mask_fast(joined, _DIRECTIVE_TRIGGER))
                masked_cache[index] = masked
            for match in call.finditer(masked):
                symbol = match.group(1)
                base, postfix = base_of(symbol)
                if postfix == "_nid_no_patch_cut":
                    if base not in provided:
                        provided[base] = f"through macro {macro} literally as {base} under {symbol}"
                    continue
                if postfix == "_nid_no_patch":
                    if symbol not in provided:
                        provided[symbol] = f"through macro {macro} literally as {symbol}"
                    continue
                if postfix is not None and postfix != "_nid_postfix":
                    continue
                base = strip_disambig(base)
                nid = nid_names.compute_nid(base)
                if nid not in provided:
                    provided[nid] = f"through macro {macro} as {symbol}"
    return provided


def _collect_provided_from_texts(module_dir, texts):
    provided = collect_source_exports(texts)
    owners = progress.shared_source_owners(module_dir)
    if owners:
        declared = {}
        for _, text in texts:
            joined = joined_lines(text)
            if "APS5_VABI" not in joined:
                continue
            for match in VABI.finditer(_mask_fast(joined, _NON_CODE_TRIGGER)):
                record_provided(declared, match.group(1))
        for owner in owners:
            shared = collect_source_exports(source_texts(module_dir.parent / owner))
            for nid in declared.keys() & shared.keys():
                provided.setdefault(nid, f"from {owner}: {shared[nid]}")
    return provided


def collect_provided(module_dir):
    return _collect_provided_from_texts(module_dir, source_texts(module_dir))


def _find_self_calls_in_texts(texts, missing):
    if not missing:
        return []
    name_to_nid = {}
    for key, value in missing.items():
        name_to_nid.setdefault(value, key)
    names = sorted(name_to_nid)
    alts = "|".join(re.escape(name) for name in names)
    any_name = re.compile(alts)
    call = re.compile(r"(?<![\w:])(::)?\b(" + alts + r")\s*\(")
    calls = []
    for path, text in texts:
        joined = joined_lines(text)
        if any_name.search(joined) is None:
            continue
        masked = _mask_fast(joined, _NON_CODE_TRIGGER)
        matches = list(call.finditer(masked))
        if not matches:
            continue
        splices = []
        if "\\\n" in text or "\\\r\n" in text:
            removed = 0
            for splice in _SPLICE_RE.finditer(text):
                splices.append(splice.start() - removed)
                removed += len(splice.group())
        rows = joined.split("\n")
        try:
            rel = path.relative_to(ROOT).as_posix()
        except ValueError:
            rel = path.as_posix()
        declared = {}
        if "APS5_VABI" in masked:
            for match in VABI.finditer(masked):
                declared.setdefault(match.start(1), match.group(1))
        prev_pos = 0
        line_number = 1
        for match in matches:
            qual = match.group(1) or ""
            name = match.group(2)
            start = match.start(2)
            if declared.get(start) == name:
                continue
            mstart = match.start()
            line_number += masked.count("\n", prev_pos, mstart)
            prev_pos = mstart
            if rows[line_number - 1].lstrip().startswith("#"):
                continue
            nid = name_to_nid[name]
            extra = bisect_right(splices, mstart) if splices else 0
            calls.append((name, nid, rel, line_number + extra, qual))
    calls.sort(key=lambda item: (item[0], item[2], item[3], item[4]))
    return calls


def find_self_calls(module_dir, missing):
    if not missing:
        return []
    return _find_self_calls_in_texts(source_texts(module_dir), missing)


def parse_elf_nids(path):
    data = Path(path).read_bytes()
    if not data.startswith(b"\x7fELF"):
        raise GapError(f"{path}: expected a console module in ELF format")
    try:
        names = import_audit.elf_exports(path, data)
    except import_audit.AuditError as error:
        raise GapError(str(error)) from error
    nids = sorted(name for name in names if NID_RE.fullmatch(name))
    if not nids:
        raise GapError(f"{path}: no NID-shaped dynamic exports found")
    return nids


def load_console_nids(modules, export_table, nids_file, elf=None):
    if elf is not None:
        if len(modules) != 1:
            raise GapError("--elf requires a single --module")
        return {modules[0]: parse_elf_nids(elf)}
    if export_table is not None:
        table_dir = Path(export_table)
        if not table_dir.is_dir():
            raise GapError(f"{table_dir}: not a directory")
        console = {}
        for module in modules:
            table = table_dir / (module + ".c")
            if not table.is_file():
                raise GapError(f"{table}: export table not found")
            console[module] = parse_export_table(table)
        return console
    nids = parse_nids_file(nids_file)
    return {module: list(nids) for module in modules}


def report_module(module, console_nids, provided, db):
    missing = []
    already = []
    unknown = []
    for nid in console_nids:
        if nid in provided:
            already.append((nid, db.get(nid), provided[nid]))
        elif nid not in db:
            unknown.append(nid)
        else:
            missing.append((nid, db[nid]))
    lines = [f"{module}: {len(missing)} missing, {len(already)} already provided, {len(unknown)} unknown"]
    for nid, name in missing:
        reason = helper_reason(name)
        if reason is None:
            lines.append(f"missing {nid} {name}")
        else:
            action = "may use a renamed helper bound with APS5_EXPORT" if is_mangled(name) else "needs a renamed helper bound with APS5_EXPORT"
            lines.append(f"{module}: {name} ({nid}) {reason} and {action}, like libSceAmpr")
    for nid, name, how in already:
        if name is None:
            lines.append(f"already provided {nid} {how}")
        else:
            lines.append(f"already provided {nid} {name} {how}")
    for nid in unknown:
        lines.append(f"unknown {nid} not in database, left out")
    return missing, already, unknown, lines


def normalize_module(name):
    if name.lower().endswith(".prx") and len(name) > 4:
        return name[: -len(".prx")]
    return name


def main(argv=None):
    parser = argparse.ArgumentParser(description="report the console exports an AnyPS5 library does not provide")
    parser.add_argument("--module", action="append", default=[], help="AnyPS5 library to report on, e.g. libScePad; repeat for several")
    parser.add_argument("--export-table", default=None, help="directory of genstub style export tables, one <module>.c file per library")
    parser.add_argument("--nids", default=None, help="file with one export NID per line")
    parser.add_argument("--elf", help="read NID-shaped dynamic exports from a single console module ELF")
    parser.add_argument("--out", default=None, help="write the missing NIDs to this file; without it they go to standard output")
    parser.add_argument("--db", default=None, help="NID name database file; the pinned database is used by default")
    args = parser.parse_args(argv)
    try:
        modules = [normalize_module(name) for name in args.module]
        if not modules:
            raise GapError("--module is required")
        if sum(source is not None for source in (args.export_table, args.nids, args.elf)) != 1:
            raise GapError("exactly one of --export-table, --nids and --elf is required")
        for module in modules:
            if not is_safe_module(module):
                raise GapError(f"{module}: invalid module name")
            if not (PRX / module).is_dir():
                raise GapError(f"{PRX / module}: no such library")
        if args.out is not None:
            sources = [args.nids, args.elf, args.db]
            if args.export_table is not None:
                sources.extend(Path(args.export_table) / (module + ".c") for module in modules)
            for source in filter(None, sources):
                if same_file(args.out, source):
                    raise GapError(f"output {args.out!r} would overwrite input {source!r}")
        try:
            db = nid_names.load_db(Path(args.db) if args.db else nid_names.DEFAULT_CACHE)
        except (OSError, UnicodeError) as error:
            raise GapError(f"{args.db or nid_names.DEFAULT_CACHE}: cannot read NID database: {error}") from error
        except ValueError as error:
            raise GapError(str(error)) from error
        console = load_console_nids(modules, args.export_table, args.nids, args.elf)
        report = []
        missing_nids = []
        seen = set()
        for module in modules:
            module_dir = PRX / module
            texts = source_texts(module_dir)
            provided = _collect_provided_from_texts(module_dir, texts)
            missing, _, _, lines = report_module(module, console[module], provided, db)
            missing_names = {nid: name for nid, name in missing}
            calls = _find_self_calls_in_texts(texts, missing_names)
            report.extend(lines)
            for name, nid, rel, line, qual in calls:
                if qual:
                    report.append(f"{module}: {name} ({nid}) called at {rel}:{line} {qual}{name} may need a renamed helper bound with APS5_EXPORT, like libSceAmpr")
                else:
                    report.append(f"{module}: {name} ({nid}) called at {rel}:{line} may need a renamed helper bound with APS5_EXPORT, like libSceAmpr")
            for nid, _ in missing:
                if nid not in seen:
                    seen.add(nid)
                    missing_nids.append(nid)
        if args.out is not None:
            destination = Path(args.out)
            if destination.exists() and destination.is_dir():
                raise GapError(f"{destination}: output is a directory")
            try:
                with open(destination, "w", encoding="utf-8", newline="\n") as handle:
                    handle.write("".join(nid + "\n" for nid in missing_nids))
            except OSError as error:
                raise GapError(f"{destination}: cannot write output: {error.strerror or error}") from error
        else:
            for nid in missing_nids:
                print(nid)
        for line in report:
            print(line)
    except GapError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"FAIL: {error.strerror}: {error.filename}" if error.filename else f"FAIL: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    raise SystemExit(main())
