from collections import defaultdict
import argparse
import ast
import itertools
import json
import re
import subprocess
import sys
from pathlib import Path

TEST_OUTPUT = r"tests[\\/]"
_TEST_EXEC_PATH = re.compile(rf"^{TEST_OUTPUT}(?:.*[\\/])?([^\\/]+)$")
SCOPE_START = {"if", "foreach", "while", "function", "macro", "block"}
COMMAND = re.compile(r"^\s*(add_test|set_tests_properties|set_property|subdirs|if|elseif|else|endif|foreach|endforeach|while|endwhile|function|endfunction|macro|endmacro|block|endblock)\s*\(", re.M | re.I)
_COMMAND_HINT = re.compile(r"(?:add_test|set_tests_properties|set_property|subdirs|if|elseif|else|endif|foreach|endforeach|while|endwhile|function|endfunction|macro|endmacro|block|endblock)\s*\(", re.I)
ARGUMENT = re.compile(r'"(?:\\.|[^"\\])*"|\[(?P<equals>=*)\[.*?\](?P=equals)\]|(?:\\\r?\n|\\[ \t]|[^\s()])+', re.S)
LEXICAL = re.compile(r'\\.|"(?:\\.|[^"\\])*"|\[(?P<literal_equals>=*)\[.*?\](?P=literal_equals)\]'
                     r'|(?P<comment>\#\[(?P<comment_equals>=*)\[.*?\](?P=comment_equals)\]|\#[^\n]*)', re.S)
_PAREN = re.compile(r"[()]")
_QUOTE_ESCAPE = re.compile(r"\\(\r?\n|.)", re.S)
_QUOTE_ESCAPES = {"\n": "", "\r\n": "", "n": "\n", "r": "\r", "t": "\t", ";": r"\;"}
_UNQUOTED_ESCAPE = re.compile(r"\\(\r?\n|[ \t])")
_ENV_ASSIGN = re.compile(r"^[A-Za-z_]\w*=")
_PATH_SPLIT = re.compile(r"[\\/]")
_YAML_SEQUENCE = re.compile(r"-\s+")
_YAML_KEY = re.compile(r"(['\"]?)([A-Za-z_][\w.-]*)\1\s*:(?:\s+(.*)|$)")
_YAML_INDENT = re.compile(r"[1-9]")
_HASH_COMMENT = re.compile(r"\s#")
_TEST_SCRIPT_NAME = re.compile(r"^test_\w+\.py$")


class PropertyListError(ValueError):
    pass


KNOWN_UNRUN = set()
SANITIZER_SKIPPED_LIBRARIES = {"libSceAddressSanitizer"}
SET_VARIABLE = re.compile(r'^[ \t]*set\(\s*(\w+)\s+"?([^"\s()]+)"?\s*\)', re.M | re.I)
VARIABLE = re.compile(r"\$\{(\w+)\}")
SCRIPT = re.compile(r"(?<![\w./-])(?:[\w.-]+/)*test_\w+\.py(?![\w./-])")
EXPORTED = re.compile(r"\bAPS5_VABI\s+[A-Za-z_]\w*\s*\([^;{}]*\)\s*(?:noexcept\s*)?(?:->\s*[^;{]+)?(?:try\s*)?\{")
INCLUDED_SOURCE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+\.cpp)"', re.M)
COMPILE_FLAG = re.compile(r"(?:^|\s)[-/]c(?:\s|$)")

CPP_TOKEN = re.compile(
    r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)'
    r'|(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*')"
    r"|(?P<code>(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S)")
_NON_CODE_TRIGGER = re.compile(r'//|/\*|"|\'')
_RAW_BODY = re.compile(r'"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"')
_STRING_BODY = re.compile(r'"(?:\\[\s\S]|[^"\\])*"')
_CHAR_BODY = re.compile(r"'(?:\\[\s\S]|[^'\\])*'")
_CODE_TOKEN = re.compile(r"(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S")
_PRE_TOKEN_CHARS = frozenset("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.'")
_STRING_PREFIXES = frozenset(("u8", "u", "U", "L"))
_RAW_PREFIXES = frozenset(("R", "u8R", "uR", "UR", "LR"))


def _blank_span(span, fill=" "):
    if "\n" not in span:
        return fill * len(span)
    return "\n".join(fill * len(line) for line in span.split("\n"))


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


def cpp_source(text, keep_strings=False):
    if "\\\n" in text:
        text = text.replace("\\\n", "")
    if "\\\r\n" in text:
        text = text.replace("\\\r\n", "")
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
                if not keep_strings:
                    out.append(text[pos:start])
                    out.append(_blank_span(text[start:end]))
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
                out.append(text[pos:start])
                out.append(_blank_span(text[start:end]))
                pos = cursor = floor = end
            else:
                cursor = floor = index + 1
    if not out:
        return text
    out.append(text[pos:])
    return "".join(out)


def ninja(build):
    program, generator = None, None
    configurations = []
    for line in (Path(build) / "CMakeCache.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("CMAKE_MAKE_PROGRAM:"):
            program = line.partition("=")[2]
        elif line.startswith("CMAKE_GENERATOR:"):
            generator = line.partition("=")[2]
        elif line.startswith("CMAKE_CONFIGURATION_TYPES:"):
            configurations = list(filter(None, line.partition("=")[2].split(";")))
    if generator not in ("Ninja", "Ninja Multi-Config"):
        raise ValueError(f"test registration audit requires a Ninja generator, got {generator!r}")
    if not program or program.endswith("-NOTFOUND"):
        raise FileNotFoundError(f"CMAKE_MAKE_PROGRAM is not set in {Path(build) / 'CMakeCache.txt'}")
    encoding = "utf-8"
    if sys.platform == "win32":
        codepage = subprocess.run([program, "-t", "wincodepage"], capture_output=True, text=True,
                                  encoding="ascii", check=True).stdout.strip()
        encodings = {"Build file encoding: UTF-8": "utf-8", "Build file encoding: ANSI": "mbcs"}
        if codepage not in encodings:
            raise ValueError(f"unsupported Ninja build-file encoding: {codepage!r}")
        encoding = encodings[codepage]
    return program, encoding, configurations if generator == "Ninja Multi-Config" else []


def ninja_outputs(build, *arguments):
    program, encoding, configurations = ninja(build)
    manifests = [("-f", f"build-{configuration}.ninja") for configuration in configurations]
    for manifest in manifests or [()]:
        yield subprocess.run([program, "-C", str(build), *manifest, "-t", *arguments],
                             capture_output=True, text=True, encoding=encoding, check=True).stdout


def built(build):
    names = set()
    for output in ninja_outputs(build, "targets", "all"):
        for line in output.splitlines():
            path, _, rule = line.partition(": ")
            if "EXECUTABLE" not in rule:
                continue
            found = _TEST_EXEC_PATH.match(path)
            if not found:
                continue
            name = found.group(1)
            if name.endswith(".exe"):
                name = name[:-4]
            if name:
                names.add(name)
    return names


def compiled(build):
    sources = set()
    for database in ninja_outputs(build, "compdb"):
        for entry in json.loads(database):
            command = entry.get("command") or " ".join(entry.get("arguments", []))
            if not COMPILE_FLAG.search(command):
                continue
            file = Path(entry["file"])
            if not file.is_absolute():
                file = Path(entry["directory"]) / file
            sources.add(file.resolve())
    return sources


def skipped_libraries(build):
    enabled = False
    for line in (Path(build) / "CMakeCache.txt").read_text(encoding="utf-8-sig").splitlines():
        name, _, value = line.partition(":")
        on = value.partition("=")[2].strip().upper() in ("ON", "1", "TRUE", "YES", "Y")
        if name in ("ANYPS5_RELINKER_ONLY", "ANYPS5_BUILD_WASM") and on:
            return None
        if name in ("ANYPS5_ENABLE_ASAN", "ANYPS5_ENABLE_TSAN"):
            enabled = enabled or on
    return set(SANITIZER_SKIPPED_LIBRARIES) if enabled else set()


def source_directory(build):
    cache = Path(build) / "CMakeCache.txt"
    for line in cache.read_text(encoding="utf-8-sig").splitlines():
        name, _, value = line.partition("=")
        if name == "CMAKE_HOME_DIRECTORY:INTERNAL" and value:
            return Path(value)
    raise ValueError(f"CMAKE_HOME_DIRECTORY is not set in {cache}")


def unbuilt_exports(root, sources, skipped=()):
    prx = (Path(root) / "core" / "libs" / "prx").resolve()
    built_sources = {source for source in sources if prx in source.parents}
    if not built_sources:
        return None
    pending = list(built_sources)
    while pending:
        source = pending.pop()
        try:
            text = source.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if '.cpp"' not in text.replace("\\\n", "").replace("\\\r\n", ""):
            continue
        for name in INCLUDED_SOURCE.findall(cpp_source(text, keep_strings=True)):
            included = (source.parent / name).resolve()
            if included not in built_sources and included.is_file():
                built_sources.add(included)
                pending.append(included)
    unbuilt = []
    for file in sorted(prx.glob("*/**/*.cpp")):
        relative = file.relative_to(prx)
        if relative.parts[0] in skipped:
            continue
        if any(part in ("tests", "test") for part in relative.parts[1:-1]) or file.resolve() in built_sources:
            continue
        if EXPORTED.search(cpp_source(file.read_text(encoding="utf-8", errors="replace"))):
            unbuilt.append(Path("core/libs/prx") / relative)
    return unbuilt


def _mask_cmake(content):
    content_parts = []
    code_parts = []
    pos = 0
    for match in LEXICAL.finditer(content):
        start, end = match.span()
        if start > pos:
            chunk = content[pos:start]
            content_parts.append(chunk)
            code_parts.append(chunk)
        token = match.group()
        if match.group("comment") is not None:
            blanked = _blank_span(token, " ")
            content_parts.append(blanked)
            code_parts.append(blanked)
        else:
            content_parts.append(token)
            code_parts.append(_blank_span(token, "_"))
        pos = end
    if not pos:
        return content, content
    tail = content[pos:]
    content_parts.append(tail)
    code_parts.append(tail)
    return "".join(content_parts), "".join(code_parts)


def paired_properties(command, arguments, where):
    properties = arguments.index("PROPERTIES")
    values = arguments[properties + 1:]
    if len(values) % 2:
        location = f"{where}: " if where else ""
        raise PropertyListError(f"{location}cannot pair the arguments after PROPERTIES in {command}({' '.join(arguments)}); "
                                "a property list expanded from a variable is not supported")
    return list(zip(values[::2], values[1::2]))


def command_records(content):
    content = content.removeprefix("\ufeff")
    if _COMMAND_HINT.search(content) is None:
        return
    content, code = _mask_cmake(content)
    parsed_until = 0
    for command in COMMAND.finditer(code):
        if command.start() < parsed_until:
            continue
        name = command.group(1).lower()
        line = content.count("\n", 0, command.start(1)) + 1
        if name in SCOPE_START or name.removeprefix("end") in SCOPE_START or name in {"else", "elseif"}:
            depth = 1
            for paren in _PAREN.finditer(code, command.end()):
                depth += 1 if paren.group() == "(" else -1
                if depth == 0:
                    parsed_until = paren.end()
                    break
            else:
                raise ValueError(f"unterminated {name} command at position {command.start()}")
            yield name, [], line
            continue
        position, arguments = command.end(), []
        length = len(content)
        while position < length:
            ch = content[position]
            if ch.isspace():
                position += 1
                continue
            if ch == ")":
                parsed_until = position + 1
                yield name, arguments, line
                break
            if ch == "#":
                position = content.find("\n", position)
                if position < 0:
                    position = length
                continue
            token = ARGUMENT.match(content, position)
            if token is None:
                raise ValueError(f"invalid CTest argument at position {position}")
            argument = token.group(0)
            if argument.startswith('"'):
                inner = argument[1:-1]
                if "\\" in inner:
                    argument = _QUOTE_ESCAPE.sub(lambda match: _QUOTE_ESCAPES.get(match[1], match[1]), inner)
                else:
                    argument = inner
            elif token.group("equals") is not None:
                width = len(token.group("equals")) + 2
                argument = argument[width:-width].removeprefix("\n")
            elif "\\" in argument:
                argument = _UNQUOTED_ESCAPE.sub(lambda match: "" if "\n" in match[1] else match[1], argument)
            arguments.append(argument)
            position = token.end()
        else:
            raise ValueError(f"unterminated {command.group(1)} command at position {command.start()}")


def commands(content):
    for name, arguments, _ in command_records(content):
        yield name, arguments


def testfile_command_records(build):
    pending = [Path(build)]
    visited = set()
    while pending:
        try:
            directory = pending.pop().resolve()
        except RuntimeError:
            continue
        file = directory / "CTestTestfile.cmake"
        if directory in visited or not file.is_file():
            continue
        visited.add(directory)
        entries = list(command_records(file.read_text(encoding="utf-8", errors="replace")))
        for command, arguments, _ in entries:
            if command == "subdirs":
                pending.extend(directory / argument for argument in arguments)
        yield file, entries


def testfile_commands(build):
    for _, entries in testfile_command_records(build):
        yield [(command, arguments) for command, arguments, _ in entries]


def registered(build):
    names = set()
    for file, entries in testfile_command_records(build):
        tests, latest = [], {}
        for command, arguments, line in entries:
            if command == "add_test" and arguments:
                test = [arguments[1:], False]
                tests.append(test)
                latest[arguments[0]] = test
            elif command == "set_tests_properties" and "PROPERTIES" in arguments:
                properties = arguments.index("PROPERTIES")
                for key, value in paired_properties(command, arguments, f"{file}:{line}"):
                    if key == "DISABLED":
                        for name in arguments[:properties]:
                            if name in latest:
                                latest[name][1] = value.upper() in ("1", "ON", "YES", "TRUE", "Y")
        for arguments, disabled in tests:
            if disabled:
                continue
            for index, argument in enumerate(arguments):
                if index and (argument.startswith("-") or _ENV_ASSIGN.match(argument)):
                    continue
                base = _PATH_SPLIT.split(argument)[-1]
                if base.endswith(".exe"):
                    base = base[:-4]
                if base:
                    names.add(base)
    return names


def listed(root):
    output = subprocess.run(["git", "-c", "core.quotePath=false", "-C", str(root), "ls-files", "-z", "--cached", "--others",
                             "--exclude-standard"], capture_output=True, check=True).stdout.decode("utf-8", errors="replace")
    return sorted(set(filter(None, output.split("\0"))))


def folded_run_lines(lines, content_indent):
    paragraph = []
    for line in lines:
        text = line.lstrip(" ")
        if text.strip():
            indent = len(line) - len(text)
            if content_indent is None:
                content_indent = indent
            if indent == content_indent:
                paragraph.append(text)
                continue
        if paragraph:
            yield " ".join(paragraph)
            paragraph = []
        if text.strip():
            yield line
    if paragraph:
        yield " ".join(paragraph)


def workflow_run_lines(content):
    parents = []
    step_key_indent = None
    block_indent = None
    folded = None
    content_indent = None
    for line in content.splitlines():
        text = line.lstrip()
        indent = len(line) - len(text)
        if folded is not None:
            if not text or indent > block_indent:
                folded.append(line)
                continue
            yield from folded_run_lines(folded, content_indent)
            folded = None
            block_indent = None
        if not text or text.startswith("#"):
            continue
        if block_indent is not None:
            if indent > block_indent:
                yield line
                continue
            block_indent = None
        sequence = _YAML_SEQUENCE.match(text)
        while parents and (parents[-1][0] > indent or
                           (parents[-1][0] == indent and not (sequence and parents[-1][1] == "steps"))):
            parents.pop()
        path = [key for _, key in parents]
        step = len(path) == 3 and path[0] == "jobs" and path[2] == "steps"
        key_indent = indent
        if sequence:
            key_indent += sequence.end()
            text = text[sequence.end():]
            if step:
                step_key_indent = key_indent
        match = _YAML_KEY.match(text)
        if match is None:
            continue
        key, value = match[2], match[3] or ""
        if key == "steps" and len(path) == 2 and path[0] == "jobs":
            step_key_indent = None
        if step and key_indent == step_key_indent and key == "run":
            if value.startswith(("|", ">")):
                block_indent = key_indent
                if value.startswith(">"):
                    folded = []
                    explicit = _YAML_INDENT.search(value.split()[0])
                    content_indent = key_indent + int(explicit[0]) if explicit else None
            elif value:
                yield value
        parents.append((key_indent, key))
    if folded is not None:
        yield from folded_run_lines(folded, content_indent)


def disabled_properties(command, arguments, where=""):
    if command == "set_tests_properties" and "PROPERTIES" in arguments:
        properties = arguments.index("PROPERTIES")
        for key, value in paired_properties(command, arguments, where):
            if key == "DISABLED":
                yield arguments[:properties], "replace", value
    elif command == "set_property" and arguments[:1] == ["TEST"] and "PROPERTY" in arguments:
        properties = arguments.index("PROPERTY")
        names = arguments[1:properties]
        if "DIRECTORY" in names or arguments[properties + 1:properties + 2] != ["DISABLED"]:
            return
        mode = "string" if "APPEND_STRING" in names else "append" if "APPEND" in names else "replace"
        yield [name for name in names if name not in {"APPEND", "APPEND_STRING"}], mode, ";".join(arguments[properties + 2:])


def disabled_value(current, mode, value):
    if "${" in value or "$<" in value:
        return None, False
    if mode == "replace":
        return value, is_disabled(value)
    if not value:
        return current
    raw, _ = current
    if raw is None:
        return None, False
    if mode == "string":
        updated = raw + value
    else:
        updated = raw + ";" + value if raw else value
    return updated, is_disabled(updated)


def is_disabled(value):
    return value is not None and value.upper() in ("1", "ON", "YES", "TRUE", "Y")


def run_scripts(root, files, invocations=None):
    run = set()
    root_path = Path(root)
    for name in files:
        if name.endswith("CMakeLists.txt") or name.endswith(".cmake"):
            content = (root_path / name).read_text(encoding="utf-8", errors="replace")
            tests = []
            scopes = []
            for index, (command, arguments, line) in enumerate(command_records(content)):
                if command in SCOPE_START:
                    scopes.append((index, index, command))
                elif command.removeprefix("end") in SCOPE_START:
                    if scopes:
                        scopes.pop()
                elif command in {"else", "elseif"}:
                    if scopes:
                        scopes[-1] = (scopes[-1][0], index, scopes[-1][2])
                elif command == "add_test" and arguments:
                    if arguments and arguments[0] == "NAME":
                        if "COMMAND" not in arguments[2:]:
                            continue
                        test_name = arguments[1]
                        start = arguments.index("COMMAND", 2) + 1
                        end = next((index for index in range(start, len(arguments))
                                    if arguments[index] in ("CONFIGURATIONS", "WORKING_DIRECTORY", "COMMAND_EXPAND_LISTS", "BUILD_DEPENDS")), len(arguments))
                        arguments = arguments[start:end]
                    else:
                        test_name = arguments[0]
                        arguments = arguments[1:]
                    tests.append([test_name, arguments, ("", False), scopes[:]])
                elif command in {"set_tests_properties", "set_property"}:
                    for names, mode, value in disabled_properties(command, arguments, f"{name}:{line}"):
                        for test in tests:
                            if test[0] in names:
                                exclusive = False
                                for left, right in zip(test[3], scopes):
                                    if left[0] != right[0] or left[2] in {"foreach", "while", "function", "macro"}:
                                        break
                                    if left[1] != right[1]:
                                        exclusive = True
                                        break
                                if exclusive:
                                    continue
                                updated = disabled_value(test[2], mode, value)
                                if test[3][:len(scopes)] == scopes:
                                    test[2] = updated
                                elif updated != test[2]:
                                    test[2] = None, test[2][1] and updated[1]
            if tests:
                variables = dict(SET_VARIABLE.findall(content)) if invocations is not None else None
                for _, arguments, value, _ in tests:
                    if not value[1]:
                        run.update(argument.rpartition("}")[2].lstrip("/") for argument in arguments)
                        if invocations is not None:
                            invocations.append([VARIABLE.sub(lambda match: variables.get(match.group(1), match.group()), argument)
                                                .rpartition("}")[2].lstrip("/") for argument in arguments])
        elif name.startswith(".github/workflows/") and name.endswith((".yml", ".yaml")):
            for line in workflow_run_lines((root_path / name).read_text(encoding="utf-8", errors="replace")):
                if line.lstrip().startswith("#"):
                    continue
                line = _HASH_COMMENT.split(line, 1)[0]
                if "python" in line:
                    run.update(token[2:] if token.startswith("./") else token for token in SCRIPT.findall(line))
                    if invocations is not None:
                        tokens = [token.strip("'\"") for token in line.split()]
                        invocations.append([token[2:] if token.startswith("./") else token for token in tokens])
    return {name for name in run if SCRIPT.fullmatch(name)}


def unittest_classes(path):
    classes = {node.name: node for node in ast.parse(path.read_text(encoding="utf-8", errors="replace")).body
               if isinstance(node, ast.ClassDef)}

    def lineage(name, seen):
        found = {name}
        for base in classes[name].bases:
            base = base.attr if isinstance(base, ast.Attribute) else getattr(base, "id", None)
            if base in classes and base not in seen:
                found |= lineage(base, seen | {base})
            elif base:
                found.add(base)
        return found

    def tested(name):
        return name in classes and any(isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)) and item.name.startswith("test")
                                       for item in classes[name].body)

    return [name for name in classes if any(base.endswith("TestCase") for base in lineage(name, {name}))
            and any(tested(base) for base in lineage(name, {name}))]


def _unselected_classes_from(root, files, invocations):
    selections = defaultdict(list)
    for arguments in invocations:
        for index, argument in enumerate(arguments):
            if SCRIPT.fullmatch(argument):
                selections[argument].append(
                    list(itertools.takewhile(lambda token: token not in {"&&", "||", ";", "|"}, arguments[index + 1:])))
    by_base = defaultdict(list)
    for path, found in selections.items():
        by_base[path.rpartition("/")[2]].append((path, found))
    root_path = Path(root)
    unselected = {}
    for name in files:
        base = name.rpartition("/")[2]
        if not _TEST_SCRIPT_NAME.match(base):
            continue
        candidates = by_base.get(base)
        if not candidates:
            continue
        runs = [names for path, found in candidates if name == path or name.endswith("/" + path) for names in found]
        if not runs:
            continue
        if any(not any(argument.partition(".")[0].isidentifier() for argument in names) for names in runs):
            continue
        classes = unittest_classes(root_path / name)
        selected = set()
        for names in runs:
            if not any(argument.partition(".")[0] in classes for argument in names):
                selected.update(classes)
            selected.update(names)
        if missing := [cls for cls in classes if cls not in selected]:
            unselected[name] = missing
    return unselected


def unselected_classes(root):
    files = listed(root)
    invocations = []
    run_scripts(root, files, invocations)
    return _unselected_classes_from(root, files, invocations)


def _unrun_scripts_from(files, run):
    run_by_base = defaultdict(list)
    for path in run:
        run_by_base[path.rpartition("/")[2]].append(path)
    scripts = [name for name in files if _TEST_SCRIPT_NAME.match(name.rpartition("/")[2])]
    unrun = []
    for name in scripts:
        paths = run_by_base.get(name.rpartition("/")[2])
        if not paths or not any(name == path or name.endswith("/" + path) for path in paths):
            unrun.append(name)
    return scripts, unrun


def unrun_scripts(root):
    files = listed(root)
    run = run_scripts(root, files)
    return _unrun_scripts_from(files, run)


_DEFAULT_UNRUN_SCRIPTS = unrun_scripts
_DEFAULT_UNSELECTED_CLASSES = unselected_classes


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="fail when a test executable is built that no ctest test runs, or a library source that defines exports is never compiled")
    parser.add_argument("build", type=Path, nargs="?", default=Path("build"), help="configured build directory")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--json", action="store_true", help="emit registration audit results as JSON")
    output.add_argument("--list", action="store_true", help="print every test executable and whether ctest runs it")
    parser.add_argument("--scripts", action="store_true", help="instead check that an add_test or a workflow runs every test_*.py of the source tree, "
                                                                "and every test class of a script that is only run class by class")
    args = parser.parse_args(argv)
    if args.scripts:
        try:
            root = Path(__file__).resolve().parents[1]
            if unrun_scripts is _DEFAULT_UNRUN_SCRIPTS and unselected_classes is _DEFAULT_UNSELECTED_CLASSES:
                files = listed(root)
                invocations = []
                run = run_scripts(root, files, invocations)
                scripts, unrun = _unrun_scripts_from(files, run)
                unselected = _unselected_classes_from(root, files, invocations)
            else:
                scripts, unrun = unrun_scripts(root)
                unselected = unselected_classes(root)
        except PropertyListError as error:
            parser.error(str(error))
        except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
            parser.error(f"--scripts reads the source tree from git, so run it in a git checkout: {error}")
        if args.json:
            print(json.dumps({"mode": "scripts", "tests": scripts, "unregistered": unrun, "unselected": unselected,
                              "registered": sorted(set(scripts) - set(unrun))}, sort_keys=True))
            return int(bool(unrun or unselected))
        for name in unrun:
            print(f"error: {name} is a test script that no add_test or workflow runs")
        for name, classes in unselected.items():
            print(f"error: {name} runs only some of its test classes; never run: {', '.join(classes)}")
        if unrun:
            print(f"{len(unrun)} test script(s) would never run: register each with add_test, or run it from a workflow")
        if unselected:
            print(f"{len(unselected)} test script(s) leave test classes unrun: name each class in an add_test, or run the whole script")
        if unrun or unselected:
            return 1
        print(f"{len(scripts)} test script(s), each run by an add_test or a workflow")
        return 0
    try:
        every = sorted(built(args.build))
        run = registered(args.build)
        skipped = skipped_libraries(args.build)
        unbuilt = None if skipped is None else unbuilt_exports(source_directory(args.build), compiled(args.build), skipped)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.error(str(error))
    if args.list:
        for name in every:
            print(f"{'run' if name in run else 'NOT RUN'}  {name}")
        return 0
    unrun = sorted(set(every) - run)
    stale = sorted(KNOWN_UNRUN & run)
    fresh = sorted(set(unrun) - KNOWN_UNRUN)
    if args.json:
        print(json.dumps({"mode": "executables", "tests": every, "registered": sorted(set(every) & run),
                          "unregistered": unrun, "known_unrun": sorted(set(unrun) & KNOWN_UNRUN),
                          "stale_exceptions": stale, "new_unregistered": fresh,
                          "unbuilt_exports": None if unbuilt is None else [path.as_posix() for path in unbuilt]}, sort_keys=True))
        return int(not every or bool(stale or fresh or unbuilt))
    if not every:
        print(f"no test executables found under {args.build}; is it configured with -DBUILD_TESTING=ON?")
        return 1
    for name in sorted(set(unrun) & KNOWN_UNRUN):
        print(f"note: {name} is built and not run, as recorded")
    for name in sorted(skipped or ()):
        print(f"note: {name} is not built in this ASan/TSan configuration")
    for name in stale:
        print(f"error: {name} is run by ctest now; remove it from KNOWN_UNRUN")
    for name in fresh:
        print(f"error: {name} is built but no ctest test runs it")
    if fresh:
        print(f"{len(fresh)} test executable(s) would never run: ctest reports a full pass over what it knows, so a test nobody registered is invisible in CI")
    for path in unbuilt or []:
        print(f"error: {path.as_posix()} defines APS5_VABI functions but no target compiles it")
    if unbuilt:
        print(f"{len(unbuilt)} library source(s) are never built: their exports are counted but never exported; list each in its library's add_library or target_sources")
    if stale or fresh or unbuilt:
        return 1
    print(f"{len(every)} test executable(s), {len(unrun)} built and not run as recorded, none new")
    if unbuilt is None:
        print("the core/libs/prx libraries are not part of this build; library sources were not checked")
    else:
        print("every core/libs/prx source that defines an APS5_VABI function is compiled")
    return 0


if __name__ == "__main__":
    sys.exit(main())
