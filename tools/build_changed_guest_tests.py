import argparse
from collections import Counter, defaultdict
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_mingw_host_only_tests as host_only
from check_test_registration import testfile_commands
from select_tests import chunks, ctest_index_file, paths

BROAD_CHANGES = {
    "CMakeLists.txt",
    ".github/workflows/build.yml",
    "tools/build_mingw_host_only_tests.py",
    "tools/build_changed_guest_tests.py",
}
LIBRARY_LOAD_TEST = "patched_libraries_tests"
PUBLIC_HEADER_TESTS = re.compile(r"^public_headers(_compile_\d+)?$")
NID_TESTS = re.compile(r"^nid_")
PUBLIC_HEADER_INPUTS = {"core/libs/tests/test_public_headers.py", "tools/nid_names.py"}
HEADER_SUFFIXES = (".h", ".hpp", ".hh", ".inl")


def phase_report(label, started, detail=""):
    elapsed = time.perf_counter() - started
    suffix = f" ({detail})" if detail else ""
    print(f"[ci-timing] {label}: {elapsed:.1f} s{suffix}", flush=True)
    return elapsed


def report_compiler_cache():
    try:
        subprocess.run(["ccache", "--show-stats"], check=False)
    except OSError:
        pass


def ninja_log_entries(build_dir):
    try:
        with (build_dir / ".ninja_log").open("rb") as log:
            if log.readline().rstrip() in (b"# ninja log v5", b"# ninja log v6", b"# ninja log v7"):
                return Counter(log)
    except FileNotFoundError:
        return Counter()
    except OSError:
        pass
    return None


def report_libs_ninja_steps(build_dir, previous):
    if previous is None:
        return
    current = ninja_log_entries(build_dir)
    if current is None:
        return
    steps = {}
    for line in current - previous:
        match = re.fullmatch(rb"(\d{1,10})\t(\d{1,10})\t-?\d+\t([^\t\r\n]+)\t([0-9a-fA-F]+)\r?\n", line)
        if not match:
            continue
        start, end, output, command_hash = match.groups()
        elapsed = int(end) - int(start)
        if elapsed >= 0:
            steps.setdefault((start, end, command_hash), (elapsed, output))
    for elapsed, output in sorted(steps.values(), reverse=True)[:10]:
        name = output.decode("utf-8", errors="backslashreplace")
        print(f"[ci-timing] libs Ninja step: {elapsed} ms {name!a}", flush=True)


def build_libs(build_dir):
    previous = ninja_log_entries(build_dir)
    try:
        subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", "libs"], check=True)
    finally:
        report_libs_ninja_steps(build_dir, previous)


def reaches_libs(changed):
    return any(
        path == "CMakeLists.txt" or path.startswith(("core/libs/", ".github/workflows/build.yml", "tools/") + host_only.BUILD_WIDE_PREFIXES)
        for path in changed
    )


def checks_public_headers(changed):
    return any(
        path in BROAD_CHANGES or path in PUBLIC_HEADER_INPUTS or path.startswith(host_only.BUILD_WIDE_PREFIXES)
        or path in ("core/libs/CMakeLists.txt",) or path.startswith("core/libs/") and path.endswith(HEADER_SUFFIXES)
        for path in (Path(item).as_posix() for item in changed if item)
    )


def checks_nid(changed):
    return any(
        path in BROAD_CHANGES or path.startswith(host_only.BUILD_WIDE_PREFIXES)
        or path == "core/libs/CMakeLists.txt" or path.startswith("core/libs/nid/")
        for path in (Path(item).as_posix() for item in changed if item)
    )


def script_test_patterns(changed):
    return [pattern for check, pattern in ((checks_public_headers, PUBLIC_HEADER_TESTS), (checks_nid, NID_TESTS)) if check(changed)]


def ctest_tests(build_dir, *arguments):
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1", *arguments],
        capture_output=True, text=True, encoding="utf-8", check=True)
    return json.loads(result.stdout).get("tests", [])


def script_test_indices(build_dir, patterns):
    names = [test["name"] for test in ctest_tests(build_dir)]
    indices = set()
    for pattern in patterns:
        matched = {index for index, name in enumerate(names, 1) if pattern.match(name)}
        if not matched:
            raise ValueError("no ctest registration matched " + pattern.pattern)
        indices |= matched
    return sorted(indices)


def registered_test_commands(build_dir):
    commands = {}
    for entries in testfile_commands(build_dir):
        for command, arguments in entries:
            if command == "add_test" and arguments:
                commands[arguments[0]] = arguments[1:]
    return commands


def scheduled_test_owners(build_dir, selection, targets_by_name):
    tests = ctest_tests(build_dir, "-I", selection)
    registered = registered_test_commands(build_dir)
    owners = set()
    for test in tests:
        for command in (test.get("command", []), registered.get(test["name"], [])):
            owner = test_owner(command, targets_by_name)
            if owner is None:
                owner = test_owner(list(paths(command[1:])), targets_by_name)
            if owner is not None:
                owners.add(owner)
                break
    return owners


def guest_test_targets(source_root, targets, changed_paths=None):
    selected = []
    for target in targets.values():
        if target.get("type") != "EXECUTABLE":
            continue
        target_source_dir = (source_root / target["paths"]["source"]).resolve()
        try:
            target_source_dir.relative_to(source_root / "core" / "libs")
        except ValueError:
            continue
        sources = [source_root / source["path"] for source in target.get("sources", []) if not source.get("isGenerated")]
        if not any(host_only.is_core_libs_test_source(source, source_root / "core" / "libs") for source in sources):
            continue
        if not host_only.links_guest_libc(target):
            continue
        selected.append(target)
    if changed_paths is None:
        return sorted({target["name"] for target in selected}, key=str.casefold)
    changed = {Path(path).as_posix() for path in changed_paths if path}
    broad_change = any(path in BROAD_CHANGES or path.startswith(host_only.BUILD_WIDE_PREFIXES) or path.startswith("core/libs/") and (
        path.endswith((".h", ".hpp", ".hh", ".inl", ".cmake")) or Path(path).name == "CMakeLists.txt"
    ) for path in changed)
    loads_libraries = broad_change or any(path.startswith("core/libs/prx/") for path in changed)
    load_test = [LIBRARY_LOAD_TEST] if loads_libraries and any(
        target.get("name") == LIBRARY_LOAD_TEST and target.get("type") == "EXECUTABLE" for target in targets.values()) else []
    if broad_change:
        return sorted({target["name"] for target in selected} | set(load_test), key=str.casefold)
    return [
        target["name"] for target in selected
        if changed & host_only.target_reached_sources(target, targets, source_root)
    ] + load_test


def core_lib_test_targets(source_root, targets, changed_paths=None):
    guest = guest_test_targets(source_root, targets, changed_paths)
    host = host_only.host_only_test_targets(source_root, targets, changed_paths)
    return sorted(set(guest) | set(host), key=str.casefold)


def target_artifact_names(target):
    names = set()
    for artifact in target.get("artifacts", []):
        name = Path(artifact.get("path", "")).name
        if name:
            if name.casefold().endswith(".exe"):
                name = name[:-4]
            names.add(name)
    names.add(target.get("name", ""))
    names.discard("")
    return {name.casefold(): target["name"] for name in names}


def test_owner(command, targets_by_name):
    candidates = [command[0]] if command else []
    if "--" in command:
        separator = command.index("--")
        if command[1:3] == ["-E", "env"] and Path(command[0]).stem.casefold() == "cmake":
            program = next((part for part in command[3:separator] if not part.startswith("-") and "=" not in part), None)
            if program:
                candidates.append(program)
        candidates += command[separator + 1:separator + 2]
    else:
        candidates += [part for part in command[1:] if not part.startswith("-")
                       and (Path(part).suffix.casefold() == ".exe" or "/" in part or "\\" in part)]
    for part in candidates:
        name = Path(part).name.casefold()
        if name.endswith(".exe"):
            name = name[:-4]
        if name in targets_by_name:
            return targets_by_name[name]
    return None


def fixture_consumers(tests, selected):
    setups = {}
    consumers = defaultdict(list)
    for index, test in enumerate(tests, 1):
        properties = {item["name"]: item["value"] for item in test.get("properties", [])}
        setups[index] = properties.get("FIXTURES_SETUP", [])
        for fixture in properties.get("FIXTURES_REQUIRED", []):
            consumers[fixture].append(index)
    added = set()
    pending = list(selected)
    while pending:
        for fixture in setups[pending.pop()]:
            for consumer in consumers[fixture]:
                if consumer not in selected and consumer not in added:
                    added.add(consumer)
                    pending.append(consumer)
    return sorted(added)


def ctest_resolutions(build_dir, targets_by_name, selected=None):
    tests = ctest_tests(build_dir)
    resolutions = {}
    for index, test in enumerate(tests, 1):
        command = [str(part) for part in test.get("command", [])]
        owner = test_owner(command, targets_by_name)
        references = {
            targets_by_name.get(Path(part).stem.casefold()) for part in command
            if not part.startswith("-") and ("/" in part or "\\" in part or Path(part).suffix.casefold() == ".exe")
        }
        if owner is not None and (selected is None or owner in selected or references & selected):
            resolutions[test["name"]] = (owner, index)

    for index in fixture_consumers(tests, {index for _, index in resolutions.values()}):
        resolutions[tests[index - 1]["name"]] = (None, index)
    return resolutions


def run(build_dir, selected, targets_by_name, output_log=None, script_tests=None):
    total = time.perf_counter()
    started = time.perf_counter()
    try:
        build_libs(build_dir)
    finally:
        phase_report("build libs", started)
    indices = set()
    if script_tests:
        started = time.perf_counter()
        indices = set(script_test_indices(build_dir, script_tests))
        phase_report("script test discovery", started, f"{len(indices)} tests")
    selected_names = set(selected)
    if selected:
        batches = list(chunks(selected))
        started = time.perf_counter()
        try:
            for batch in batches:
                subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", *batch], check=True)
        finally:
            phase_report("build selected test targets", started, f"{len(selected)} targets in {len(batches)} batches")
        started = time.perf_counter()
        resolutions = ctest_resolutions(build_dir, targets_by_name, selected_names)
        phase_report("ctest discovery", started, f"{len(resolutions)} tests")
        missing = sorted(selected_names - {owner for owner, _ in resolutions.values()})
        if missing:
            raise ValueError("no ctest registration resolved for: " + ", ".join(missing))
        indices.update(index for _, index in resolutions.values())
    if not indices:
        print("No core/libs test sources changed; the libs build still ran")
        phase_report("total", total, "0 tests")
        return
    with ctest_index_file("0,0,0," + ",".join(map(str, sorted(indices)))) as selection:
        started = time.perf_counter()
        additional = scheduled_test_owners(build_dir, selection, targets_by_name) - selected_names
        phase_report("scheduled test owner discovery", started, f"{len(additional)} targets")
        if additional:
            batches = list(chunks(sorted(additional, key=str.casefold)))
            started = time.perf_counter()
            try:
                for batch in batches:
                    subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", *batch], check=True)
            finally:
                phase_report("build additional test owners", started, f"{len(additional)} targets in {len(batches)} batches")
        report_compiler_cache()
        started = time.perf_counter()
        try:
            subprocess.run(["ctest", "--test-dir", str(build_dir), "--output-on-failure", "--timeout", "120",
                            "--parallel", "4", "--no-tests=error", "-I", selection,
                            *(["--output-log", str(output_log)] if output_log else [])], check=True)
        finally:
            phase_report("ctest", started, f"{len(indices)} tests")
    phase_report("total", total, f"{len(selected)} targets, {len(indices)} tests")


def main(argv=None):
    parser = argparse.ArgumentParser(description="build and run changed guest and host-only core/libs tests")
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("changed_paths", nargs="?", type=Path)
    parser.add_argument("--output-log", type=Path)
    args = parser.parse_args(argv)
    try:
        changed_paths = args.changed_paths.read_text(encoding="utf-8-sig").splitlines() if args.changed_paths else None
        if changed_paths is not None:
            listed = {Path(path).as_posix() for path in changed_paths if path}
            if not reaches_libs(listed) and not script_test_patterns(listed):
                print("No changed core/libs tests require a run")
                return 0
        source_root, targets = host_only.read_reply(Path(os.path.realpath(args.build_dir)))
        selected = core_lib_test_targets(source_root, targets, changed_paths)
        if not selected:
            print("No core/libs test sources changed")
        else:
            print("Core/libs tests:")
            print("\n".join(selected))
        by_name = {target.get("name"): target for target in targets.values()}
        targets_by_name = {}
        for name, target in by_name.items():
            if target.get("type") == "EXECUTABLE":
                targets_by_name.update(target_artifact_names(target))
                targets_by_name[name.casefold()] = name
        script_tests = script_test_patterns(changed_paths) if changed_paths is not None else None
        run(args.build_dir, selected, targets_by_name, args.output_log, script_tests)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
