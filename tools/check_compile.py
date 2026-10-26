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
from pathlib import Path

CPP = (".cpp", ".cc", ".cxx", ".c")
DROP = ("-c", "-MD", "-MMD", "-MP", "-fsyntax-only", "-fno-syntax-only", "-save-temps", "--save-temps",
        "-save-temps=cwd", "-save-temps=obj", "--save-temps=cwd", "--save-temps=obj")
DROP_WITH_VALUE = ("-o", "-MT", "-MF", "-MQ", "-MJ")
PREPROCESSING = ("-E", "--preprocess", "-M", "--dependencies", "-MM", "--user-dependencies",
                 "-MG", "--print-missing-file-dependencies", "-P", "--no-line-commands",
                 "-C", "-CC", "-dM", "-dD", "-dI")
WITH_VALUE = ("-I", "-L", "-F", "-B", "-D", "-U", "-x", "-include", "--include", "-imacros", "--imacros",
              "--define-macro", "--undefine-macro", "-iquote", "-isystem", "-idirafter", "-iprefix",
              "-iwithprefix", "-iwithprefixbefore", "-isysroot", "--sysroot", "-target", "--target",
              "-arch", "-std", "-T", "-u", "-e", "--param", "-Xclang", "-Xpreprocessor", "-Xassembler", "-Xlinker")
WITHOUT_VALUE = (*DROP, *PREPROCESSING, "-S", "-fsyntax-only", "-Werror", "-Wall", "-Wextra",
                 "-pedantic", "-pedantic-errors", "-pthread", "-pipe", "-v")
JOINED_OPTIONS = ("-I", "-L", "-F", "-B", "-D", "-U", "-O", "-g", "-W")


def capture(command, cwd=None, encoding="utf-8"):
    return subprocess.run(command, capture_output=True, text=True, encoding=encoding,
                          errors="replace", cwd=cwd)


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
    return Path(result.stdout.strip())


def validate_arguments(arguments):
    if not isinstance(arguments, list) or not arguments or any(not isinstance(argument, str) for argument in arguments) or not arguments[0]:
        raise ValueError("arguments must be a nonempty list of strings with an executable")


def compilation_database(text, source, skip_empty_commands=False):
    database = json.loads(text)
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
        commands.append(entry)
    return commands


def entries(build):
    if not build.is_dir():
        raise ValueError(f"{build} is not a directory; configure the build first")
    manifest = build / "compile_commands.json"
    if manifest.is_file():
        return compilation_database(manifest.read_text(encoding="utf-8-sig"), manifest)
    ninja = shutil.which("ninja")
    if ninja is None:
        raise ValueError(f"no {manifest} and ninja is not on PATH")
    result = capture([ninja, "-C", str(build), "-t", "compdb", "-x"], encoding=ninja_encoding(ninja))
    if result.returncode != 0:
        raise ValueError(f"ninja -t compdb: {result.stderr.strip()}")
    return compilation_database(result.stdout, "ninja -t compdb", skip_empty_commands=True)


def entry_key(entry, root):
    directory = Path(entry["directory"]).resolve()
    source = Path(entry["file"])
    if not source.is_absolute():
        source = directory / source
    source = source.resolve()
    try:
        return source.relative_to(Path(root).resolve()).as_posix()
    except ValueError:
        return source.as_posix()


def index(build, root):
    commands = {}
    for entry in entries(build):
        commands.setdefault(entry_key(entry, root), []).append(entry)
    return commands


def changed(root, base):
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
    return [name for name in names if name.endswith(CPP)]


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
                      "-isystem", "-iquote", "-idirafter", "-isysroot"):
            value = next(options, None)
            if not value:
                raise ValueError(f"missing operand for {option} in {argument}")
            kept.append(value)
        elif not (option.startswith(("-D", "-U", "-I", "-F")) and len(option) > 2):
            raise ValueError(f"unsupported forwarded compiler option {option} in {argument}")
    return "-Wp," + ",".join(kept) if kept else None


def syntax_arguments(arguments, directory, source):
    validate_arguments(arguments)
    for argument in arguments[1:]:
        if argument.startswith("@"):
            raise ValueError(f"unexpanded response file {argument[1:]}; provide expanded compile arguments before checking syntax")
    wanted = source.resolve()
    trimmed = [arguments[0]]
    index = 1
    positional = False
    syntax_added = False
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
            raise ValueError(f"unsupported forwarded compiler option {argument}")
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
        if not positional and (argument in (*DROP, *PREPROCESSING) or argument.startswith(("-MT", "-MF", "-MQ", "-MJ"))):
            index += 1
            continue
        if positional or not argument.startswith("-"):
            candidate = Path(argument)
            if not candidate.is_absolute():
                candidate = directory / candidate
            if candidate.resolve() == wanted:
                index += 1
                continue
            if positional:
                trimmed.append(str(candidate.resolve()))
                index += 1
                continue
        elif (argument not in WITHOUT_VALUE and not argument.startswith(JOINED_OPTIONS) and "=" not in argument and index + 1 < len(arguments)
              and arguments[index + 1] in PREPROCESSING):
            raise ValueError(f"unknown operand count for {argument} before {arguments[index + 1]}")
        trimmed.append(argument)
        index += 1
    if not syntax_added:
        trimmed.append("-fsyntax-only")
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


def check(root, build, files, jobs=1):
    commands = index(build, root)
    tasks = []
    for file in files:
        source = file.resolve()
        try:
            name = source.relative_to(root.resolve()).as_posix()
        except ValueError:
            name = source.as_posix()
        tasks.append((source, name, build, commands.get(name)))
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
    parser = argparse.ArgumentParser(
        description="compile the changed translation units only, to check a change builds without a full build")
    parser.add_argument("--build", type=Path, default=Path("build"),
                        help="a configured Ninja build directory of this checkout (default: build)")
    parser.add_argument("--jobs", type=positive_integer, default=1, metavar="N",
                        help="check up to N translation units concurrently; diagnostics retain input order (default 1)")
    parser.add_argument("--base", default="origin/main",
                        help="branch the change is against (default: origin/main)")
    parser.add_argument("paths", nargs="*", help="translation units to check instead of the changed ones")
    args = parser.parse_args(argv)
    try:
        root = repository()
        files = [Path(path) for path in args.paths] or [root / path for path in changed(root, args.base)]
        if not files:
            print(f"no changed translation unit against {args.base}")
            return 0
        checked, failed = check(root, args.build, files, args.jobs)
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
