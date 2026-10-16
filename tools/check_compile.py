#!/usr/bin/env python3
import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

CPP = (".cpp", ".cc", ".cxx", ".c")
DROP = ("-c", "-MD", "-MMD", "-MP")
DROP_WITH_VALUE = ("-o", "-MT", "-MF")


def capture(command, cwd=None):
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                          errors="replace", cwd=cwd)


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
        return json.loads(manifest.read_text(encoding="utf-8"))
    ninja = shutil.which("ninja")
    if ninja is None:
        raise ValueError(f"no {manifest} and ninja is not on PATH")
    result = capture([ninja, "-C", str(build), "-t", "compdb"])
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
        commands.setdefault(entry_key(entry, root), entry)
    return commands


def changed(root, base):
    result = capture(["git", "-C", str(root), "diff", "--name-only", "--diff-filter=ACMR", "-z", base])
    if result.returncode != 0:
        raise ValueError(f"git diff {base}: {result.stderr.strip()}")
    untracked = capture(["git", "-C", str(root), "ls-files", "--others", "--exclude-standard", "-z"])
    if untracked.returncode != 0:
        raise ValueError(f"git ls-files: {untracked.stderr.strip()}")
    names = dict.fromkeys(result.stdout.split("\0") + untracked.stdout.split("\0"))
    return [name for name in names if name.endswith(CPP)]


def syntax_only(entry, source):
    arguments = shlex.split(entry["command"])
    directory = Path(entry["directory"])
    wanted = source.resolve()
    trimmed = []
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in DROP_WITH_VALUE:
            index += 2
            continue
        if argument in DROP:
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


def check(root, build, files):
    commands = index(build, root)
    checked = 0
    failed = 0
    for file in files:
        source = file.resolve()
        name = os.path.relpath(source, root).replace(os.sep, "/")
        entry = commands.get(name)
        if entry is None:
            print(f"{name}: no compile command in {build}", file=sys.stderr)
            continue
        checked += 1
        result = subprocess.run(syntax_only(entry, source), cwd=entry["directory"],
                                capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode == 0:
            continue
        failed += 1
        output = (result.stdout or "") + (result.stderr or "")
        sys.stderr.write(output if output.endswith("\n") else output + "\n")
    return checked, failed


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="compile the changed translation units only, to check a change builds without a full build")
    parser.add_argument("--build", type=Path, default=Path("build"),
                        help="a configured Ninja build directory of this checkout (default: build)")
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
        checked, failed = check(root, args.build, files)
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
