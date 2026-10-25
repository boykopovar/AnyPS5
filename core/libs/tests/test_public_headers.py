import argparse
import concurrent.futures
import importlib.util
import json
import re
import subprocess
import sys
from functools import cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIBS = ROOT / "core/libs"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("nid_names", ROOT / "tools/nid_names.py")
NIDS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NIDS)
DECLARATION = re.compile(r"(?:\bAPS5_VABI\s+|^[ \t]*[A-Za-z_][\w:*&<> \t]*?[ \t]+)(\w+)\s*\([^;{}]*\)\s*(?:noexcept\s*)?;", re.M)
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
TYPE_TOKEN = re.compile(r"\bnamespace\s*([\w:]*)\s*\{|\bextern\s*\{|\b(?:struct|union|class)\s+(\w+)\s*([;{])|\busing\s+(\w+)\s*=|[{}]")


@cache
def code(text):
    return NIDS.CPP_TOKEN.sub(lambda match: match.group() if match.group("code") else re.sub(r"[^\n]", " ", match.group()), text)


def declared_types(text):
    scopes = []
    result = set()
    for token in TYPE_TOKEN.finditer(text):
        namespace, name, ending, alias = token.groups()
        if namespace is not None:
            scopes.append(namespace or None)
        elif name or alias:
            if None not in scopes:
                result.add("::".join([scope for scope in scopes if scope] + [name or alias]))
            if ending == "{":
                scopes.append(name)
        elif token.group().startswith("extern"):
            scopes.append("")
        elif token.group() == "{":
            scopes.append(None)
        elif scopes:
            scopes.pop()
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
    text = text.replace("\\\n", "").replace("\\\r\n", "")
    masked = code(text)
    ranges = []
    for match in re.finditer(r"\bnamespace\s*\{", masked):
        depth = 1
        end = match.end()
        while depth and end < len(masked):
            depth += (masked[end] == "{") - (masked[end] == "}")
            end += 1
        ranges.append((match.start(), end))
    result = set()
    position = 0
    parentheses = 0
    for match in NIDS.DEFINITION.finditer(masked):
        prefix = masked[position:match.start()]
        parentheses += prefix.count("(") - prefix.count(")")
        position = match.start()
        if parentheses:
            continue
        if any(start < match.start() < end for start, end in ranges):
            continue
        start = max(masked.rfind(delimiter, 0, match.start()) for delimiter in ";{}") + 1
        if re.search(r"\b(?:static|GCC_HIDDEN_FN)\b", masked[start:match.start()]):
            continue
        result.add(match.group(1))
    return result


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
            text = source_text(source).replace("\\\n", "").replace("\\\r\n", "")
            aliases = {name for nid, name in NIDS.source_aliases(text)} if "APS5_EXPORT" in text else set()
            definitions = exported_definitions(text)
            if any(macro in text for macro in NIDS.ASM_FUNCTION_MACROS):
                definitions.update(NIDS.source_assembly_definitions(text))
            for name in definitions | aliases:
                if name.endswith(("_nid_no_patch", "_nid_no_patch_cut")):
                    continue
                exports.setdefault(name, []).append((source, included))
    declarations = {name: [] for name in exports}
    for source in sorted((ROOT / "core").rglob("*")):
        if source.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        text = code(source_text(source))
        resolved = source.resolve()
        for match in DECLARATION.finditer(text):
            if match.group(1) not in declarations:
                continue
            prefix = match.group()[:match.start(1) - match.start()]
            if re.search(r"\b(?:return|co_return|throw|else|case)\b|&&|\|\|", prefix):
                continue
            declarations[match.group(1)].append((resolved, text.count("\n", 0, match.start()) + 1))
    errors = []
    headers = set(umbrellas)
    for name, definitions in exports.items():
        found = declarations[name]
        if len(found) != 1 or found[0][0].suffix not in {".h", ".hpp"}:
            locations = ", ".join(f"{path.relative_to(ROOT)}:{line}" for path, line in found)
            errors.append(f"{name}: expected exactly one header declaration; found {len(found)}: {locations}")
            continue
        header = found[0][0]
        headers.add(header)
        for source, included in definitions:
            if header not in included:
                errors.append(f"{name}: {header.relative_to(ROOT)} is not included by {source.parent} library umbrella")
    if errors:
        raise RuntimeError("\n".join(errors))
    public_types = set()
    for header in headers:
        text = code(source_text(header))
        public_types.update(declared_types(text))
    for source in sorted((ROOT / "core").rglob("*.cpp")):
        text = code(source_text(source))
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
        result = subprocess.run([compiler, *compiler_args, "-std=c++20", *warnings, "-fsyntax-only", "-x", "c++", f"-I{LIBS}", f"-I{ROOT / '3rdparty/freetype/include'}", "-"], input=includes * 2, text=True, capture_output=True)
        if result.returncode:
            raise RuntimeError(f"Header compilation failed: {group}\n{result.stdout}{result.stderr}")

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
        list(pool.map(compile_one, groups))


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
