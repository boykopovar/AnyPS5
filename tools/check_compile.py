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
DROP = ("-c", "-MD", "-MMD", "-MP")
DROP_WITH_VALUE = ("-o", "-MT", "-MF", "-MQ", "-MJ")


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


def entries(build):
    if not build.is_dir():
        raise ValueError(f"{build} is not a directory; configure the build first")
    manifest = build / "compile_commands.json"
    if manifest.is_file():
        return json.loads(manifest.read_text(encoding="utf-8-sig"))
    ninja = shutil.which("ninja")
    if ninja is None:
        raise ValueError(f"no {manifest} and ninja is not on PATH")
    result = capture([ninja, "-C", str(build), "-t", "compdb", "-x"], encoding=ninja_encoding(ninja))
    if result.returncode != 0:
        raise ValueError(f"ninja -t compdb: {result.stderr.strip()}")
    return json.loads(result.stdout)


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


def syntax_arguments(arguments, directory, source):
    if not isinstance(arguments, list) or not arguments or any(not isinstance(argument, str) for argument in arguments) or not arguments[0]:
        raise ValueError("arguments must be a nonempty list of strings with an executable")
    for argument in arguments[1:]:
        if argument.startswith("@"):
            raise ValueError(f"unexpanded response file {argument[1:]}; provide expanded compile arguments before checking syntax")
    wanted = source.resolve()
    trimmed = []
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in DROP_WITH_VALUE:
            index += 2
            continue
        if argument in DROP or argument.startswith(("-MT", "-MF", "-MQ", "-MJ")):
            index += 1
            continue
        if not argument.startswith("-"):
            candidate = Path(argument)
            if not candidate.is_absolute():
                candidate = directory / candidate
            if candidate.resolve() == wanted:
                index += 1
                continue
        trimmed.append(argument)
        index += 1
    trimmed += ["-fsyntax-only", str(wanted)]
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
