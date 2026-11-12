import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_mingw_host_only_tests as host_only
from select_tests import chunks

BROAD_CHANGES = {
    "CMakeLists.txt",
    ".github/workflows/build.yml",
    "tools/build_mingw_host_only_tests.py",
    "tools/build_changed_guest_tests.py",
}
LIBRARY_LOAD_TEST = "patched_libraries_tests"
PUBLIC_HEADER_TESTS = re.compile(r"^public_headers(_compile_\d+)?$")
PUBLIC_HEADER_INPUTS = {"core/libs/tests/test_public_headers.py", "tools/nid_names.py"}
HEADER_SUFFIXES = (".h", ".hpp", ".hh", ".inl")


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


def script_test_indices(build_dir, pattern):
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
        capture_output=True, text=True, check=True)
    return [index for index, test in enumerate(json.loads(result.stdout).get("tests", []), 1) if pattern.match(test["name"])]


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
        if not any("tests" in source.parts and source.is_relative_to(source_root / "core" / "libs") for source in sources):
            continue
        if not host_only.links_guest_libc(target):
            continue
        selected.append(target["name"])
    selected = sorted(set(selected), key=str.casefold)
    if changed_paths is None:
        return selected
    changed = {Path(path).as_posix() for path in changed_paths if path}
    broad_change = any(path in BROAD_CHANGES or path.startswith(host_only.BUILD_WIDE_PREFIXES) or path.startswith("core/libs/") and (
        path.endswith((".h", ".hpp", ".hh", ".inl", ".cmake")) or Path(path).name == "CMakeLists.txt"
    ) for path in changed)
    loads_libraries = broad_change or any(path.startswith("core/libs/prx/") for path in changed)
    load_test = [LIBRARY_LOAD_TEST] if loads_libraries and any(
        target.get("name") == LIBRARY_LOAD_TEST and target.get("type") == "EXECUTABLE" for target in targets.values()) else []
    if broad_change:
        return sorted(set(selected + load_test), key=str.casefold)
    changed_source = {
        relative
        for target in targets.values()
        if target.get("name") in selected
        for item in target.get("sources", []) if not item.get("isGenerated")
        for relative in [host_only.source_relative_path(source_root, source_root / item["path"])]
        if relative is not None
    }
    matched_sources = changed & changed_source
    if matched_sources:
        return [
            target["name"] for target in targets.values()
            if target.get("name") in selected
            and any(
                host_only.source_relative_path(source_root, source_root / item["path"]) in matched_sources
                for item in target.get("sources", []) if not item.get("isGenerated")
            )
        ] + load_test
    return load_test


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
        owner_index = command.index("--") + 1
        candidates += command[owner_index:owner_index + 1]
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


def ctest_resolutions(build_dir, targets_by_name, selected=None):
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
        capture_output=True, text=True, check=True)
    document = json.loads(result.stdout)
    resolutions = {}
    for index, test in enumerate(document.get("tests", []), 1):
        command = [str(part) for part in test.get("command", [])]
        owner = test_owner(command, targets_by_name)
        references = {
            targets_by_name.get(Path(part).stem.casefold()) for part in command
            if not part.startswith("-") and ("/" in part or "\\" in part or Path(part).suffix.casefold() == ".exe")
        }
        if owner is not None and (selected is None or owner in selected or references & selected):
            resolutions[test["name"]] = (owner, index)
    return resolutions


def run_ctest(build_dir, indices, output_log):
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-ctest-") as directory:
        selection = Path(directory) / "selected-tests.txt"
        selection.write_text("0,0,0," + ",".join(str(index) for index in indices) + "\n", encoding="ascii")
        subprocess.run(["ctest", "--test-dir", str(build_dir), "--output-on-failure", "--timeout", "120",
                        "--parallel", "4", "--no-tests=error", "-I", str(selection),
                        *(["--output-log", str(output_log)] if output_log else [])], check=True)


def run(build_dir, selected, targets_by_name, output_log=None, script_tests=None):
    subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", "libs"], check=True)
    scripts = script_test_indices(build_dir, script_tests) if script_tests else []
    if script_tests and not scripts:
        raise ValueError("no ctest registration matched " + script_tests.pattern)
    if selected:
        for batch in chunks(selected):
            subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", *batch], check=True)
        selected_names = set(selected)
        resolutions = ctest_resolutions(build_dir, targets_by_name, selected_names)
        additional = sorted({owner for owner, _ in resolutions.values()} - selected_names, key=str.casefold)
        for batch in chunks(additional):
            subprocess.run(["cmake", "--build", str(build_dir), "--parallel", "3", "--target", *batch], check=True)
        missing = sorted(selected_names - {owner for owner, _ in resolutions.values()})
        if missing:
            raise ValueError("no ctest registration resolved for: " + ", ".join(missing))
        run_ctest(build_dir, sorted({index for _, index in resolutions.values()} | set(scripts)), output_log)
    elif scripts:
        run_ctest(build_dir, scripts, output_log)
    else:
        print("No core/libs test sources changed; the libs build still ran")


def main(argv=None):
    parser = argparse.ArgumentParser(description="build and run changed guest and host-only core/libs tests")
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("changed_paths", nargs="?", type=Path)
    parser.add_argument("--output-log", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.changed_paths is not None:
            listed = {Path(path).as_posix() for path in args.changed_paths.read_text(encoding="utf-8").splitlines() if path}
            if not reaches_libs(listed) and not checks_public_headers(listed):
                print("No changed core/libs tests require a run")
                return 0
        source_root, targets = host_only.read_reply(Path(os.path.realpath(args.build_dir)))
        changed_paths = args.changed_paths.read_text(encoding="utf-8").splitlines() if args.changed_paths else None
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
        script_tests = PUBLIC_HEADER_TESTS if changed_paths is not None and checks_public_headers(changed_paths) else None
        run(args.build_dir, selected, targets_by_name, args.output_log, script_tests)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
