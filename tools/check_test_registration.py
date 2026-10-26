import argparse
import re
import subprocess
import sys
from pathlib import Path

TEST_OUTPUT = r"tests[\\/]"
COMMAND = re.compile(r"^\s*(add_test|set_tests_properties|subdirs)\s*\(", re.M | re.I)
ARGUMENT = re.compile(r'"(?:\\.|[^"\\])*"|\[(?P<equals>=*)\[.*?\](?P=equals)\]|[^\s()]+', re.S)
LEXICAL = re.compile(r'\\.|"(?:\\.|[^"\\])*"|\[(?P<literal_equals>=*)\[.*?\](?P=literal_equals)\]'
                     r'|(?P<comment>\#\[(?P<comment_equals>=*)\[.*?\](?P=comment_equals)\]|\#[^\n]*)', re.S)

KNOWN_UNRUN = set()
SCRIPT = re.compile(r"(?<![\w./-])(?:[\w.-]+/)*test_\w+\.py(?![\w./-])")


def built(build):
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


def commands(content):
    content = LEXICAL.sub(lambda match: re.sub(r"[^\n]", " ", match.group())
                          if match.group("comment") is not None else match.group(), content)
    code = LEXICAL.sub(lambda match: re.sub(r"[^\n]", "_", match.group()), content)
    parsed_until = 0
    for command in COMMAND.finditer(code):
        if command.start() < parsed_until:
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
                    break
                continue
            token = ARGUMENT.match(content, position)
            if token is None:
                raise ValueError(f"invalid CTest argument at position {position}")
            argument = token.group(0)
            if argument.startswith('"'):
                argument = re.sub(r"\\(\r?\n|.)",
                                  lambda match: {"\n": "", "\r\n": "", "n": "\n", "r": "\r", "t": "\t"}.get(match[1], match[1]),
                                  argument[1:-1], flags=re.S)
            elif token.group("equals") is not None:
                width = len(token.group("equals")) + 2
                argument = argument[width:-width].removeprefix("\n")
            arguments.append(argument)
            position = token.end()


def registered(build):
    names = set()
    pending = [Path(build).resolve()]
    visited = set()
    while pending:
        directory = pending.pop().resolve()
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


def run_scripts(root, files):
    run = set()
    for name in files:
        if name.endswith("CMakeLists.txt") or name.endswith(".cmake"):
            content = (Path(root) / name).read_text(encoding="utf-8", errors="replace")
            for command, arguments in commands(content):
                if command == "add_test":
                    run.update(argument.rpartition("}")[2].lstrip("/") for argument in arguments)
        elif name.startswith(".github/workflows/") and name.endswith((".yml", ".yaml")):
            for line in (Path(root) / name).read_text(encoding="utf-8", errors="replace").splitlines():
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
    parser = argparse.ArgumentParser(description="fail when a test executable is built that no ctest test runs")
    parser.add_argument("build", type=Path, nargs="?", default=Path("build"), help="configured build directory")
    parser.add_argument("--list", action="store_true", help="print every test executable and whether ctest runs it")
    parser.add_argument("--scripts", action="store_true", help="instead check that an add_test or a workflow runs every test_*.py of the source tree")
    args = parser.parse_args(argv)
    if args.scripts:
        scripts, unrun = unrun_scripts(Path(__file__).resolve().parents[1])
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
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.error(str(error))
    if args.list:
        for name in every:
            print(f"{'run' if name in run else 'NOT RUN'}  {name}")
        return 0
    unrun = sorted(set(every) - run)
    if not every:
        print(f"no test executables found under {args.build}; is it configured with -DBUILD_TESTING=ON?")
        return 1
    for name in sorted(set(unrun) & KNOWN_UNRUN):
        print(f"note: {name} is built and not run, as recorded")
    stale = sorted(KNOWN_UNRUN & run)
    for name in stale:
        print(f"error: {name} is run by ctest now; remove it from KNOWN_UNRUN")
    fresh = sorted(set(unrun) - KNOWN_UNRUN)
    for name in fresh:
        print(f"error: {name} is built but no ctest test runs it")
    if fresh:
        print(f"{len(fresh)} test executable(s) would never run: ctest reports a full pass over what it knows, so a test nobody registered is invisible in CI")
    if stale or fresh:
        return 1
    print(f"{len(every)} test executable(s), {len(unrun)} built and not run as recorded, none new")
    return 0


if __name__ == "__main__":
    sys.exit(main())
