#!/usr/bin/env python3
import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nid_names import CPP_TOKEN, compute_nid

NID_POSTFIX = "_nid_postfix"
NO_PATCH = "_nid_no_patch"
NO_PATCH_CUT = "_nid_no_patch_cut"
DISAMBIG = re.compile(r"_nid_disambig\d+$")
LINE_MARKER = re.compile(r'^#\s*(\d+)\s+"((?:[^"\\]|\\.)*)"')
ALIAS = re.compile(r'\.set "([A-Za-z0-9+\-]{11})' + NO_PATCH_CUT + r'", (\w+)')
SKIPPED_STATEMENTS = {"typedef", "using", "return", "static_assert", "template", "friend", "goto", "case", "delete", "throw"}
CMAKE_KEYWORDS = {"SHARED", "STATIC", "OBJECT", "MODULE", "EXCLUDE_FROM_ALL", "PRIVATE", "PUBLIC", "INTERFACE", "SYSTEM", "BEFORE", "AFTER"}


def export_name(symbol):
    if symbol.endswith(NO_PATCH_CUT):
        return symbol[:-len(NO_PATCH_CUT)]
    if symbol.endswith(NO_PATCH):
        return symbol
    base = symbol[:-len(NID_POSTFIX)] if symbol.endswith(NID_POSTFIX) else symbol
    return compute_nid(DISAMBIG.sub("", base))


def main_file_text(preprocessed, source):
    wanted = Path(source).resolve()
    keep = True
    lines = []
    for line in preprocessed.splitlines():
        marker = LINE_MARKER.match(line)
        if marker:
            name = marker.group(2).encode().decode("unicode_escape")
            keep = Path(name).resolve() == wanted
            continue
        if keep:
            lines.append(line)
    return "\n".join(lines)


def tokens(text):
    return [match.group() for match in CPP_TOKEN.finditer(text) if match.group("comment") is None]


def string_value(token):
    body = token[token.index('"') + 1:token.rindex('"')]
    return body.encode().decode("unicode_escape")


def declarator_name(statement, marker_index):
    for index in range(marker_index + 1, len(statement)):
        if statement[index] == "(" and index > marker_index + 1 and re.fullmatch(r"[A-Za-z_]\w*", statement[index - 1]):
            return statement[index - 1]
    return None


def function_name(statement):
    depth = 0
    for index, token in enumerate(statement):
        if token == "(":
            if depth == 0 and index > 0 and re.fullmatch(r"[A-Za-z_]\w*", statement[index - 1]):
                return statement[index - 1]
            depth += 1
        elif token == ")":
            depth -= 1
    return None


def data_name(statement):
    if not statement or statement[0] in SKIPPED_STATEMENTS:
        return None
    for index, token in enumerate(statement):
        if token == "(":
            return None
        if token.endswith(NID_POSTFIX) and index > 0:
            following = statement[index + 1] if index + 1 < len(statement) else ";"
            if following not in ("=", "[", "{", ";"):
                return None
            if statement[0] == "extern" and "=" not in statement[index:] and "{" not in statement[index:]:
                return None
            return token
    return None


def scan(text):
    found = []
    stream = tokens(text)
    scopes = []
    statement = []
    index = 0

    def qualified(name):
        names = [scope for scope in scopes if scope not in ("", None)]
        return "::".join(names + [name])

    def at_namespace_scope():
        return all(scope is not None for scope in scopes)

    while index < len(stream):
        token = stream[index]
        index += 1
        if token == "{":
            if not at_namespace_scope():
                scopes.append(None)
                continue
            if statement[:1] == ["namespace"]:
                scopes.append("".join(statement[1:]) if len(statement) > 1 else "")
            elif statement[:1] == ["extern"] and len(statement) == 2 and statement[1].startswith('"'):
                scopes.append("")
            elif statement and (statement[-1] == "=" or statement[-1].endswith(NID_POSTFIX)):
                depth = 1
                while index < len(stream) and depth:
                    depth += {"{": 1, "}": -1}.get(stream[index], 0)
                    index += 1
                statement.append("{}")
                continue
            else:
                if "static" not in statement and "APS5_VABI" in statement:
                    name = declarator_name(statement, statement.index("APS5_VABI"))
                    if name:
                        found.append(("function", export_name(name), qualified(name)))
                elif "static" not in statement and "APS5_VABI" not in statement:
                    name = function_name(statement)
                    if name and (name.endswith(NO_PATCH) and not name.endswith(NO_PATCH_CUT)):
                        found.append(("function", export_name(name), qualified(name)))
                scopes.append(None)
            statement = []
        elif token == "}":
            if scopes:
                scopes.pop()
            statement = []
        elif token == ";":
            if at_namespace_scope():
                if statement[:1] == ["__asm__"] or statement[:1] == ["asm"]:
                    text_value = "".join(string_value(part) for part in statement if part.startswith('"'))
                    for nid, name in ALIAS.findall(text_value):
                        found.append(("function", nid, qualified(name)))
                elif "static" not in statement and "APS5_VABI" not in statement:
                    name = data_name(statement)
                    if name:
                        found.append(("data", export_name(name), qualified(name)))
            statement = []
        else:
            if at_namespace_scope():
                statement.append(token)
    unique = []
    seen = set()
    for entry in found:
        if entry[1] not in seen:
            seen.add(entry[1])
            unique.append(entry)
    return unique


def symbol(kind, nid):
    return "aps5_%s_%s" % ("nid" if kind == "function" else "data", hashlib.sha1(nid.encode()).hexdigest()[:20])


def render(source, exports):
    lines = ['#include "%s"' % Path(source).resolve().as_posix(), '#include "wasm-runtime/NidThunk.hpp"', ""]
    for kind, nid, name in exports:
        macro = "APS5_WASM_NID" if kind == "function" else "APS5_WASM_DATA"
        lines.append('%s(%s, "%s", %s)' % (macro, symbol(kind, nid), nid, name))
    return "\n".join(lines) + "\n"


def write_if_changed(path, text):
    path = Path(path)
    if path.exists() and path.read_text(encoding="utf-8") == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def cmake_arguments(text):
    return re.findall(r'"[^"]*"|[^\s()]+', text)


def cmake_commands(text):
    text = re.sub(r"#[^\n]*", "", text)
    commands = []
    depth = 0
    for match in re.finditer(r"([A-Za-z_]\w*)\s*\(", text):
        name = match.group(1).lower()
        start = match.end()
        level = 1
        position = start
        while position < len(text) and level:
            level += {"(": 1, ")": -1}.get(text[position], 0)
            position += 1
        if name in ("if", "foreach", "function", "macro", "while"):
            depth += 1
        elif name in ("endif", "endforeach", "endfunction", "endmacro", "endwhile"):
            depth -= 1
        elif depth == 0:
            commands.append((name, cmake_arguments(text[start:position - 1])))
    return commands


def library_layout(library_dir, libs_dir, root):
    library_dir = Path(library_dir)
    name = library_dir.name
    cmake = library_dir / "CMakeLists.txt"
    variables = {"CMAKE_CURRENT_SOURCE_DIR": library_dir.as_posix(), "LIBS_INCLUDE_DIR": Path(libs_dir).as_posix(), "CMAKE_SOURCE_DIR": Path(root).as_posix()}

    def expand(value):
        value = value.strip('"')
        if "$<" in value:
            return None
        unknown = False

        def substitute(match):
            nonlocal unknown
            if match.group(1) not in variables:
                unknown = True
                return ""
            return variables[match.group(1)]

        value = re.sub(r"\$\{(\w+)\}", substitute, value)
        if unknown or not value:
            return None
        path = Path(value)
        return (path if path.is_absolute() else library_dir / path).resolve().as_posix()

    sources, includes, definitions = [], [], []
    if not cmake.exists():
        return {"sources": [(library_dir / "Export.cpp").resolve().as_posix()], "includes": [], "definitions": []}
    for command, arguments in cmake_commands(cmake.read_text(encoding="utf-8")):
        if not arguments or arguments[0] != name:
            continue
        values = [argument for argument in arguments[1:] if argument not in CMAKE_KEYWORDS]
        if command in ("add_library", "target_sources"):
            sources += [path for path in map(expand, values) if path and path.endswith((".cpp", ".cc", ".c"))]
        elif command == "target_include_directories":
            includes += [path for path in map(expand, values) if path]
        elif command == "target_compile_definitions":
            definitions += [value.strip('"') for value in values if "$" not in value]
    return {"sources": sources, "includes": includes, "definitions": definitions}


def cmake_list(values):
    return ";".join(value.replace(";", "\\;") for value in values)


def command_layout(arguments):
    root = Path(arguments.root).resolve()
    libs = root / "core" / "libs"
    lines = []
    for library in arguments.libraries:
        layout = library_layout(libs / "prx" / library, libs, root)
        variable = re.sub(r"\W", "_", library)
        lines.append('set(ANYPS5_WASM_%s_SOURCES "%s")' % (variable, cmake_list(layout["sources"])))
        lines.append('set(ANYPS5_WASM_%s_INCLUDES "%s")' % (variable, cmake_list(layout["includes"])))
        lines.append('set(ANYPS5_WASM_%s_DEFINITIONS "%s")' % (variable, cmake_list(layout["definitions"])))
        lines.append('set(ANYPS5_WASM_%s_CMAKE "%s")' % (variable, (libs / "prx" / library / "CMakeLists.txt").as_posix()))
    write_if_changed(arguments.output, "\n".join(lines) + "\n")
    return 0


def command_wrap(arguments):
    compiler = arguments.compiler
    if not compiler:
        print("wasm_nid_thunks.py wrap: pass the compiler command after --", file=sys.stderr)
        return 2
    result = subprocess.run(compiler + ["-E", "-DAPS5_WASM_EXPORT_SCAN", arguments.source], capture_output=True, text=True, errors="replace")
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        return result.returncode
    exports = scan(main_file_text(result.stdout, arguments.source))
    write_if_changed(arguments.output, render(arguments.source, exports))
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description="Generate NID thunks that export library functions to translated wasm games")
    commands = parser.add_subparsers(dest="command", required=True)
    layout = commands.add_parser("layout", help="write the sources, include directories and definitions of libraries as a CMake file")
    layout.add_argument("--root", required=True)
    layout.add_argument("--output", required=True)
    layout.add_argument("libraries", nargs="+")
    wrap = commands.add_parser("wrap", help="write a translation unit that includes a source and exports its guest functions")
    wrap.add_argument("source")
    wrap.add_argument("output")
    arguments_list = list(sys.argv[1:] if argv is None else argv)
    compiler = []
    if "--" in arguments_list:
        split = arguments_list.index("--")
        compiler = arguments_list[split + 1:]
        arguments_list = arguments_list[:split]
    arguments = parser.parse_args(arguments_list)
    arguments.compiler = compiler
    if arguments.command == "layout":
        return command_layout(arguments)
    return command_wrap(arguments)


if __name__ == "__main__":
    sys.exit(main())
