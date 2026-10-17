import argparse
import collections
import json
import os
import re
import subprocess
import sys
from pathlib import Path

FULL = re.compile(r"^CMakeLists\.txt$|^core/CMakeLists\.txt$|^core/libs/CMakeLists\.txt$|^\.github/workflows/build\.yml$"
                  r"|^3rdparty/|^cmake/|^\.gitmodules$|^tools/select_tests\.py$")
LOCAL = re.compile(r"(^|/)CMakeLists\.txt$|\.cmake$")
BATCH = 20000


class Build:
    def __init__(self, path):
        cache = {}
        for line in (Path(path) / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
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

    def tests(self, *arguments):
        listing = json.loads(subprocess.run(["ctest", "--test-dir", self.path, "--show-only=json-v1", *arguments],
                                            capture_output=True, text=True, check=True).stdout)
        return listing, listing["tests"]

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


def chunks(names):
    batch, size = [], 0
    for name in names:
        if batch and size + len(name) + 1 > BATCH:
            yield batch
            batch, size = [], 0
        batch.append(name)
        size += len(name) + 1
    if batch:
        yield batch


def dump(build, out):
    deps = collections.defaultdict(set)
    current = None
    for line in build.run("-t", "deps").splitlines():
        if not line.startswith(" ") and ": #deps" in line:
            current = build.key(line.split(": #deps")[0])
        elif line.strip() and current:
            path = build.repo(line.strip())
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
        arguments = sorted({build.repo(a) for a in (test.get("command") or [])[1:] if os.path.isabs(a)} - {None})
        tests[test["name"]] = {"registered": sorted(registered - {None}), "arguments": arguments,
                               "requires": props.get("FIXTURES_REQUIRED", []), "sets": props.get("FIXTURES_SETUP", []),
                               "depends": props.get("DEPENDS", []), "target": build.target(test)}
    with open(out, "w", encoding="utf-8") as file:
        json.dump({"tests": tests, "closures": closures}, file)
    print(f"{len(tests)} tests, {len(closures)} test executables, {len(deps)} objects with header deps")


def select(graph, changed):
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
    setups = collections.defaultdict(set)
    for name, test in tests.items():
        for fixture in test["sets"]:
            setups[fixture].add(name)
    todo = list(picked)
    while todo:
        test = tests[todo.pop()]
        for extra in set(test["depends"]).union(*(setups[f] for f in test["requires"])):
            if extra in tests and extra not in picked:
                picked.add(extra)
                todo.append(extra)
    return picked, []


def plan(build, graph, picked):
    _, listed = build.tests()
    run = [i for i, t in enumerate(listed, 1) if t["name"] in picked or t["name"] not in graph["tests"]]
    _, selected = build.tests("-I", "0,0,0," + ",".join(map(str, run)))
    names = {t["name"] for t in selected}
    run = [i for i, t in enumerate(listed, 1) if t["name"] in names]
    keep = {build.output(a) for t in listed if t["name"] in names for a in t.get("command") or [] if os.path.isabs(a)}
    skip = {build.target(t) for t in listed if t["name"] not in names} - keep - {None}
    return run, [t for t in build.leaves() if t not in skip], len(listed)


def escape(name):
    return name.replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def main():
    parser = argparse.ArgumentParser(description="Select the tests whose build inputs a change touches")
    sub = parser.add_subparsers(dest="command", required=True)
    d = sub.add_parser("dump", help="write the test graph of a built tree")
    d.add_argument("build")
    d.add_argument("graph")
    s = sub.add_parser("select", help="write a ninja file and a ctest index list for a configured tree")
    s.add_argument("build")
    s.add_argument("graph")
    s.add_argument("changed", help="file with one changed path per line")
    c = sub.add_parser("ctest", help="run ctest on a tree, limited to the selected tests if select wrote any")
    c.add_argument("build")
    c.add_argument("arguments", nargs=argparse.REMAINDER, help="ctest arguments")
    args = parser.parse_args()
    if args.command == "ctest":
        selected = Path(args.build, "select.tests")
        extra = ["-I", selected.read_text(encoding="utf-8").strip()] if selected.exists() else []
        return subprocess.run(["ctest", "--test-dir", args.build, *args.arguments, *extra]).returncode
    build = Build(args.build)
    if args.command == "dump":
        dump(build, args.graph)
        return 0
    for name in ("select.ninja", "select.tests"):
        Path(build.path, name).unlink(missing_ok=True)
    if not os.path.exists(args.graph):
        print("full run: no test graph")
        return 0
    graph = json.loads(Path(args.graph).read_text(encoding="utf-8"))
    changed = [line.strip() for line in Path(args.changed).read_text(encoding="utf-8").splitlines() if line.strip()]
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
