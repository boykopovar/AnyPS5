import argparse
import concurrent.futures
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
from functools import cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIBS = ROOT / "core/libs"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("nid_names", ROOT / "tools/nid_names.py")
NIDS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NIDS)
DECLARATION = re.compile(r"(?=APS5_VABI|^)(?:\bAPS5_VABI\s+|^[ \t]*[A-Za-z_][\w:*&<> \t]*[ \t]+)(\w+)\s*\([^;{}]*\)\s*(?:noexcept\s*)?;", re.M)
_LINE_DECLARATION = re.compile(r"^[ \t]*[A-Za-z_][\w:*&<> \t]*[ \t]+(\w+)\s*\([^;{}]*\)\s*(?:noexcept\s*)?;", re.M)
DATA_DECLARATION = re.compile(
    r"^[ \t]*extern\s+(?:const\s+)?[A-Za-z_][\w:<>]*(?:\s+(?:const|volatile))?\s+\**\s*(\w+)\s*(?:\[[^;\]\n]*\])?\s*;", re.M
)
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
TYPE_TOKEN = re.compile(r"(?=[{}censu])(?:\bnamespace\s*([\w:]*)\s*\{|\bextern\s*\{|\b(?:struct|union|class)\s+(\w+)\s*(?:final\s*)?(?::(?!:)[^;{]*)?([;{])|\busing\s+(\w+)\s*=|[{}])")
_TYPE_NAME_HINT = re.compile(r"\b(?:struct|union|class|using)\s+(\w+)")
TOKENS, _, CODE_TOKEN = NIDS.CPP_TOKEN.pattern.rpartition("|(?P<code>")
CODE_RUN = re.compile(
    rf"""{TOKENS}|(?P<code>(?:{CODE_TOKEN[:-1]})(?:\s*(?:(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|(?!(?:u8|u|U|L)?R?["'])[A-Za-z_][A-Za-z0-9_]*|[^\w\s/"']))*)"""
)
MASK = re.compile(r"[^\n]")
_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))
_ANON_NAMESPACE = re.compile(r"\bnamespace\s*\{")
_BRACE = re.compile(r"[{}]")
_HIDDEN_SPECIFIER = re.compile(r"\b(?:static|GCC_HIDDEN_FN)\b")
_DECL_PREFIX_SKIP = re.compile(r"\b(?:return|co_return|throw|else|case)\b|&&|\|\|")


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


@cache
def code(text):
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
                out.append(_blank_span(text[start:end]))
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def declared_types(text):
    if "struct" not in text and "union" not in text and "class" not in text and "using" not in text:
        return set()
    scopes = []
    result = set()
    for token in TYPE_TOKEN.finditer(text):
        raw = token.group()
        if raw == "{":
            scopes.append(None)
            continue
        if raw == "}":
            if scopes:
                scopes.pop()
            continue
        namespace, name, ending, alias = token.groups()
        if namespace is not None:
            scopes.append(namespace or None)
        elif name or alias:
            if None not in scopes:
                result.add("::".join([scope for scope in scopes if scope] + [name or alias]))
            if ending == "{":
                scopes.append(name)
        else:
            scopes.append("")
    return result


@cache
def source_text(path):
    return path.read_text(encoding="utf-8")


@cache
def included_headers(header):
    result = []
    for name in INCLUDE.findall(source_text(header)):
        candidates = {path.resolve() for path in (header.parent / name, LIBS / name) if path.is_file()}
        if len(candidates) != 1:
            raise RuntimeError(f"{header}: include {name} resolves to {len(candidates)} files")
        result.append(candidates.pop())
    return result


def reachable(header, visited):
    if header in visited:
        return
    visited.add(header)
    for included in included_headers(header):
        reachable(included, visited)


def exported_definitions(text):
    if "\\\n" in text:
        text = text.replace("\\\n", "")
    if "\\\r\n" in text:
        text = text.replace("\\\r\n", "")
    if "APS5_VABI" not in text:
        return set()
    masked = code(text)
    matches = list(NIDS.DEFINITION.finditer(masked))
    if not matches:
        return set()
    ranges = []
    if "namespace" in masked:
        for match in _ANON_NAMESPACE.finditer(masked):
            depth = 1
            end = len(masked)
            for brace in _BRACE.finditer(masked, match.end()):
                depth += 1 if brace.group() == "{" else -1
                if depth == 0:
                    end = brace.end()
                    break
            ranges.append((match.start(), end))
    result = set()
    position = 0
    parentheses = 0
    for match in matches:
        prefix = masked[position:match.start()]
        parentheses += prefix.count("(") - prefix.count(")")
        position = match.start()
        if parentheses:
            continue
        if any(start < match.start() < end for start, end in ranges):
            continue
        start = max(masked.rfind(delimiter, 0, match.start()) for delimiter in ";{}") + 1
        if _HIDDEN_SPECIFIER.search(masked, start, match.start()):
            continue
        result.add(match.group(1))
    return result


def _scan_core_sources(core_dir):
    all_sources = []
    cpp_sources = []
    stack = [os.fspath(core_dir)]
    while stack:
        current = stack.pop()
        with os.scandir(current) as entries:
            for entry in entries:
                if entry.is_dir(follow_symlinks=False):
                    stack.append(entry.path)
                else:
                    name = entry.name
                    if name.endswith(".cpp"):
                        path = Path(entry.path)
                        all_sources.append(path)
                        cpp_sources.append(path)
                    elif name.endswith((".h", ".hpp")):
                        all_sources.append(Path(entry.path))
    all_sources.sort()
    cpp_sources.sort()
    return all_sources, cpp_sources


def _declaration_matches(text):
    return DECLARATION.finditer(text) if "APS5_VABI" in text else _LINE_DECLARATION.finditer(text)


def audit():
    umbrellas = sorted(path / f"{path.name}.hpp" for path in (LIBS / "prx").iterdir() if path.is_dir())
    if not umbrellas:
        raise RuntimeError("No library umbrella headers found")
    missing = [str(path.relative_to(ROOT)) for path in umbrellas if not path.is_file()]
    if missing:
        raise RuntimeError("Missing library umbrella headers: " + ", ".join(missing))
    exports = {}
    for umbrella in umbrellas:
        included = set()
        reachable(umbrella.resolve(), included)
        for source in NIDS.source_files(umbrella.parent):
            if "tools" in source.relative_to(umbrella.parent).parts:
                continue
            raw_text = source_text(source)
            text = raw_text
            if "\\\n" in text:
                text = text.replace("\\\n", "")
            if "\\\r\n" in text:
                text = text.replace("\\\r\n", "")
            aliases = {name for nid, name in NIDS.source_aliases(text)} if "APS5_EXPORT" in text else set()
            definitions = exported_definitions(text)
            if any(macro in text for macro in NIDS.ASM_FUNCTION_MACROS):
                definitions.update(NIDS.source_assembly_definitions(text))
            for name in definitions | aliases:
                if name.endswith(("_nid_no_patch", "_nid_no_patch_cut")):
                    continue
                exports.setdefault(name, []).append((source, included, name in definitions))
    declarations = {name: [] for name in exports}
    core_sources, cpp_sources = _scan_core_sources(ROOT / "core")
    for source in core_sources:
        text = code(source_text(source))
        resolved = None
        if "(" in text and ";" in text:
            prev_pos = 0
            line_no = 1
            for match in _declaration_matches(text):
                name = match.group(1)
                if name not in declarations:
                    continue
                if _DECL_PREFIX_SKIP.search(text, match.start(), match.start(1)):
                    continue
                if resolved is None:
                    resolved = source.resolve()
                line_no += text.count("\n", prev_pos, match.start())
                prev_pos = match.start()
                declarations[name].append((resolved, line_no, "function"))
        if "extern" in text and ";" in text:
            prev_pos = 0
            line_no = 1
            for match in DATA_DECLARATION.finditer(text):
                name = match.group(1)
                if name in declarations:
                    if resolved is None:
                        resolved = source.resolve()
                    line_no += text.count("\n", prev_pos, match.start())
                    prev_pos = match.start()
                    declarations[name].append((resolved, line_no, "data"))
    errors = []
    headers = set(umbrellas)
    for name, definitions in exports.items():
        found = declarations[name]
        if len(found) != 1 or found[0][0].suffix not in {".h", ".hpp"}:
            locations = ", ".join(f"{path.relative_to(ROOT)}:{line}" for path, line, _ in found)
            errors.append(f"{name}: expected exactly one header declaration; found {len(found)}: {locations}")
            continue
        header, _, kind = found[0]
        if kind != "function" and any(is_function for _, _, is_function in definitions):
            errors.append(f"{name}: function definition requires a function declaration")
            continue
        headers.add(header)
        for source, included, _ in definitions:
            if header not in included:
                errors.append(f"{name}: {header.relative_to(ROOT)} is not included by {source.parent} library umbrella")
    if errors:
        raise RuntimeError("\n".join(errors))
    public_types = set()
    for header in headers:
        text = code(source_text(header))
        public_types.update(declared_types(text))
    public_short_names = {name.rpartition("::")[2] for name in public_types}
    for source in cpp_sources:
        text = code(source_text(source))
        if "struct" not in text and "union" not in text and "class" not in text and "using" not in text:
            continue
        if not any(match.group(1) in public_short_names for match in _TYPE_NAME_HINT.finditer(text)):
            continue
        types = declared_types(text)
        for name in sorted(public_types & types):
            errors.append(f"{source.relative_to(ROOT)}: duplicate public type {name}; include its header")
    if errors:
        raise RuntimeError("\n".join(errors))
    return umbrellas, sorted(headers), len(exports)


def compile_headers(compiler, compiler_args, groups, workers=4):
    macros = subprocess.run([compiler, *compiler_args, "-dM", "-E", "-x", "c++", "-"], input="", text=True, capture_output=True, check=True).stdout
    warnings = ["-Werror"]
    if re.search(r"^#define __clang__\b", macros, re.M):
        warnings.append("-Wno-return-type-c-linkage")

    def compile_one(group):
        includes = "".join(f'#include "{header.relative_to(LIBS).as_posix()}"\n' for header in group)
        with tempfile.TemporaryDirectory() as directory:
            dependencies = Path(directory) / "headers.d"
            result = subprocess.run([compiler, *compiler_args, "-std=c++20", *warnings, "-fsyntax-only", "-MD", "-MF", str(dependencies), "-MT", "public_headers", "-x", "c++", f"-I{LIBS}", f"-I{ROOT / '3rdparty/freetype/include'}", "-"], input=includes * 2, text=True, capture_output=True)
            if result.returncode:
                raise RuntimeError(f"Header compilation failed: {group}\n{result.stdout}{result.stderr}")
            text = dependencies.read_text(encoding="utf-8").replace("\\\n", "")
            if not text.startswith("public_headers:"):
                raise RuntimeError(f"Invalid compiler dependency output: {dependencies}")
            names = re.findall(r"(?:\\[ \t]|[^\s])+", text.removeprefix("public_headers:"))
            paths = [Path(re.sub(r"\\([ \t#\\])", r"\1", name).replace("$$", "$")).resolve() for name in names]
            forbidden = [path for path in paths if path.is_relative_to((ROOT / "3rdparty").resolve())]
            if forbidden:
                raise RuntimeError(f"Public headers depend on 3rdparty: {group}\n" + "\n".join(map(str, forbidden)))

    errors = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        futures = [(group, pool.submit(compile_one, group)) for group in groups]
        for group, future in futures:
            try:
                future.result()
            except Exception as error:
                errors.append(f"{group}: {error}")
    if errors:
        raise RuntimeError(f"{len(errors)} header compilation units failed:\n\n" + "\n\n".join(errors))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler")
    parser.add_argument("--compiler-arg", action="append", default=[])
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--audit-only", action="store_true")
    parser.add_argument("--batch-index", type=int)
    parser.add_argument("--batch-count", type=int, default=64)
    args = parser.parse_args()
    if args.batch_count < 1:
        parser.error("--batch-count must be positive")
    if args.audit_only and args.batch_index is not None:
        parser.error("--audit-only cannot be combined with --batch-index")
    if not args.audit_only and not args.compiler:
        parser.error("--compiler is required for header compilation")
    if args.batch_index is not None:
        if not args.manifest or not 0 <= args.batch_index < args.batch_count:
            parser.error("a batch requires --manifest and an index within --batch-count")
        groups = [[LIBS / name for name in group] for group in json.loads(args.manifest.read_text(encoding="utf-8"))]
        selected = groups[args.batch_index::args.batch_count]
        if not selected:
            raise RuntimeError("Header compilation batch is empty")
        compile_headers(args.compiler, args.compiler_arg, selected, workers=1)
        print(f"Batch {args.batch_index}: {len(selected)} header compilation units verified")
        return
    umbrellas, headers, count = audit()
    groups = [[header] for header in headers] + [umbrellas]
    if args.manifest:
        args.manifest.write_text(json.dumps([[header.relative_to(LIBS).as_posix() for header in group] for group in groups]), encoding="utf-8")
    print(f"{len(umbrellas)} libraries, {count} exports: unique declarations and umbrella coverage verified", flush=True)
    if not args.audit_only:
        compile_headers(args.compiler, args.compiler_arg, groups)
        print("Standalone headers verified")


if __name__ == "__main__":
    main()
