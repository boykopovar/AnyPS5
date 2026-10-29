import argparse
import collections
import importlib.util
import json
import os
import re
import subprocess
import sys
from pathlib import Path

REGISTRATION_SPEC = importlib.util.spec_from_file_location("check_test_registration", Path(__file__).with_name("check_test_registration.py"))
REGISTRATION = importlib.util.module_from_spec(REGISTRATION_SPEC)
REGISTRATION_SPEC.loader.exec_module(REGISTRATION)

FULL = re.compile(r"^CMakeLists\.txt$|^core/CMakeLists\.txt$|^core/libs/CMakeLists\.txt$|^\.github/workflows/build\.yml$"
                  r"|^3rdparty/|^cmake/|^\.gitmodules$|^tools/select_tests\.py$")
LOCAL = re.compile(r"(^|/)CMakeLists\.txt$|\.cmake$")
BATCH = 20000


class Build:
    def __init__(self, path):
        cache = {}
        for line in (Path(path) / "CMakeCache.txt").read_text(encoding="utf-8-sig").splitlines():
            name, _, value = line.partition("=")
            cache[name.partition(":")[0]] = value
        self.path = os.path.normpath(cache["CMAKE_CACHEFILE_DIR"])
        self.source = os.path.normpath(cache["CMAKE_HOME_DIRECTORY"])
        self.program = cache.get("CMAKE_MAKE_PROGRAM") or "ninja"
        self.encoding = "utf-8"
        if sys.platform == "win32":
            codepage = self.run("-t", "wincodepage").strip()
            self.encoding = {"Build file encoding: UTF-8": "utf-8", "Build file encoding: ANSI": "mbcs"}[codepage]

    def run(self, *args):
        return subprocess.run([self.program, "-C", self.path, *args], capture_output=True, text=True,
                              encoding=self.encoding, check=True).stdout

    def absolute(self, path):
        return os.path.normpath(path if os.path.isabs(path) else os.path.join(self.path, path))

    def key(self, path):
        return os.path.normcase(self.absolute(path))

    def repo(self, path):
        absolute = self.absolute(path)
        if inside(absolute, self.path) or not inside(absolute, self.source):
            return None
        return relative(absolute, self.source)

    def query(self, targets):
        found = {}
        for batch in chunks(targets):
            current, section = None, None
            for line in self.run("-t", "query", *batch).splitlines():
                if not line.startswith(" "):
                    current = line.rstrip(":")
                    found[current] = {"inputs": [], "phony": False}
                elif line.startswith("  input: "):
                    section = "inputs"
                    found[current]["phony"] = line.strip() == "input: phony"
                elif line.startswith("  outputs:"):
                    section = None
                elif section and line.startswith("    "):
                    found[current]["inputs"].append(line.strip().removeprefix("|| ").removeprefix("| "))
        return found

    def walk(self, roots):
        nodes, todo = {}, list(roots)
        while todo:
            batch = sorted({t for t in todo if t not in nodes})
            if not batch:
                break
            nodes.update(self.query(batch))
            todo = [i for t in batch for i in nodes.get(t, {"inputs": []})["inputs"]]
        return nodes

    def leaves(self):
        nodes = self.walk(["all"])
        leaves, todo, seen = set(), ["all"], set()
        while todo:
            name = todo.pop()
            if name in seen or name not in nodes:
                continue
            seen.add(name)
            if nodes[name]["phony"]:
                todo.extend(nodes[name]["inputs"])
            else:
                leaves.add(name)
        return sorted(leaves)

    def outputs(self, paths):
        built = {line.rpartition(": ")[0] for line in self.run("-t", "targets", "all").splitlines()}
        return [path for path in paths if path in built]

    def tests(self, *arguments):
        listing = json.loads(subprocess.run(["ctest", "--test-dir", self.path, "--show-only=json-v1", *arguments],
                                            capture_output=True, text=True, check=True).stdout)
        missing = {test["name"] for test in listing["tests"] if not test.get("command")}
        if missing:
            commands = self.test_commands(missing)
            for test in listing["tests"]:
                candidates = commands.get(test["name"], set())
                if test["name"] in missing and len(candidates) == 1:
                    command = next(iter(candidates))
                    if command and os.path.isabs(command[0]):
                        test["command"] = list(command)
        return listing, listing["tests"]

    def test_commands(self, names):
        found, visited, pending = collections.defaultdict(set), set(), [Path(self.path)]
        while pending:
            directory = pending.pop().resolve()
            file = directory / "CTestTestfile.cmake"
            if directory in visited or not file.is_file():
                continue
            visited.add(directory)
            try:
                commands = list(REGISTRATION.commands(file.read_text(encoding="utf-8-sig")))
            except (OSError, UnicodeError, ValueError):
                continue
            for command, arguments in commands:
                if command == "subdirs":
                    pending.extend(directory / argument for argument in arguments)
                elif command == "add_test" and arguments and arguments[0] in names:
                    found[arguments[0]].add(tuple(arguments[1:]))
        return found

    def output(self, path):
        absolute = self.absolute(path)
        if not inside(absolute, self.path):
            return None
        return relative(absolute, self.path)

    def target(self, test):
        command = test.get("command") or []
        return self.output(command[0]) if command else None


def inside(path, root):
    prefix = os.path.normcase(root).rstrip(os.sep) + os.sep
    return os.path.normcase(path).startswith(prefix)


def relative(path, root):
    root_len = len(root.rstrip(os.sep))
    return path[root_len + 1:].replace(os.sep, "/")


def paths(arguments):
    for argument in arguments:
        value = argument.partition("=")[2] if argument.startswith("-D") else argument
        if os.path.isabs(value):
            yield value


def changed_paths(path):
    data = Path(path).read_bytes()
    if b"\0" in data:
        return [name for name in data.decode("utf-8").split("\0") if name]
    text = data.decode("utf-8-sig")
    return [line.strip() for line in text.splitlines() if line.strip()]


def chunks(names):
    batch, size = [], 0
    for name in names:
        length = len(name) + 1
        if sys.platform == "win32":
            length = len(subprocess.list2cmdline([name]).encode("utf-16-le", "surrogatepass")) // 2 + 1
        if batch and size + length > BATCH:
            yield batch
            batch, size = [], 0
        batch.append(name)
        size += length
    if batch:
        yield batch


def load_graph(path):
    graph = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    if not isinstance(graph, dict) or not isinstance(graph.get("tests"), dict) or not isinstance(graph.get("closures"), dict):
        raise ValueError(f"{path}: invalid test graph (expected tests and closures objects)")
    fields = ("registered", "arguments", "sets", "requires", "depends")
    for name, test in graph["tests"].items():
        if (not isinstance(name, str) or not isinstance(test, dict) or "target" not in test
                or not (test["target"] is None or isinstance(test["target"], str))
                or any(not isinstance(test.get(field), list)
                       or any(not isinstance(item, str) for item in test[field]) for field in fields)):
            raise ValueError(f"{path}: invalid test graph entry {name!r}")
    for name, paths in graph["closures"].items():
        if not isinstance(name, str) or not isinstance(paths, list) or any(not isinstance(path, str) for path in paths):
            raise ValueError(f"{path}: invalid test graph closure {name!r}")
    for name, test in graph["tests"].items():
        if test["target"] is not None and test["target"] not in graph["closures"]:
            raise ValueError(f"{path}: invalid test graph entry {name!r}: missing closure for {test['target']!r}")
    return graph


def dump(build, out):
    deps = collections.defaultdict(set)
    current = None
    recorded = subprocess.run([build.program, "-C", build.path, "-t", "deps"], capture_output=True, check=True).stdout
    for line in recorded.splitlines():
        if not line.startswith(b" ") and b": #deps" in line:
            current = build.key(line.split(b": #deps")[0].decode(build.encoding))
        elif line.strip() and current:
            path = build.repo(line.strip().decode("utf-8"))
            if path:
                deps[current].add(path)
    listing, listed = build.tests()
    graph = listing["backtraceGraph"]
    targets = {build.target(t) for t in listed} - {None}
    nodes = build.walk(sorted(targets))
    closures = {}
    for target in sorted(targets):
        files, seen, todo = set(), set(), [target]
        while todo:
            name = todo.pop()
            if name in seen:
                continue
            seen.add(name)
            path = build.repo(name)
            if path:
                files.add(path)
            files |= deps.get(build.key(name), set())
            todo.extend(nodes.get(name, {"inputs": []})["inputs"])
        closures[target] = sorted(files)
    tests = {}
    for test in listed:
        registered, node = set(), test.get("backtrace")
        while node is not None:
            registered.add(build.repo(graph["files"][graph["nodes"][node]["file"]]))
            node = graph["nodes"][node].get("parent")
        props = {p["name"]: p["value"] for p in test.get("properties", [])}
        arguments = sorted({build.repo(a) for a in paths((test.get("command") or [])[1:])} - {None})
        tests[test["name"]] = {"registered": sorted(registered - {None}), "arguments": arguments,
                               "requires": props.get("FIXTURES_REQUIRED", []), "sets": props.get("FIXTURES_SETUP", []),
                               "depends": props.get("DEPENDS", []), "target": build.target(test)}
    with open(out, "w", encoding="utf-8") as file:
        json.dump({"tests": tests, "closures": closures}, file)
    print(f"{len(tests)} tests, {len(closures)} test executables, {len(deps)} objects with header deps")


def select(graph, changed, reasons=None):
    full = [f for f in changed if FULL.search(f)]
    if full:
        return None, full
    tests, closures = graph["tests"], {t: set(f) for t, f in graph["closures"].items()}
    known = set().union(*closures.values(), *(t["arguments"] for t in tests.values()))
    files = set(changed)
    dirs = tuple(f.rpartition("/")[0] + "/" for f in changed if "/" in f and (LOCAL.search(f) or f not in known))
    picked = set()
    for name, test in tests.items():
        scope = closures.get(test["target"], set()) | set(test["arguments"])
        if (test["target"] is None or files & set(test["registered"]) or files & scope
                or any(f.startswith(dirs) for f in scope | set(test["registered"]))):
            picked.add(name)
            if reasons is not None:
                reasons[name] = (["no build executable"] if test["target"] is None else [])
                reasons[name] += ["input: " + path for path in sorted(files & scope)]
                reasons[name] += ["registration: " + path for path in sorted(files & set(test["registered"]))]
                reasons[name] += ["directory: " + directory for directory in dirs
                                  if any(path.startswith(directory) for path in scope | set(test["registered"]))]
    setups = collections.defaultdict(set)
    for name, test in tests.items():
        for fixture in test["sets"]:
            setups[fixture].add(name)
    todo = list(picked)
    while todo:
        name = todo.pop()
        test = tests[name]
        for extra in set(test["depends"]).union(*(setups[f] for f in test["requires"])):
            if extra not in tests:
                continue
            if reasons is not None:
                fixture = set(test["requires"]) & set(tests[extra]["sets"])
                kind = "fixture setup for: " if fixture else "dependency of: "
                reasons.setdefault(extra, []).append(kind + name)
            if extra not in picked:
                picked.add(extra)
                todo.append(extra)
    return picked, []


def plan(build, graph, picked):
    _, listed = build.tests()
    run = [i for i, t in enumerate(listed, 1) if t["name"] in picked or t["name"] not in graph["tests"]]
    if not run:
        skip = {build.target(t) for t in listed} - {None}
        return [], [t for t in build.leaves() if t not in skip], len(listed)
    _, selected = build.tests("-I", "0,0,0," + ",".join(map(str, run)))
    names = {t["name"] for t in selected}
    run = [i for i, t in enumerate(listed, 1) if t["name"] in names]
    if any(not t.get("command") for t in listed if t["name"] in names):
        return run, build.leaves(), len(listed)
    keep = {build.output(a) for t in listed if t["name"] in names for a in paths(t.get("command") or [])}
    skip = {build.target(t) for t in listed if t["name"] not in names} - keep - {None}
    targets = [t for t in build.leaves() if t not in skip]
    return run, targets + build.outputs(sorted(keep - set(targets) - {None})), len(listed)


def escape(name):
    return name.replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="backslashreplace")
    parser = argparse.ArgumentParser(description="Select the tests whose build inputs a change touches")
    sub = parser.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="write the test graph of a built tree")
    d.add_argument("build")
    d.add_argument("graph")
    s = sub.add_parser("select", help="write a ninja file and a ctest index list for a configured tree")
    s.add_argument("build")
    s.add_argument("graph")
    s.add_argument("changed", help="file with newline-separated or Git -z NUL-separated changed paths")
    e = sub.add_parser("explain", help="explain saved-graph selections without a configured build; CTest may add fixtures")
    e.add_argument("graph", type=Path)
    e.add_argument("changed", type=Path, help="file with newline-separated or Git -z NUL-separated changed paths")
    c = sub.add_parser("ctest", help="run ctest on a tree, limited to the selected tests if select wrote any")
    c.add_argument("build")
    c.add_argument("arguments", nargs=argparse.REMAINDER, help="ctest arguments")
    args = parser.parse_args(argv)
    if args.command == "ctest":
        selected = Path(args.build, "select.tests")
        extra = ["-I", selected.read_text(encoding="utf-8-sig").strip()] if selected.exists() else []
        return subprocess.run(["ctest", "--test-dir", args.build, *args.arguments, *extra]).returncode
    if args.command == "explain":
        try:
            graph = load_graph(args.graph)
            changed = changed_paths(args.changed)
            reasons = {}
            picked, full = select(graph, changed, reasons)
        except (OSError, ValueError) as error:
            parser.error(str(error))
        print(json.dumps({"tests": sorted(picked) if picked is not None else None,
                          "full_run_paths": full, "reasons": {name: sorted(set(why)) for name, why in reasons.items()}},
                         indent=2, sort_keys=True))
        return 0
    try:
        build = Build(args.build)
    except (OSError, KeyError) as error:
        parser.error(f"{args.build}: not a configured CMake build directory ({error})")
    if args.command == "dump":
        dump(build, args.graph)
        return 0
    for name in ("select.ninja", "select.tests"):
        Path(build.path, name).unlink(missing_ok=True)
    if not os.path.exists(args.graph):
        print("full run: no test graph")
        return 0
    try:
        graph = load_graph(args.graph)
        changed = changed_paths(args.changed)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    picked, full = select(graph, changed)
    if picked is None:
        print(f"full run: {', '.join(full[:5])}")
        return 0
    run, targets, total = plan(build, graph, picked)
    Path(build.path, "select.ninja").write_text("include build.ninja\nbuild selected: phony "
                                                + " ".join(escape(t) for t in targets) + "\n", encoding=build.encoding)
    Path(build.path, "select.tests").write_text("0,0,0," + ",".join(map(str, run)) + "\n", encoding="utf-8")
    print(f"selected {len(run)}/{total} tests, {len(targets)} build targets")
    return 0


if __name__ == "__main__":
    sys.exit(main())
