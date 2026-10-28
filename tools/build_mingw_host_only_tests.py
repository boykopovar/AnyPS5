import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


def object_document(value, path, name):
    if not isinstance(value, dict):
        raise ValueError(f"{path}: invalid {name} (expected an object)")
    return value


def object_entries(value, path, name):
    if not isinstance(value, list) or any(not isinstance(item, dict) for item in value):
        raise ValueError(f"{path}: invalid {name} (expected a list of objects)")
    return value


def target_document(value, path):
    target = object_document(value, path, "CMake target")
    if "name" in target and not isinstance(target["name"], str):
        raise ValueError(f"{path}: invalid CMake target name (expected a string)")
    link = object_document(target.get("link", {}), path, "CMake target link")
    object_entries(link.get("commandFragments", []), path, "CMake link command fragments")
    object_entries(target.get("artifacts", []), path, "CMake target artifacts")
    object_entries(target.get("sources", []), path, "CMake target sources")
    return target


def read_reply(build_dir):
    reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply"
    indexes = list(reply_dir.glob("index-*.json"))
    if not indexes:
        raise ValueError(f"{reply_dir}: no CMake File API index; configure with the codemodel-v2 query first")
    index = max(indexes, key=lambda path: path.stat().st_mtime_ns)
    document = object_document(json.loads(index.read_text(encoding="utf-8")), index, "CMake File API index")
    reply = object_document(document.get("reply", {}), index, "CMake File API reply")
    codemodel = reply.get("codemodel-v2")
    if codemodel is None:
        raise ValueError(f"{index}: missing codemodel-v2 reply")
    codemodel = object_document(codemodel, index, "CMake codemodel reference")
    if not codemodel:
        raise ValueError(f"{index}: missing codemodel-v2 reply")
    model_path = reply_dir / codemodel["jsonFile"]
    model = object_document(json.loads(model_path.read_text(encoding="utf-8")), model_path, "CMake codemodel")
    configurations = model.get("configurations", [])
    if len(configurations) != 1:
        raise ValueError(f"{index}: expected one CMake configuration, found {len(configurations)}")
    configuration = object_document(configurations[0], model_path, "CMake configuration")
    targets = {}
    for target in configuration.get("targets", []):
        target = object_document(target, model_path, "CMake target reference")
        target_path = reply_dir / target["jsonFile"]
        targets[target["id"]] = target_document(
            json.loads(target_path.read_text(encoding="utf-8")), target_path)
    return Path(model["paths"]["source"]).resolve(), targets


def link_command(target):
    link = target.get("link", {})
    fragments = link.get("commandFragments", [])
    return " ".join(fragment.get("fragment", "") for fragment in fragments)


def is_libc_link(name, target):
    if name == "libc":
        return True
    command = link_command(target)
    return re.search(r"(?i)(?:^|[/\\\s])libc(?:\.prx|\.dll\.a)(?:$|[\s\"'])", command) is not None


def links_guest_libc(target):
    return is_libc_link(target["name"], target)


def is_product_library(target, source_root):
    if target.get("type") not in {"STATIC_LIBRARY", "SHARED_LIBRARY", "MODULE_LIBRARY"}:
        return False
    source_dir = (source_root / target["paths"]["source"]).resolve()
    try:
        source_dir.relative_to(source_root / "core" / "libs")
    except ValueError:
        return False
    try:
        source_dir.relative_to(source_root / "core" / "libs" / "tests")
    except ValueError:
        return True
    return False


def link_command_mentions_target(command, target):
    link_names = set()
    for artifact in target.get("artifacts", []):
        name = Path(artifact.get("path", "")).name
        if name and name in command:
            return True
        for suffix in (".dll.a", ".dylib", ".prx", ".so", ".a", ".lib"):
            if name.casefold().endswith(suffix):
                name = name[:-len(suffix)]
                break
        if name.startswith("lib") and target.get("name", "").casefold() != "libc":
            name = name[3:]
        if name:
            link_names.add(name)
    target_name = target.get("name", "")
    if target_name:
        link_names.add(target_name)
    return any(
        re.search(rf"(?i)(?:^|\s)-l{re.escape(name)}(?=$|\s)", command) is not None
        for name in link_names
    )


def links_product_library(target, targets, source_root):
    command = link_command(target)
    if not command:
        return False
    for linked_target in targets.values():
        if is_product_library(linked_target, source_root) and link_command_mentions_target(command, linked_target):
            return True
    return False


def source_relative_path(source_root, source):
    try:
        return source.relative_to(source_root).as_posix()
    except ValueError:
        return None


def host_only_test_targets(source_root, targets, changed_paths=None):
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
        if links_guest_libc(target) or links_product_library(target, targets, source_root):
            continue
        selected.append(target["name"])
    selected = sorted(set(selected), key=str.casefold)
    if changed_paths is None:
        return selected
    changed = {Path(path).as_posix() for path in changed_paths if path}
    broad_change = any(
        path in {"CMakeLists.txt", ".github/workflows/build.yml", "tools/build_mingw_host_only_tests.py"}
        or path.startswith("core/libs/") and (
            path.endswith((".h", ".hpp", ".hh", ".inl")) or Path(path).name == "CMakeLists.txt"
        )
        for path in changed
    )
    if broad_change:
        return selected
    changed_source = {
        relative
        for target in targets.values()
        if target.get("name") in selected
        for item in target.get("sources", []) if not item.get("isGenerated")
        for relative in [source_relative_path(source_root, source_root / item["path"])]
        if relative is not None
    }
    matched_sources = changed & changed_source
    if matched_sources:
        return [
            target["name"] for target in targets.values()
            if target.get("name") in selected
            and any(
                source_relative_path(source_root, source_root / item["path"]) in matched_sources
                for item in target.get("sources", []) if not item.get("isGenerated")
            )
        ]
    return []


def main(argv=None):
    parser = argparse.ArgumentParser(description="build core/libs test executables that do not link guest libc")
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("changed_paths", nargs="?", type=Path)
    parser.add_argument("--list-only", action="store_true")
    args = parser.parse_args(argv)
    try:
        source_root, targets = read_reply(args.build_dir.resolve())
        changed_paths = args.changed_paths.read_text(encoding="utf-8").splitlines() if args.changed_paths else None
        selected = host_only_test_targets(source_root, targets, changed_paths)
        if not selected:
            print("No changed host-only core/libs test executables require a link check")
            return 0
        print("Host-only core/libs test executables:")
        print("\n".join(selected))
        if not args.list_only:
            subprocess.run(["cmake", "--build", str(args.build_dir), "--parallel", "3", "--target", *selected], check=True)
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"{parser.prog}: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
