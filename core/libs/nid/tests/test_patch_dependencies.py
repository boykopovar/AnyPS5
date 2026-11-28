import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def run(command, success=True, env=None):
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, env=env)
    output = result.stdout + result.stderr
    if (result.returncode == 0) != success:
        raise AssertionError(f"unexpected exit {result.returncode}: {command}\n{output}")
    return output


def ninja(args, build, *command):
    invocation = [args.ninja, "-C", str(build)]
    if (build / f"build-{args.config}.ninja").exists():
        invocation.extend(["-f", f"build-{args.config}.ninja"])
    return invocation + list(command)


def repository_graph(args):
    for selected, unrelated in (("libSceIcu", "libScePad"), ("libScePad", "libSceIcu")):
        output = run(ninja(args, args.build, "-t", "query", f"core/libs/libs/{selected}.prx"))
        dependencies = []
        for line in output.splitlines():
            if line.strip() == "outputs:":
                break
            if line.strip().startswith("|| "):
                dependencies.append(line.strip()[3:].replace("\\", "/"))
        if not dependencies:
            raise AssertionError(f"no patch prerequisites found for {selected}\n{output}")
        unwanted = [path for path in dependencies if path.endswith(f"/libs/unpatched/{unrelated}.prx")]
        if unwanted:
            raise AssertionError(f"focused {selected} patch requires unrelated {unrelated}: {unwanted}")
        print(f"{selected}: {len(dependencies)} order-only prerequisites, no unrelated {unrelated}")


def helper_fixture(args):
    helper = Path(__file__).resolve().parents[4] / "cmake" / "PatchedRuntimeTargets.cmake"
    with tempfile.TemporaryDirectory(prefix="nid patch dependencies ") as temporary:
        root = Path(temporary)
        source = root / "source"
        build = root / "build"
        (source / "consumer").mkdir(parents=True)
        project = '''cmake_minimum_required(VERSION 3.20)
project(PatchDependencyFixture NONE)
include("@HELPER@")
option(BREAK_UNRELATED "Refuse the unrelated runtime build" ON)
foreach(name wanted unrelated)
    file(WRITE "${CMAKE_BINARY_DIR}/${name}.seed" "${name}-initial")
    set(unpatched "${CMAKE_BINARY_DIR}/${name}.unpatched")
    if(name STREQUAL "unrelated" AND BREAK_UNRELATED)
        add_custom_command(OUTPUT "${unpatched}"
            COMMAND "${CMAKE_COMMAND}" -E false
            DEPENDS "${CMAKE_BINARY_DIR}/${name}.seed")
    else()
        add_custom_command(OUTPUT "${unpatched}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/${name}.seed" "${unpatched}"
            DEPENDS "${CMAKE_BINARY_DIR}/${name}.seed")
    endif()
    add_custom_target(${name} DEPENDS "${unpatched}")
    set(output "${CMAKE_BINARY_DIR}/${name}.patched")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E copy "${unpatched}" "${output}"
        DEPENDS ${name} "${unpatched}")
    list(APPEND patched_outputs "${output}")
endforeach()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/runtime-copy.bin"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/wanted.seed" "${CMAKE_BINARY_DIR}/runtime-copy.bin"
    DEPENDS "${CMAKE_BINARY_DIR}/wanted.seed")
list(APPEND patched_outputs "${CMAKE_BINARY_DIR}/runtime-copy.bin")
add_patched_runtime_targets(libs ${patched_outputs})
add_subdirectory(consumer)
'''
        (source / "CMakeLists.txt").write_text(project.replace("@HELPER@", helper.as_posix()), encoding="utf-8")
        (source / "consumer" / "CMakeLists.txt").write_text(
            'add_custom_target(focused DEPENDS "${CMAKE_BINARY_DIR}/wanted.patched")\n', encoding="utf-8")
        configure = [args.cmake, "-S", str(source), "-B", str(build), "-G", args.generator,
                     f"-DCMAKE_MAKE_PROGRAM={args.ninja}"]
        run(configure)
        commands = run(ninja(args, build, "-t", "commands", "focused"))
        if "unrelated.patched" in commands or "-E false" in commands:
            raise AssertionError(f"focused command closure includes the unrelated runtime\n{commands}")
        run(ninja(args, build, "-j1", "focused"))
        wanted = build / "wanted.patched"
        if wanted.read_text(encoding="utf-8") != "wanted-initial":
            raise AssertionError("focused target did not copy the selected runtime")
        if (build / "unrelated.unpatched").exists() or (build / "unrelated.patched").exists():
            raise AssertionError("focused target built an unrelated runtime")
        time.sleep(1.05)
        (build / "wanted.seed").write_text("wanted-changed", encoding="utf-8")
        run(ninja(args, build, "-j1", "focused"))
        if wanted.read_text(encoding="utf-8") != "wanted-changed":
            raise AssertionError("changing the selected input did not regenerate its patched output")
        (build / "unrelated.seed").write_text("unrelated-changed", encoding="utf-8")
        before = wanted.stat().st_mtime_ns
        run(ninja(args, build, "-j1", "focused"))
        if wanted.stat().st_mtime_ns != before:
            raise AssertionError("changing an unrelated input regenerated the selected output")
        commands = run(ninja(args, build, "-t", "commands", "libs"))
        for output in ("wanted.patched", "unrelated.patched", "runtime-copy.bin"):
            if output not in commands:
                raise AssertionError(f"full aggregate no longer generates {output}\n{commands}")
        failure = run(ninja(args, build, "-j1", "libs"), success=False)
        if "-E false" not in failure:
            raise AssertionError(f"aggregate failed for a reason other than the unrelated runtime\n{failure}")
        run(configure + ["-DBREAK_UNRELATED=OFF"])
        run(ninja(args, build, "-j1", "libs"))
        for filename, expected in (("wanted.patched", "wanted-initial"),
                                   ("unrelated.patched", "unrelated-initial"),
                                   ("runtime-copy.bin", "wanted-initial")):
            if (build / filename).read_text(encoding="utf-8") != expected:
                raise AssertionError(f"full aggregate did not generate the unchanged output {filename}")
        print(f"{args.generator}: focused copy/invalidation, unrelated failure and complete aggregate passed")


ALIAS_PROJECT = '''cmake_minimum_required(VERSION 3.20)
project(PatchModuleTargetFixture NONE)
include("@HELPER@")
foreach(name wanted unrelated)
    file(WRITE "${CMAKE_BINARY_DIR}/${name}.seed" "${name}-initial")
    set(unpatched "${CMAKE_BINARY_DIR}/${name}.unpatched")
    if(name STREQUAL "unrelated")
        add_custom_command(OUTPUT "${unpatched}"
            COMMAND "${CMAKE_COMMAND}" -E false
            DEPENDS "${CMAKE_BINARY_DIR}/${name}.seed")
    else()
        add_custom_command(OUTPUT "${unpatched}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/${name}.seed" "${unpatched}"
            DEPENDS "${CMAKE_BINARY_DIR}/${name}.seed")
    endif()
    add_custom_target(${name} DEPENDS "${unpatched}")
    set(output "${CMAKE_BINARY_DIR}/patched/${name}.prx")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/patched"
        COMMAND "${CMAKE_COMMAND}" -E copy "${unpatched}" "${output}"
        DEPENDS ${name} "${unpatched}")
    list(APPEND patched_outputs "${output}")
endforeach()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/patched/runtime-copy.dll"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/patched"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_BINARY_DIR}/wanted.seed" "${CMAKE_BINARY_DIR}/patched/runtime-copy.dll"
    DEPENDS "${CMAKE_BINARY_DIR}/wanted.seed")
list(APPEND patched_outputs "${CMAKE_BINARY_DIR}/patched/runtime-copy.dll")
if(DUPLICATE_NAME)
    list(APPEND patched_outputs "${CMAKE_BINARY_DIR}/other/wanted.prx")
endif()
add_patched_runtime_targets(libs ${patched_outputs})
'''


def alias_fixture(args, generator, program):
    helper = Path(__file__).resolve().parents[4] / "cmake" / "PatchedRuntimeTargets.cmake"
    environment = dict(os.environ)
    if generator == "MinGW Makefiles":
        environment["PATH"] = os.pathsep.join(
            entry for entry in environment.get("PATH", "").split(os.pathsep)
            if entry and not (Path(entry) / "sh.exe").exists())
    with tempfile.TemporaryDirectory(prefix="nid patch module targets ") as temporary:
        root = Path(temporary)
        source = root / "source"
        build = root / "build"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(ALIAS_PROJECT.replace("@HELPER@", helper.as_posix()), encoding="utf-8")
        configure = [args.cmake, "-S", str(source), "-B", str(build), "-G", generator, f"-DCMAKE_MAKE_PROGRAM={program}"]

        def build_target(target, success=True):
            return run([args.cmake, "--build", str(build), "--target", target, "--config", args.config], success, environment)

        run(configure, env=environment)
        wanted = build / "patched" / "wanted.prx"
        build_target("libs_wanted")
        if wanted.read_text(encoding="utf-8") != "wanted-initial":
            raise AssertionError(f"{generator}: libs_wanted did not produce the selected module")
        if (build / "unrelated.unpatched").exists() or (build / "patched" / "unrelated.prx").exists():
            raise AssertionError(f"{generator}: libs_wanted built an unrelated module")
        time.sleep(1.05)
        (build / "wanted.seed").write_text("wanted-changed", encoding="utf-8")
        build_target("libs_wanted")
        if wanted.read_text(encoding="utf-8") != "wanted-changed":
            raise AssertionError(f"{generator}: changing the module's input did not regenerate it through libs_wanted")
        before = wanted.stat().st_mtime_ns
        (build / "unrelated.seed").write_text("unrelated-changed", encoding="utf-8")
        build_target("libs_wanted")
        if wanted.stat().st_mtime_ns != before:
            raise AssertionError(f"{generator}: changing an unrelated input regenerated the selected module")
        build_target("libs_runtime-copy")
        if (build / "patched" / "runtime-copy.dll").read_text(encoding="utf-8") != "wanted-changed":
            raise AssertionError(f"{generator}: libs_runtime-copy did not produce its output")
        build_target("libs_unrelated", success=False)
        duplicate = run(configure + ["-DDUPLICATE_NAME=ON"], success=False, env=environment)
        if "libs_wanted" not in duplicate:
            raise AssertionError(f"{generator}: two outputs named wanted did not stop the configuration\n{duplicate}")
        print(f"{generator}: libs_wanted builds, refreshes and isolates its module; duplicate names refused")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("graph", "helper", "aliases"), required=True)
    parser.add_argument("--make", help="make program for the Makefile generator in aliases mode")
    parser.add_argument("--build", type=Path)
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--generator", choices=("Ninja", "Ninja Multi-Config"), default="Ninja")
    parser.add_argument("--config", default="Release")
    args = parser.parse_args()
    if args.mode == "graph":
        if args.build is None:
            parser.error("--build is required for the actual repository graph")
        repository_graph(args)
    elif args.mode == "aliases":
        alias_fixture(args, "Ninja", args.ninja)
        candidates = ("mingw32-make",) if os.name == "nt" else ("gmake", "make")
        make = args.make or next(filter(None, map(shutil.which, candidates)), None)
        if make:
            alias_fixture(args, "MinGW Makefiles" if os.name == "nt" else "Unix Makefiles", make)
        else:
            print(f"no {' or '.join(candidates)} on PATH: the Makefile generator was not checked")
    else:
        helper_fixture(args)


if __name__ == "__main__":
    main()
