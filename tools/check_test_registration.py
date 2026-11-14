import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

TEST_OUTPUT = r"tests[\\/]"
SCOPE_START = {"if", "foreach", "while", "function", "macro", "block"}
COMMAND = re.compile(r"^\s*(add_test|set_tests_properties|set_property|subdirs|if|elseif|else|endif|foreach|endforeach|while|endwhile|function|endfunction|macro|endmacro|block|endblock)\s*\(", re.M | re.I)
ARGUMENT = re.compile(r'"(?:\\.|[^"\\])*"|\[(?P<equals>=*)\[.*?\](?P=equals)\]|[^\s()]+', re.S)
LEXICAL = re.compile(r'\\.|"(?:\\.|[^"\\])*"|\[(?P<literal_equals>=*)\[.*?\](?P=literal_equals)\]'
                     r'|(?P<comment>\#\[(?P<comment_equals>=*)\[.*?\](?P=comment_equals)\]|\#[^\n]*)', re.S)

KNOWN_UNRUN = set()
SANITIZER_SKIPPED_LIBRARIES = {"libSceAddressSanitizer"}
SCRIPT = re.compile(r"(?<![\w./-])(?:[\w.-]+/)*test_\w+\.py(?![\w./-])")
EXPORTED = re.compile(r"\bAPS5_VABI\s+[A-Za-z_]\w*\s*\([^;{}]*\)\s*(?:noexcept\s*)?(?:try\s*)?\{")
INCLUDED_SOURCE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+\.cpp)"', re.M)
COMPILE_FLAG = re.compile(r"(?:^|\s)[-/]c(?:\s|$)")

CPP_TOKEN = re.compile(
    r'(?P<comment>//[^\n]*|/\*[\s\S]*?\*/)'
    r'|(?P<raw>(?:u8|u|U|L)?R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)")'
    r'|(?P<string>(?:u8|u|U|L)?"(?:\\[\s\S]|[^"\\])*")'
    r"|(?P<char>(?:u8|u|U|L)?'(?:\\[\s\S]|[^'\\])*')"
    r"|(?P<code>(?:[0-9]|\.[0-9])[A-Za-z0-9_.']*|[A-Za-z_][A-Za-z0-9_]*|\S)")

def cpp_source(text, keep_strings=False):
    text = text.replace("\\\n", "").replace("\\\r\n", "")

    def mask(match):
        if match.group("code") is not None or keep_strings and match.group("string") is not None:
            return match.group()
        return re.sub(r"[^\n]", " ", match.group())

    return CPP_TOKEN.sub(mask, text)


def ninja(build):
    program, generator = None, None
    for line in (Path(build) / "CMakeCache.txt").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("CMAKE_MAKE_PROGRAM:"):
            program = line.partition("=")[2]
        elif line.startswith("CMAKE_GENERATOR:"):
            generator = line.partition("=")[2]
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
    return program, encoding


def built(build):
    program, encoding = ninja(build)
    listed = subprocess.run([program, "-C", str(build), "-t", "targets", "all"],
                            capture_output=True, text=True, encoding=encoding, check=True).stdout
    names = set()
    for line in listed.splitlines():
        path, _, rule = line.partition(": ")
        if "EXECUTABLE" not in rule:
            continue
        found = re.match(rf"^{TEST_OUTPUT}(?:.*[\\/])?([^\\/]+)$", path)
        if not found:
            continue
        name = found.group(1)
        if name.endswith(".exe"):
            name = name[:-4]
        if name:
            names.add(name)
    return names


def compiled(build):
    program, encoding = ninja(build)
    database = subprocess.run([program, "-C", str(build), "-t", "compdb"],
                              capture_output=True, text=True, encoding=encoding, check=True).stdout
    sources = set()
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
        if name in ("ANYPS5_ENABLE_ASAN", "ANYPS5_ENABLE_TSAN"):
            enabled = enabled or value.partition("=")[2].strip().upper() in ("ON", "1", "TRUE", "YES", "Y")
    return set(SANITIZER_SKIPPED_LIBRARIES) if enabled else set()


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


def commands(content):
    content = content.removeprefix("\ufeff")
    content = LEXICAL.sub(lambda match: re.sub(r"[^\n]", " ", match.group())
                          if match.group("comment") is not None else match.group(), content)
    code = LEXICAL.sub(lambda match: re.sub(r"[^\n]", "_", match.group()), content)
    parsed_until = 0
    for command in COMMAND.finditer(code):
        if command.start() < parsed_until:
            continue
        name = command.group(1).lower()
        if name in SCOPE_START or name.removeprefix("end") in SCOPE_START or name in {"else", "elseif"}:
            depth, position = 1, command.end()
            while position < len(code) and depth:
                depth += (code[position] == "(") - (code[position] == ")")
                position += 1
            if depth:
                raise ValueError(f"unterminated {name} command at position {command.start()}")
            parsed_until = position
            yield name, []
            continue
        position, arguments = command.end(), []
        while position < len(content):
            if content[position].isspace():
                position += 1
                continue
            if content[position] == ")":
                parsed_until = position + 1
                yield command.group(1).lower(), arguments
                break
            if content[position] == "#":
                position = content.find("\n", position)
                if position < 0:
                    position = len(content)
                continue
            token = ARGUMENT.match(content, position)
            if token is None:
                raise ValueError(f"invalid CTest argument at position {position}")
            argument = token.group(0)
            if argument.startswith('"'):
                argument = re.sub(r"\\(\r?\n|.)",
                                  lambda match: {"\n": "", "\r\n": "", "n": "\n", "r": "\r", "t": "\t", ";": r"\;"}.get(match[1], match[1]),
                                  argument[1:-1], flags=re.S)
            elif token.group("equals") is not None:
                width = len(token.group("equals")) + 2
                argument = argument[width:-width].removeprefix("\n")
            arguments.append(argument)
            position = token.end()
        else:
            raise ValueError(f"unterminated {command.group(1)} command at position {command.start()}")


def registered(build):
    names = set()
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
        tests, latest = [], {}
        for command, arguments in commands(file.read_text(encoding="utf-8", errors="replace")):
            if command == "subdirs":
                pending.extend(directory / argument for argument in arguments)
            elif command == "add_test" and arguments:
                test = [arguments[1:], False]
                tests.append(test)
                latest[arguments[0]] = test
            elif command == "set_tests_properties" and "PROPERTIES" in arguments:
                properties = arguments.index("PROPERTIES")
                values = arguments[properties + 1:]
                for key, value in zip(values[::2], values[1::2]):
                    if key == "DISABLED":
                        for name in arguments[:properties]:
                            if name in latest:
                                latest[name][1] = value.upper() in ("1", "ON", "YES", "TRUE", "Y")
        for arguments, disabled in tests:
            if disabled:
                continue
            for index, argument in enumerate(arguments):
                if index and (argument.startswith("-") or re.match(r"^[A-Za-z_]\w*=", argument)):
                    continue
                base = re.split(r"[\\/]", argument)[-1]
                if base.endswith(".exe"):
                    base = base[:-4]
                if base:
                    names.add(base)
    return names


def listed(root):
    output = subprocess.run(["git", "-c", "core.quotePath=false", "-C", str(root), "ls-files", "-z", "--cached", "--others",
                             "--exclude-standard"], capture_output=True, check=True).stdout.decode("utf-8", errors="replace")
    return sorted(set(filter(None, output.split("\0"))))


def workflow_run_lines(content):
    parents = []
    step_key_indent = None
    block_indent = None
    for line in content.splitlines():
        text = line.lstrip()
        if not text or text.startswith("#"):
            continue
        indent = len(line) - len(text)
        if block_indent is not None:
            if indent > block_indent:
                yield line
                continue
            block_indent = None
        sequence = re.match(r"-\s+", text)
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
        match = re.match(r"(['\"]?)([A-Za-z_][\w.-]*)\1\s*:(?:\s+(.*)|$)", text)
        if match is None:
            continue
        key, value = match[2], match[3] or ""
        if key == "steps" and len(path) == 2 and path[0] == "jobs":
            step_key_indent = None
        if step and key_indent == step_key_indent and key == "run":
            if value.startswith(("|", ">")):
                block_indent = key_indent
            elif value:
                yield value
        parents.append((key_indent, key))


def disabled_properties(command, arguments):
    if command == "set_tests_properties" and "PROPERTIES" in arguments:
        properties = arguments.index("PROPERTIES")
        values = arguments[properties + 1:]
        for key, value in zip(values[::2], values[1::2]):
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


def run_scripts(root, files):
    run = set()
    for name in files:
        if name.endswith("CMakeLists.txt") or name.endswith(".cmake"):
            content = (Path(root) / name).read_text(encoding="utf-8", errors="replace")
            tests = []
            scopes = []
            for index, (command, arguments) in enumerate(commands(content)):
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
                    for names, mode, value in disabled_properties(command, arguments):
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
            for _, arguments, value, _ in tests:
                if not value[1]:
                    run.update(argument.rpartition("}")[2].lstrip("/") for argument in arguments)
        elif name.startswith(".github/workflows/") and name.endswith((".yml", ".yaml")):
            for line in workflow_run_lines((Path(root) / name).read_text(encoding="utf-8", errors="replace")):
                if line.lstrip().startswith("#"):
                    continue
                line = re.split(r"\s#", line)[0]
                if "python" in line:
                    run.update(token[2:] if token.startswith("./") else token for token in SCRIPT.findall(line))
    return {name for name in run if SCRIPT.fullmatch(name)}


def unrun_scripts(root):
    files = listed(root)
    run = run_scripts(root, files)
    scripts = [name for name in files if re.match(r"^test_\w+\.py$", name.rpartition("/")[2])]
    return scripts, [name for name in scripts if not any(name == path or name.endswith("/" + path) for path in run)]


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="fail when a test executable is built that no ctest test runs, or a library source that defines exports is never compiled")
    parser.add_argument("build", type=Path, nargs="?", default=Path("build"), help="configured build directory")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--json", action="store_true", help="emit registration audit results as JSON")
    output.add_argument("--list", action="store_true", help="print every test executable and whether ctest runs it")
    parser.add_argument("--scripts", action="store_true", help="instead check that an add_test or a workflow runs every test_*.py of the source tree")
    args = parser.parse_args(argv)
    if args.scripts:
        try:
            scripts, unrun = unrun_scripts(Path(__file__).resolve().parents[1])
        except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
            parser.error(f"--scripts reads the source tree from git, so run it in a git checkout: {error}")
        if args.json:
            print(json.dumps({"mode": "scripts", "tests": scripts, "unregistered": unrun,
                              "registered": sorted(set(scripts) - set(unrun))}, sort_keys=True))
            return int(bool(unrun))
        for name in unrun:
            print(f"error: {name} is a test script that no add_test or workflow runs")
        if unrun:
            print(f"{len(unrun)} test script(s) would never run: register each with add_test, or run it from a workflow")
            return 1
        print(f"{len(scripts)} test script(s), each run by an add_test or a workflow")
        return 0
    try:
        every = sorted(built(args.build))
        run = registered(args.build)
        skipped = skipped_libraries(args.build)
        unbuilt = unbuilt_exports(Path(__file__).resolve().parents[1], compiled(args.build), skipped)
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
    for name in sorted(skipped):
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
        print("no core/libs/prx source is compiled in this build; library sources were not checked")
    else:
        print("every core/libs/prx source that defines an APS5_VABI function is compiled")
    return 0


if __name__ == "__main__":
    sys.exit(main())
