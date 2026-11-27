#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from contextlib import nullcontext
from functools import cache
from pathlib import Path, PureWindowsPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
import base_ref

CPP =(".cpp", ".cc", ".cxx", ".c")
HEADERS = (".h", ".hh", ".hpp", ".hxx", ".inc", ".inl", ".ipp", ".tpp")
DROP = ("-c", "-MD", "-MMD", "-MP", "-fsyntax-only", "-fno-syntax-only", "-save-temps", "--save-temps",
        "-save-temps=cwd", "-save-temps=obj", "--save-temps=cwd", "--save-temps=obj")
DROP_WITH_VALUE = ("-o", "-MT", "-MF", "-MQ", "-MJ", "-serialize-diagnostics", "--serialize-diagnostics")
PREPROCESSING = ("-E", "--preprocess", "-M", "--dependencies", "-MM", "--user-dependencies",
                 "-MG", "--print-missing-file-dependencies", "-P", "--no-line-commands",
                 "-C", "-CC", "-dM", "-dD", "-dI")
DRIVER_QUERIES = ("--version", "--help", "-help", "--help-hidden", "--target-help", "--autocomplete", "-###", "-fdriver-only",
                  "-dumpversion", "-dumpfullversion", "-dumpmachine", "-dumpspecs", "-ccc-print-phases", "-ccc-print-bindings",
                  "-print-file-name", "-print-prog-name", "-print-search-dirs", "-print-libgcc-file-name", "-print-multiarch",
                  "-print-multi-directory", "-print-multi-lib", "-print-multi-os-directory", "-print-sysroot",
                  "-print-sysroot-headers-suffix", "-print-cxx-stdlib", "-print-cxx-stdlib-include-dirs",
                  "-print-diagnostic-categories", "-print-diagnostic-options", "-print-effective-triple", "-print-enabled-extensions",
                  "-print-library-module-manifest-path", "-print-multi-flags-experimental", "-print-resource-dir",
                  "-print-rocm-search-dirs", "-print-runtime-dir", "-print-supported-cpus", "-print-supported-extensions", "-print-target-triple", "-print-targets")
WITH_VALUE = ("-I", "-L", "-F", "-B", "-D", "-U", "-x", "-include", "--include", "-imacros", "--imacros",
              "--define-macro", "--undefine-macro", "-iquote", "-isystem", "-idirafter", "-iprefix",
              "-iwithprefix", "-iwithprefixbefore", "-isysroot", "--sysroot", "-target", "--target",
              "-arch", "-std", "-T", "-u", "-e", "--param", "-Xclang", "-Xpreprocessor", "-Xassembler", "-Xlinker",
              "-fdebug-compilation-dir", "-fcoverage-compilation-dir", "-resource-dir", "-working-directory",
              "-include-pch", "-mllvm", "-fprofile-list", "-fprofile-remapping-file", "-fcrash-diagnostics-dir",
              "-fpass-plugin", "-fplugin", "-load", "-fprofile-use", "-fprofile-sample-use", "-fprofile-instr-use")
WITHOUT_VALUE = (*DROP, *PREPROCESSING, "-S", "-fsyntax-only", "-Werror", "-Wall", "-Wextra",
                 "-pedantic", "-pedantic-errors", "-pthread", "-pipe", "-v")
JOINED_OPTIONS = ("-I", "-L", "-F", "-B", "-D", "-U", "-O", "-g", "-W")
FORWARDED_SAFE = ("-fno-spell-checking", "-fno-caret-diagnostics", "-fno-color-diagnostics",
                  "-fno-diagnostics-show-option")
FORWARDED_SAFE_JOINED = ("-D", "-U", "-I", "-F", "-W", "-include", "--include", "-imacros", "--imacros",
                         "-isystem", "-iquote", "-idirafter", "-isysroot")
COMPILER_WRAPPERS = frozenset(("ccache", "sccache", "distcc"))
MAX_RESPONSE_FILE_DEPTH = 20


def capture(command, cwd=None, encoding="utf-8"):
    return subprocess.run(command, capture_output=True, text=True, encoding=encoding,
                          errors="replace", cwd=cwd)


def real(path):
    return Path(os.path.realpath(path))


def ninja_encoding(ninja):
    if sys.platform != "win32":
        return "utf-8"
    result = capture([ninja, "-t", "wincodepage"])
    if result.returncode != 0:
        raise ValueError(f"ninja -t wincodepage: {result.stderr.strip() or result.stdout.strip()}")
    declared = result.stdout.strip()
    encodings = {"Build file encoding: UTF-8": "utf-8", "Build file encoding: ANSI": "mbcs"}
    if declared not in encodings:
        raise ValueError(f"unsupported Ninja build-file encoding: {declared!r}")
    return encodings[declared]


def repository():
    result = capture(["git", "rev-parse", "--show-toplevel"])
    if result.returncode != 0:
        raise ValueError(result.stderr.strip() or "not a git checkout")
    root = Path(result.stdout.strip())
    if os.name == "nt" and not root.is_dir():
        relative = capture(["git", "rev-parse", "--show-cdup"]).stdout.strip()
        root = (Path.cwd() / relative).resolve()
    return root


def validate_arguments(arguments):
    if not isinstance(arguments, list) or not arguments or any(not isinstance(argument, str) for argument in arguments) or not arguments[0]:
        raise ValueError("arguments must be a nonempty list of strings with an executable")


def _unique_compilation_object(pairs):
    record = {}
    for key, value in pairs:
        if key in record:
            raise ValueError(f"duplicate compilation database JSON key {key!r}")
        record[key] = value
    return record


def compilation_database(text, source, skip_empty_commands=False):
    try:
        database = json.loads(text, object_pairs_hook=_unique_compilation_object)
    except RecursionError as error:
        raise ValueError(f"{source}: compilation database is nested too deeply") from error
    if not isinstance(database, list):
        raise ValueError(f"{source}: expected an array of command objects")
    commands = []
    for index, entry in enumerate(database):
        context = f"{source}: entry {index}"
        if not isinstance(entry, dict):
            raise ValueError(f"{context}: expected a command object")
        for field in ("directory", "file"):
            if field not in entry:
                raise ValueError(f"{context}: missing {field}")
            if not isinstance(entry[field], str) or not entry[field]:
                raise ValueError(f"{context}: {field} must be a nonempty string")
        if skip_empty_commands and "arguments" not in entry and entry.get("command") == "":
            continue
        if "arguments" in entry:
            try:
                validate_arguments(entry["arguments"])
            except ValueError as error:
                raise ValueError(f"{context}: {error}") from error
        elif "command" not in entry:
            raise ValueError(f"{context}: expected arguments or command")
        elif not isinstance(entry["command"], str) or not entry["command"].strip():
            raise ValueError(f"{context}: command must be a nonempty string")
        values = [entry["directory"], entry["file"]]
        if "arguments" in entry:
            values.extend(entry["arguments"])
        else:
            values.append(entry["command"])
        if any("\0" in value for value in values):
            raise ValueError(f"{context}: NUL byte in compilation command fields")
        commands.append(entry)
    return commands


def ninja_for_build(build):
    try:
        lines = (build / "CMakeCache.txt").read_text(encoding="utf-8-sig", errors="replace").splitlines()
    except OSError:
        lines = ()
    values = {}
    for line in lines:
        key, separator, value = line.partition("=")
        name = key.partition(":")[0]
        if separator and name in ("CMAKE_GENERATOR", "CMAKE_MAKE_PROGRAM"):
            values[name] = value
    program = values.get("CMAKE_MAKE_PROGRAM", "")
    if values.get("CMAKE_GENERATOR") in ("Ninja", "Ninja Multi-Config") and program and not program.endswith("-NOTFOUND"):
        executable = shutil.which(program)
        if executable is not None:
            return executable
    return shutil.which("ninja")


def entries(build):
    if not build.is_dir():
        raise ValueError(f"{build} is not a directory; configure the build first")
    manifest = build / "compile_commands.json"
    if manifest.is_file():
        return compilation_database(manifest.read_text(encoding="utf-8-sig"), manifest)
    ninja = ninja_for_build(build)
    if ninja is None:
        raise ValueError(f"no {manifest} and ninja is not on PATH")
    result = capture([ninja, "-C", str(build), "-t", "compdb", "-x"], encoding=ninja_encoding(ninja))
    if result.returncode != 0:
        raise ValueError(f"ninja -t compdb: {result.stderr.strip()}")
    return compilation_database(result.stdout, "ninja -t compdb", skip_empty_commands=True)


def entry_key(entry, root, resolve=real):
    source = Path(entry["file"])
    if not source.is_absolute():
        source = resolve(entry["directory"]) / source
    source = resolve(os.fspath(source))
    try:
        return source.relative_to(resolve(os.fspath(root))).as_posix()
    except ValueError:
        return source.as_posix()


def index(build, root):
    resolve = cache(real)
    commands = {}
    for entry in entries(build):
        commands.setdefault(entry_key(entry, root, resolve), []).append(entry)
    return commands


def changed(root, base, suffixes=CPP):
    fork = capture(["git", "-C", str(root), "merge-base", base, "HEAD"])
    if fork.returncode != 0:
        raise ValueError(f"git merge-base {base}: {fork.stderr.strip()}")
    result = capture(["git", "-C", str(root), "diff", "--name-only", "--diff-filter=ACMR", "-z", fork.stdout.strip()])
    if result.returncode != 0:
        raise ValueError(f"git diff {base}: {result.stderr.strip()}")
    untracked = capture(["git", "-C", str(root), "ls-files", "--others", "--exclude-standard", "-z"])
    if untracked.returncode != 0:
        raise ValueError(f"git ls-files: {untracked.stderr.strip()}")
    names = dict.fromkeys(result.stdout.split("\0") + untracked.stdout.split("\0"))
    return [name for name in names if name.endswith(suffixes)]


def command_arguments(command):
    if os.name != "nt":
        return shlex.split(command)
    import ctypes
    from ctypes import wintypes
    count = ctypes.c_int()
    split = ctypes.WinDLL("shell32", use_last_error=True).CommandLineToArgvW
    split.argtypes = (wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int))
    split.restype = ctypes.POINTER(wintypes.LPWSTR)
    free = ctypes.WinDLL("kernel32", use_last_error=True).LocalFree
    free.argtypes = (wintypes.HLOCAL,)
    free.restype = wintypes.HLOCAL
    arguments = split(command, ctypes.byref(count))
    if not arguments:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        return [arguments[index] for index in range(count.value)]
    finally:
        free(arguments)


def syntax_only(entry, source):
    arguments = command_arguments(entry["command"])
    directory = Path(entry["directory"])
    return syntax_arguments(arguments, directory, source)


def response_arguments(text):
    if "\0" in text or "\x1a" in text:
        raise ValueError("unsupported response-file control character")
    quote = None
    index = 0
    while index < len(text):
        character = text[index]
        if quote is None and character in "\v\f":
            raise ValueError("unsupported response-file control character")
        if quote is not None and character == "\r" and text[index + 1:index + 2] == "\n":
            raise ValueError("quoted response-file CRLF has compiler-specific meanings")
        if character == "\\":
            following = text[index + 1:index + 2]
            if quote == "'" or following in ("\r", "\n") or (quote == '"' and following not in ('"', "\\", "")):
                raise ValueError("response-file escape has different GCC and Clang meanings")
            index += 2
            continue
        if character == quote:
            quote = None
        elif quote is None and character in ("'", '"'):
            quote = character
        index += 1
    if text.startswith("\ufeff"):
        raise ValueError("response-file byte order marks have different GCC and Clang meanings")
    arguments = shlex.split(text)
    if "" in arguments:
        raise ValueError("empty response-file options have different GCC and Clang meanings")
    return arguments


def windows_response_text(data):
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    if data.startswith(b"\xef\xbb\xbf"):
        return data[3:].decode("utf-8")
    return data.decode("utf-8")


def windows_response_arguments(text):
    if "\0" in text:
        raise ValueError("unsupported response-file control character")
    arguments = []
    token = []
    state = "init"
    index = 0
    while index < len(text):
        character = text[index]
        if character == "\\":
            end = index
            while end < len(text) and text[end] == "\\":
                end += 1
            if text[end:end + 1] == '"':
                token.append("\\" * ((end - index) // 2))
                if (end - index) % 2:
                    token.append('"')
                    end += 1
            else:
                token.append(text[index:end])
            if state == "init":
                state = "unquoted"
            index = end
            continue
        if state == "quoted":
            if character == '"' and text[index + 1:index + 2] == '"':
                token.append('"')
                index += 1
            elif character == '"':
                state = "unquoted"
            else:
                token.append(character)
        elif character in " \t\r\n":
            if state == "unquoted":
                arguments.append("".join(token))
                token = []
                state = "init"
        elif character == '"':
            state = "quoted"
        else:
            token.append(character)
            state = "unquoted"
        index += 1
    if state != "init":
        arguments.append("".join(token))
    return arguments


def response_quoting(arguments):
    quoting = "posix"
    for argument in arguments:
        if argument.startswith("--rsp-quoting="):
            quoting = argument.partition("=")[2]
            if quoting not in ("posix", "windows"):
                raise ValueError(f"unsupported response-file quoting {argument}")
    return quoting


def expand_response_files(arguments, directory, seen=frozenset(), windows=False):
    expanded = []
    for argument in arguments:
        if argument.startswith("@") and len(argument) > 1:
            response = Path(argument[1:])
            if not response.is_absolute():
                response = directory / response
            resolved = real(response)
            if resolved in seen:
                raise ValueError(f"response file cycle at {resolved}")
            if len(seen) >= MAX_RESPONSE_FILE_DEPTH:
                raise ValueError(f"response file nesting exceeds {MAX_RESPONSE_FILE_DEPTH} levels at {resolved}")
            data = response.read_bytes()
            if windows:
                nested = windows_response_arguments(windows_response_text(data))
            else:
                nested = response_arguments(data.decode("utf-8"))
            expanded.extend(expand_response_files(nested, directory, seen | {resolved}, windows))
        else:
            expanded.append(argument)
    return expanded


def preprocessor_arguments(argument):
    forwarded = argument[4:].split(",")
    if any(option.startswith("@") for option in forwarded):
        raise ValueError(f"unsupported forwarded compiler option response file in {argument}")
    options = iter(forwarded)
    kept = []
    for option in options:
        if option in ("-MD", "-MMD", "-MF", "-MT", "-MQ", "--write-dependencies", "--write-user-dependencies"):
            if not next(options, None):
                raise ValueError(f"missing value for {option} in {argument}")
            continue
        if option in ("-M", "-MM", "-MG", "-MP", "--dependencies", "--user-dependencies",
                      "--print-missing-file-dependencies") or option.startswith(("-MF", "-MT", "-MQ")):
            continue
        kept.append(option)
        if option in ("-D", "-U", "-I", "-F", "-include", "--include", "-imacros", "--imacros",
                      "-isystem", "-iquote", "-idirafter", "-isysroot", "-iprefix", "-iwithprefix",
                      "-iwithprefixbefore", "-isystem-after", "-iframework", "--define-macro",
                      "--undefine-macro"):
            value = next(options, None)
            if not value:
                raise ValueError(f"missing operand for {option} in {argument}")
            kept.append(value)
        elif not (option.startswith(("-D", "-U", "-I", "-F")) and len(option) > 2):
            raise ValueError(f"unsupported forwarded compiler option {option} in {argument}")
    return "-Wp," + ",".join(kept) if kept else None


def launcher_prefix(arguments):
    if not arguments:
        return []
    executable = PureWindowsPath(arguments[0]).stem.lower()
    if executable == "cmake":
        if arguments[1:3] != ["-E", "env"]:
            return []
        index = 3
        while index < len(arguments):
            argument = arguments[index]
            if argument == "--":
                index += 1
                break
            if argument == "--modify":
                if index + 1 == len(arguments):
                    raise ValueError("missing operand for cmake -E env --modify")
                index += 2
                continue
            if argument.startswith("--unset="):
                index += 1
                continue
            if argument.startswith("-"):
                raise ValueError(f"unsupported cmake -E env option {argument}")
            if "=" not in argument:
                break
            index += 1
        if index == len(arguments):
            raise ValueError("cmake -E env launcher has no command")
        return arguments[:index]
    if executable not in COMPILER_WRAPPERS:
        return []
    if len(arguments) < 2:
        raise ValueError(f"{executable} launcher has no command")
    if arguments[1].startswith("-"):
        raise ValueError(f"{executable} launcher options are not supported")
    return arguments[:1]


def syntax_arguments(arguments, directory, source):
    validate_arguments(arguments)
    if prefix := launcher_prefix(arguments):
        return [*prefix, *syntax_arguments(arguments[len(prefix):], directory, source)]
    windows = False
    if any(argument.startswith("@") for argument in arguments[1:]):
        compiler = Path(arguments[0]).stem.lower()
        if compiler in ("cl", "clang-cl") or "--driver-mode=cl" in arguments:
            raise ValueError("cl-mode compiler response files are unsupported; provide expanded arguments")
        windows = response_quoting(arguments[1:]) == "windows"
    arguments = [arguments[0], *expand_response_files(arguments[1:], directory, windows=windows)]
    wanted = real(source)
    trimmed = [arguments[0]]
    index = 1
    positional = False
    syntax_added = False
    source_added = False
    while index < len(arguments):
        argument = arguments[index]
        if not syntax_added and argument.startswith("-"):
            trimmed.append("-fsyntax-only")
            syntax_added = True
        if not positional and argument == "--":
            positional = True
            trimmed.append(argument)
            index += 1
            continue
        if not positional and (argument in ("-Xclang", "-Xpreprocessor") or argument.startswith(("-Xclang=", "-Xpreprocessor="))):
            option, separator, forwarded = argument.partition("=")
            if not separator:
                if index + 1 == len(arguments):
                    raise ValueError(f"missing operand for {argument}")
                forwarded = arguments[index + 1]
                step = 2
            else:
                step = 1
            joined = any(forwarded.startswith(prefix) and len(forwarded) > len(prefix)
                         for prefix in FORWARDED_SAFE_JOINED)
            if not (forwarded in FORWARDED_SAFE or joined):
                raise ValueError(f"unsupported forwarded compiler option {forwarded}")
            trimmed.extend([argument, forwarded] if not separator else [argument])
            index += step
            continue
        if not positional and argument in (*DROP_WITH_VALUE, *WITH_VALUE):
            if index + 1 == len(arguments):
                raise ValueError(f"missing operand for {argument}")
            if argument in WITH_VALUE:
                trimmed.extend(arguments[index:index + 2])
            index += 2
            continue
        if not positional and argument.startswith("-Wp,"):
            if forwarded := preprocessor_arguments(argument):
                trimmed.append(forwarded)
            index += 1
            continue
        if not positional and argument.startswith(("-save-temps=", "--save-temps=")):
            index += 1
            continue
        if not positional and (argument in (*DROP, *PREPROCESSING) or argument.startswith(("-MT", "-MF", "-MQ", "-MJ"))):
            index += 1
            continue
        if not positional and argument in ("-fdiagnostics-format=json-file", "-fdiagnostics-format=sarif-file"):
            argument = argument.removesuffix("-file") + "-stderr"
        query, _, value = argument.partition("=")
        if query.startswith("--print-"):
            query = query[1:]
        if not positional and (query in DRIVER_QUERIES or (query in ("-mcpu", "-mtune") and value == "help")):
            raise ValueError(f"compiler option {argument} bypasses compilation")
        if positional or not argument.startswith("-"):
            candidate = Path(argument)
            if not candidate.is_absolute():
                candidate = directory / candidate
            candidate = real(candidate)
            if candidate == wanted:
                if not syntax_added:
                    trimmed.append("-fsyntax-only")
                    syntax_added = True
                if not source_added:
                    trimmed.append(str(wanted))
                    source_added = True
                index += 1
                continue
            if positional:
                trimmed.append(str(candidate))
                index += 1
                continue
        elif (argument not in WITHOUT_VALUE and not argument.startswith(JOINED_OPTIONS) and "=" not in argument and index + 1 < len(arguments)
              and arguments[index + 1] in PREPROCESSING):
            raise ValueError(f"unknown operand count for {argument} before {arguments[index + 1]}")
        trimmed.append(argument)
        index += 1
    if not syntax_added:
        trimmed.append("-fsyntax-only")
    if not source_added:
        trimmed.append(str(wanted))
    return trimmed


def check_source(task):
    source, name, build, configurations = task
    if configurations is None:
        return 0, 0, f"{name}: no compile command in {build}\n"
    failed = False
    diagnostics = []
    for entry in configurations:
        command = (syntax_arguments(entry["arguments"], Path(entry["directory"]), source)
                   if "arguments" in entry else syntax_only(entry, source))
        result = subprocess.run(command, cwd=entry["directory"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode == 0:
            continue
        failed = True
        output = (result.stdout or "") + (result.stderr or "")
        diagnostics.append(output if output.endswith("\n") else output + "\n")
    return 1, int(failed), "".join(diagnostics)


def resolve_source(file, root):
    try:
        file.stat()
    except FileNotFoundError:
        pass
    source = real(file)
    try:
        name = source.relative_to(real(root)).as_posix()
    except ValueError:
        name = source.as_posix()
    return source, name


def compilation_output(entry):
    output = entry.get("output")
    if output is not None:
        return output if isinstance(output, str) and output and "\0" not in output else None
    try:
        arguments = entry["arguments"] if "arguments" in entry else command_arguments(entry["command"])
        start = len(launcher_prefix(arguments)) + 1
    except (OSError, ValueError):
        return None
    operands = iter(arguments[start:])
    for argument in operands:
        if argument == "--":
            break
        if argument == "-o":
            output = next(operands, None)
        elif argument in (*WITH_VALUE, *DROP_WITH_VALUE):
            next(operands, None)
    return output or None


def ninja_header_owners(build, outputs, headers, normalize):
    from select_tests import dependency_path

    program = ninja_for_build(build)
    if program is None:
        raise ValueError("--headers needs Ninja and a built Ninja project")
    result = subprocess.run([program, "-C", str(build), "-t", "deps"], capture_output=True)
    if result.returncode:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(f"ninja -t deps failed with exit code {result.returncode}: {detail}")
    recorded = {}
    stale = set()
    current = None
    for line in result.stdout.splitlines():
        if line.startswith(b"    ") and current is not None:
            header = normalize(build, dependency_path(line[4:], build))
            if header in headers:
                current.add(header)
            continue
        current = None
        name, separator, status = line.rpartition(b": #deps ")
        if not separator:
            continue
        output = normalize(build, dependency_path(name, build))
        if output not in outputs:
            continue
        if status.endswith(b" (VALID)"):
            current = recorded.setdefault(output, set())
        elif status.endswith(b" (STALE)"):
            stale.add(output)
    return recorded, stale


def header_configurations(build, commands, headers):
    @cache
    def normalize(directory, name):
        return os.path.normcase(real(Path(directory) / name))

    wanted = {normalize(build, path): path for path in headers}
    configurations = []
    for name, entries_for_source in commands.items():
        if name.endswith(CPP):
            for entry in entries_for_source:
                output = compilation_output(entry)
                configurations.append((name, entry, normalize(entry["directory"], output) if output else None))
    outputs = {output for _, _, output in configurations if output is not None}
    recorded, stale = ninja_header_owners(build, outputs, wanted, normalize)
    matched = {}
    found = set()
    unknown = missing = outdated = 0
    for name, entry, output in configurations:
        if output is None:
            unknown += 1
        elif output in stale:
            outdated += 1
        elif output not in recorded:
            missing += 1
        elif recorded[output]:
            matched.setdefault(name, []).append(entry)
            found.update(recorded[output])
    if unknown or missing or outdated:
        print(f"Ninja header coverage is incomplete ({outdated} stale, {missing} missing, "
              f"{unknown} without an object output among {len(configurations)} configurations)", file=sys.stderr)
    for path, header in wanted.items():
        if path not in found:
            print(f"{header} has no compile configuration with a valid recorded Ninja dependency", file=sys.stderr)
    return matched


def compile_tasks(root, build, files, headers=False):
    commands = index(build, root)
    tasks = []
    requested_headers = []
    direct = set()
    for file in files:
        source, name = resolve_source(file, root)
        if headers and file.suffix in HEADERS:
            requested_headers.append(source)
            continue
        direct.add(name)
        tasks.append((source, name, build, commands.get(name)))
    if requested_headers:
        for name, configurations in header_configurations(build, commands, requested_headers).items():
            if name not in direct:
                tasks.append((real(Path(root) / name), name, build, configurations))
    return tasks


def check(root, build, files, jobs=1, headers=False):
    tasks = compile_tasks(root, build, files, headers)
    checked = failed = 0
    with (ThreadPoolExecutor(max_workers=jobs) if jobs > 1 else nullcontext()) as executor:
        results = executor.map(check_source, tasks) if executor is not None else map(check_source, tasks)
        for count, failures, diagnostics in results:
            checked += count
            failed += failures
            sys.stderr.write(diagnostics)
    return checked, failed


def positive_integer(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return number


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description="check C and C++ syntax using a configured build's compile commands")
    parser.add_argument("--build", type=Path, default=Path("build"),
                        help="a configured Ninja build directory of this checkout (default: build)")
    parser.add_argument("--jobs", type=positive_integer, default=1, metavar="N",
                        help="check up to N translation units concurrently; diagnostics retain input order (default 1)")
    parser.add_argument("--base", help="branch the change is against (default: upstream/main, then origin/main, then main)")
    parser.add_argument("paths", nargs="*", help="files to check instead of the changed ones")
    parser.add_argument("--headers", action="store_true",
                        help="include headers using recorded Ninja dependencies from an existing build")
    parser.add_argument("--dry-run", action="store_true", help="print planned syntax commands as JSON without executing a compiler")
    args = parser.parse_args(argv)
    try:
        root = repository()
        if args.paths:
            files = [Path(path) for path in args.paths]
        else:
            base = base_ref.resolve(root, args.base)
            suffixes = CPP + HEADERS if args.headers else CPP
            files = [root / path for path in changed(root, base, suffixes)]
            if not files:
                print(json.dumps({"translation_units": []}) if args.dry_run else f"no changed translation unit against {base}")
                return 0
        if args.dry_run:
            plan = []
            for source, _, _, configurations in compile_tasks(root, args.build, files, args.headers):
                plan.append({"source": str(source), "commands": [
                    {"directory": entry["directory"], "arguments": syntax_arguments(entry["arguments"], Path(entry["directory"]), source)
                     if "arguments" in entry else syntax_only(entry, source)} for entry in configurations or ()]})
            print(json.dumps({"translation_units": plan}, indent=2))
            return 0 if any(entry["commands"] for entry in plan) else 1
        checked, failed = check(root, args.build, files, args.jobs, args.headers)
    except (json.JSONDecodeError, OSError, ValueError) as error:
        parser.error(str(error))
    if failed:
        print(f"{failed} of {checked} translation units did not compile", file=sys.stderr)
        return 1
    if not checked:
        print(f"no translation unit to check in {args.build}", file=sys.stderr)
        return 1
    print(f"{checked} translation units compile")
    return 0


if __name__ == "__main__":
    sys.exit(main())
