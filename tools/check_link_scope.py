import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
LIBS = ROOT / "core" / "libs"
MODULE = Path("cmake") / "BundledSubdirectory.cmake"
HELPER = "add_bundled_subdirectory"
PLAIN = "add_subdirectory"
BUNDLED = "3rdparty"
REQUIRED_MOUNTS = ("nghttp2", "mbedtls", "glslang")

CMAKE_TOKEN = re.compile(r"\[(?P<bracket>=*)\[.*?\](?P=bracket)\]|\"(?:\\.|[^\"\\])*\""
                         r"|(?P<comment>#\[(?P<level>=*)\[.*?\](?P=level)\]|#[^\n]*)|\\\.", re.S)
CALL = re.compile(r"\b(?P<name>add_subdirectory|add_bundled_subdirectory)\s*\(", re.IGNORECASE)
SET_VARIABLE = re.compile(r"\bset\s*\(\s*(?P<variable>[A-Za-z_][A-Za-z0-9_]*)\s+(?P<value>[^()#]*?)\s*\)",
                          re.IGNORECASE)
BUNDLED_PATH = re.compile(r"(?:^|/)" + BUNDLED + r"/(?P<name>[^/\"' ]+)", re.IGNORECASE)
HELPER_DEFINITION = re.compile(r"function\s*\(\s*" + HELPER + r"\b", re.IGNORECASE)
HELPER_SAVE = re.compile(r"get_directory_property\s*\(\s*[A-Za-z_][A-Za-z0-9_]*\s+LINK_LIBRARIES\s*\)", re.IGNORECASE)
HELPER_CLEAR = re.compile(r"set_directory_properties\s*\(\s*PROPERTIES\s+LINK_LIBRARIES\s+\"\"\s*\)", re.IGNORECASE)
HELPER_RESTORE = re.compile(r"set_directory_properties\s*\(\s*PROPERTIES\s+LINK_LIBRARIES\s+\"?\$\{[A-Za-z_]",
                            re.IGNORECASE)
HELPER_FORWARD = re.compile(r"\badd_subdirectory\s*\(\s*\$\{?ARGN", re.IGNORECASE)
INSTALL_SUPPRESSION = re.compile(r"CMAKE_SKIP_INSTALL_RULES|CMAKE_SKIP_INSTALL_ALL_DEPENDENCY", re.IGNORECASE)

CMAKE_CANDIDATES = (
    "C:/msys64/*/bin/cmake.exe",
    "C:/Program Files/CMake/bin/cmake.exe",
    "C:/Program Files (x86)/CMake/bin/cmake.exe",
    "C:/Qt/Tools/CMake_*/bin/cmake.exe",
)
TOOL_DIRS = (
    "C:/msys64/*/bin",
    "C:/Qt/Tools/Ninja",
    "/usr/bin",
    "/usr/local/bin",
    "/opt/homebrew/bin",
)
TOOL_NAMES = ("ninja", "ninja.exe", "ninja-build", "cc", "cc.exe", "gcc", "gcc.exe", "clang", "clang.exe")

MARKER = "LINKSCOPE"
EXPLICIT = "explicit_dir_entry"
FAKE = "anyps5_fake_runtime"
OPTION = "seh_personalities_nid_no_patch"
BACKEND_VARIABLE = "bundledBackendOption"
BACKEND_VALUE = "bundledValue"
INJECTIONS = ("injected_a", "injected_b", "injected_c")
SPACED = "bundled dir"
BUNDLED_DIRS = (SPACED, "second", "empty", "emptylist")
ANYS5_TARGETS = ("anyps5_before", "anyps5_shared", "anyps5_after", "anyps5_last")
EMPTY_TARGET = "anyps5_after_empty"
BUNDLED_TARGETS = ("spaced_static", "nested_static", "second_static", "empty_static", "emptylist_static")
EXPECTED_LINKS = [EXPLICIT, *INJECTIONS, FAKE]
BUNDLED_SUBPROJECTS = ((SPACED, "spaced_static", "spaced.c"), ("second", "second_static", "second.c"),
                       ("empty", "empty_static", "empty.c"), ("emptylist", "emptylist_static", "emptylist.c"))

EXPORT_ERROR = re.compile(r'includes target "[^"]+" which requires target "(?:%s)" that is not in any export set'
                          % "|".join(INJECTIONS))
GENERATE_FAILED = re.compile(r"CMake Generate step failed")
CMAKE_COMMAND_CACHE = re.compile(r"^CMAKE_COMMAND:FILEPATH=(.+)$", re.MULTILINE)

FIXTURE_ROOT = """cmake_minimum_required(VERSION 3.22)
project(link_scope_fixture C)
@MODULE@
set_directory_properties(PROPERTIES LINK_LIBRARIES "@EXPLICIT@")
add_library(@A@ OBJECT injected_a.c)
target_link_options(@A@ INTERFACE "LINKER:--undefined=@OPTION@")
add_library(@B@ OBJECT injected_b.c)
add_library(@C@ OBJECT injected_c.c)
link_libraries(@A@ @B@ @C@ @FAKE@)
get_property(scopeDirectory DIRECTORY PROPERTY LINK_LIBRARIES)
message(STATUS "@MARKER@ DIRECTORY_BEFORE ${scopeDirectory}")

add_library(anyps5_before STATIC before.c)
function(configure_bundled_backend)
    set(@BACKEND_VARIABLE@ @BACKEND_VALUE@)
    @MOUNT@(
            "${CMAKE_CURRENT_SOURCE_DIR}/@SPACED@"
            "${CMAKE_CURRENT_BINARY_DIR}/@SPACED@"
            EXCLUDE_FROM_ALL
    )
    message(STATUS "@MARKER@ BACKEND_AFTER ${@BACKEND_VARIABLE@}")
endfunction()
configure_bundled_backend()
get_property(scopeDirectory DIRECTORY PROPERTY LINK_LIBRARIES)
message(STATUS "@MARKER@ DIRECTORY_AFTER_FIRST ${scopeDirectory}")
add_library(anyps5_shared SHARED shared.c)
add_library(anyps5_after STATIC after.c)
@MOUNT@(second "${CMAKE_CURRENT_BINARY_DIR}/second" EXCLUDE_FROM_ALL)
@MOUNT@(empty "${CMAKE_CURRENT_BINARY_DIR}/empty" EXCLUDE_FROM_ALL)
add_library(anyps5_last STATIC last.c)
get_property(scopeDirectory DIRECTORY PROPERTY LINK_LIBRARIES)
message(STATUS "@MARKER@ DIRECTORY_AFTER_THIRD ${scopeDirectory}")
set_directory_properties(PROPERTIES LINK_LIBRARIES "")
@MOUNT@(emptylist "${CMAKE_CURRENT_BINARY_DIR}/emptylist" EXCLUDE_FROM_ALL)
add_library(@EMPTY@ empty_after.c)
get_property(scopeDirectory DIRECTORY PROPERTY LINK_LIBRARIES)
message(STATUS "@MARKER@ DIRECTORY_AFTER_EMPTY ${scopeDirectory}")

foreach(scopeTarget IN ITEMS anyps5_before anyps5_shared anyps5_after anyps5_last @EMPTY@ @TARGETS@)
    get_target_property(scopeLinks ${scopeTarget} LINK_LIBRARIES)
    message(STATUS "@MARKER@ ${scopeTarget} ${scopeLinks}")
endforeach()
get_target_property(scopeSource spaced_static SOURCE_DIR)
get_target_property(scopeBinary spaced_static BINARY_DIR)
message(STATUS "@MARKER@ SOURCE_DIR ${scopeSource}")
message(STATUS "@MARKER@ BINARY_DIR ${scopeBinary}")
get_property(scopeExcluded DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/@SPACED@" PROPERTY EXCLUDE_FROM_ALL)
get_property(scopeSecond DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/second" PROPERTY EXCLUDE_FROM_ALL)
get_property(scopeRoot DIRECTORY PROPERTY EXCLUDE_FROM_ALL)
message(STATUS "@MARKER@ EXCLUDED_SPACED ${scopeExcluded}")
message(STATUS "@MARKER@ EXCLUDED_SECOND ${scopeSecond}")
message(STATUS "@MARKER@ EXCLUDED_ROOT ${scopeRoot}")
"""

EXPORTING_CMAKE = """message(STATUS "@MARKER@ BACKEND_INNER ${@BACKEND_VARIABLE@}")
add_library(spaced_static STATIC spaced.c)
add_subdirectory(nested ${CMAKE_CURRENT_BINARY_DIR}/nested)
install(TARGETS spaced_static EXPORT bundledTargets ARCHIVE DESTINATION lib)
install(EXPORT bundledTargets DESTINATION lib/cmake/bundled)
"""
NESTED_CMAKE = "add_library(nested_static STATIC nested.c)\n"
PLAIN_SUBPROJECT_CMAKE = "add_library(@target@ STATIC @source@)\n"

FIXTURE_SOURCES = ("injected_a.c", "injected_b.c", "injected_c.c", "before.c", "after.c", "last.c",
                   "empty_after.c", "shared.c", f"{SPACED}/spaced.c", f"{SPACED}/nested/nested.c",
                   "second/second.c", "empty/empty.c", "emptylist/emptylist.c")


def strip_cmake_comments(text):
    parts = []
    position = 0
    for match in CMAKE_TOKEN.finditer(text):
        parts.append(text[position:match.start()])
        parts.append("\n" * match.group().count("\n") if match.group("comment") is not None else match.group())
        position = match.end()
    parts.append(text[position:])
    return "".join(parts)


def matching_parenthesis(text, opening):
    depth = 0
    index = opening
    while index < len(text):
        char = text[index]
        if char == '"':
            index += 1
            while index < len(text) and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
        elif char == "[":
            bracket = re.match(r"\[=*\[", text[index:])
            if bracket:
                level = bracket.end() - 2
                closing = text.find("]" + "=" * level + "]", index + bracket.end())
                if closing == -1:
                    return -1
                index = closing + level + 2
                continue
            index += 1
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return -1


def split_arguments(text):
    parts = []
    current = ""
    depth = 0
    index = 0
    while index < len(text):
        char = text[index]
        if char == '"':
            start = index
            index += 1
            while index < len(text) and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
            current += text[start:index + 1]
            index += 1
        elif char in "([{":
            depth += 1
            current += char
            index += 1
        elif char in ")]}":
            depth -= 1
            current += char
            index += 1
        elif char.isspace() and depth == 0:
            if current:
                parts.append(current)
                current = ""
            index += 1
        else:
            current += char
            index += 1
    if current:
        parts.append(current)
    return parts


def same_file_variables(text):
    values = {}
    for match in SET_VARIABLE.finditer(text):
        line = text.count("\n", 0, match.start()) + 1
        values.setdefault(match.group("variable").lower(), []).append((line, match.group("value")))
    return values


def resolve_argument(argument, variables, line, depth=0):
    value = argument.strip('"')
    if depth > 3 or "${" not in value:
        return value
    def replace(match):
        entries = variables.get(match.group(1).lower())
        if not entries:
            return match.group(0)
        earlier = [entry for entry in entries if entry[0] < line]
        chosen = earlier[-1][1] if earlier else entries[0][1]
        return resolve_argument(chosen, variables, line, depth + 1)
    return re.sub(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", replace, value)


def bundled_name(value):
    normalized = value.replace("\\", "/")
    match = BUNDLED_PATH.search(normalized)
    return match.group("name") if match else None


def scan_text(label, text):
    stripped = strip_cmake_comments(text)
    variables = same_file_variables(stripped)
    mounts = []
    problems = []
    for match in CALL.finditer(stripped):
        line = text.count("\n", 0, match.start()) + 1
        opening = match.end() - 1
        closing = matching_parenthesis(stripped, opening)
        if closing == -1:
            problems.append(f"{label}:{line}: unterminated {match.group('name')} call")
            continue
        arguments = split_arguments(stripped[opening + 1:closing])
        for argument in arguments:
            name = bundled_name(resolve_argument(argument, variables, line))
            if not name:
                continue
            if match.group("name").lower() == HELPER:
                mounts.append((label, line, name))
            else:
                problems.append(f"{label}:{line}: {match.group('name')} mounts 3rdparty/{name} while inherited "
                                f"link libraries are active, use {HELPER} instead")
            break
    return mounts, problems


def self_test():
    cases = (
        ("single line", f"{PLAIN}(${{CMAKE_SOURCE_DIR}}/3rdparty/nghttp2 ${{CMAKE_BINARY_DIR}}/3rdparty/nghttp2 "
                        f"EXCLUDE_FROM_ALL)\n", ["nghttp2"]),
        ("quoted multiline", f'function(configure_backend)\n    {PLAIN}(\n            '
                             f'"${{CMAKE_SOURCE_DIR}}/3rdparty/example"\n            '
                             f'"${{CMAKE_BINARY_DIR}}/3rdparty/example"\n            EXCLUDE_FROM_ALL\n    )\n'
                             f"endfunction()\n", ["example"]),
        ("quoted spaces", f'{PLAIN}("${{CMAKE_SOURCE_DIR}}/3rdparty/spaced pack" '
                          f'"${{CMAKE_BINARY_DIR}}/3rdparty/spaced pack" EXCLUDE_FROM_ALL)\n', ["spaced"]),
        ("resolved variable", f"set(OGG_DIR ${{CMAKE_SOURCE_DIR}}/3rdparty/ogg)\n"
                              f"set(OGG_BUILD ${{CMAKE_CURRENT_BINARY_DIR}}/ogg)\n"
                              f"{PLAIN}(${{OGG_DIR}} ${{OGG_BUILD}} EXCLUDE_FROM_ALL)\n", ["ogg"]),
        ("nested submodule path", f"{PLAIN}(${{CMAKE_SOURCE_DIR}}/3rdparty/zstd/build/cmake "
                                  f"${{CMAKE_BINARY_DIR}}/3rdparty/zstd EXCLUDE_FROM_ALL)\n", ["zstd"]),
        ("library directory", f"set(LIB_DIR ${{CMAKE_CURRENT_SOURCE_DIR}}/prx/${{LIB_NAME}})\n"
                              f"{PLAIN}(${{LIB_DIR}} ${{CMAKE_CURRENT_BINARY_DIR}}/prx/${{LIB_NAME}})\n", []),
        ("repository directory", f"{PLAIN}(${{CMAKE_SOURCE_DIR}}/core/shader/recompiler)\n", []),
        ("own tests", f"{PLAIN}(tests/write_watch)\n{PLAIN}(tests/guest_arena)\n", []),
        ("line comment", f"#    {PLAIN}(${{CMAKE_SOURCE_DIR}}/3rdparty/nghttp2 "
                         f"${{CMAKE_BINARY_DIR}}/3rdparty/nghttp2 EXCLUDE_FROM_ALL)\n", []),
        ("bracket comment", f"#[==[\n    {PLAIN}(${{CMAKE_SOURCE_DIR}}/3rdparty/nghttp2)\n]==]\n", []),
    )
    problems = []
    for description, text, expected in cases:
        mounts, found = scan_text(description, text)
        if len(found) != len(expected):
            problems.append(f"self test {description}: expected {len(expected)} problems, got {found}")
        for name in expected:
            if not any(name in problem for problem in found):
                problems.append(f"self test {description}: 3rdparty/{name} was not detected")
        guarded = scan_text(description, text.replace(f"{PLAIN}(", f"{HELPER}(")) [0]
        if len(guarded) != len(expected):
            problems.append(f"self test {description}: expected {len(expected)} guarded mounts, got {guarded}")
        for label, _, name in guarded:
            if name not in expected:
                problems.append(f"self test {description}: guarded mount {name} is unexpected")
    _, unterminated = scan_text("unterminated", f"{PLAIN}(${{CMAKE_SOURCE_DIR}}/3rdparty/nghttp2\n")
    if not any("unterminated" in problem for problem in unterminated):
        problems.append("self test unterminated: the incomplete call was not reported")
    return problems


def scan_repository():
    problems = self_test()
    if not MODULE.is_file():
        return [], [f"{MODULE.as_posix()} is missing"] + problems
    module = strip_cmake_comments(MODULE.read_text(encoding="utf-8"))
    for pattern, description in ((HELPER_DEFINITION, f"does not define {HELPER}"),
                                 (HELPER_SAVE, "does not read the LINK_LIBRARIES directory property"),
                                 (HELPER_CLEAR, "does not clear LINK_LIBRARIES around add_subdirectory"),
                                 (HELPER_RESTORE, "does not restore the saved LINK_LIBRARIES"),
                                 (HELPER_FORWARD, "does not forward its arguments to add_subdirectory")):
        if not pattern.search(module):
            problems.append(f"{MODULE.as_posix()} {description}")
    if INSTALL_SUPPRESSION.search(module):
        problems.append(f"{MODULE.as_posix()} suppresses install rules of another project")
    root = LIBS / "CMakeLists.txt"
    if MODULE.name not in root.read_text(encoding="utf-8"):
        problems.append(f"{relative(root)} does not include {MODULE.as_posix()}")
    mounts = []
    for path in sorted(item for item in LIBS.rglob("*")
                       if item.is_file() and (item.name == "CMakeLists.txt" or item.suffix == ".cmake")):
        file_mounts, file_problems = scan_text(relative(path), path.read_text(encoding="utf-8"))
        mounts += file_mounts
        problems += file_problems
    guarded = {name for _, _, name in mounts}
    for library in REQUIRED_MOUNTS:
        if library not in guarded:
            problems.append(f"core/libs does not mount 3rdparty/{library} through {HELPER}")
    return mounts, problems


def relative(path):
    return path.relative_to(ROOT).as_posix()


def find_cmake(explicit, build_dir):
    if explicit:
        return explicit if shutil.which(explicit) or Path(explicit).is_file() else None
    found = shutil.which("cmake")
    if found:
        return found
    cache = Path(build_dir) / "CMakeCache.txt"
    if cache.is_file():
        match = CMAKE_COMMAND_CACHE.search(cache.read_text(encoding="utf-8", errors="replace"))
        if match and Path(match.group(1)).is_file():
            return match.group(1)
    for pattern in CMAKE_CANDIDATES:
        for candidate in sorted(glob.glob(pattern)):
            if Path(candidate).is_file():
                return candidate
    return None


def tool_directories(cmake):
    directories = [str(Path(cmake).resolve().parent)]
    for tool in ("ninja", "ninja-build", "cc", "gcc", "clang", "cl"):
        found = shutil.which(tool)
        if found:
            directory = str(Path(found).resolve().parent)
            if directory not in directories:
                directories.append(directory)
    for pattern in TOOL_DIRS:
        for candidate in sorted(glob.glob(pattern)):
            directory = str(Path(candidate))
            if directory in directories:
                continue
            if any((Path(candidate) / name).exists() for name in TOOL_NAMES):
                directories.append(directory)
    return directories


def has_ninja(directories):
    if shutil.which("ninja") or shutil.which("ninja-build"):
        return True
    suffix = ".exe" if os.name == "nt" else ""
    return any((Path(directory) / f"ninja{suffix}").is_file() or (Path(directory) / f"ninja-build{suffix}").is_file()
               for directory in directories)


def write_fixture(directory, module, mount):
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)
    root = (FIXTURE_ROOT.replace("@MODULE@", f'include("{Path(module).as_posix()}")' if module else "")
            .replace("@MOUNT@", mount).replace("@EXPLICIT@", EXPLICIT).replace("@OPTION@", OPTION)
            .replace("@FAKE@", FAKE).replace("@MARKER@", MARKER).replace("@SPACED@", SPACED)
            .replace("@EMPTY@", EMPTY_TARGET).replace("@A@", INJECTIONS[0]).replace("@B@", INJECTIONS[1])
            .replace("@C@", INJECTIONS[2])
            .replace("@TARGETS@", " ".join(BUNDLED_TARGETS)))
    files = {"CMakeLists.txt": root,
             f"{SPACED}/nested/CMakeLists.txt": NESTED_CMAKE}
    for name, target, source in BUNDLED_SUBPROJECTS:
        if name == SPACED:
            content = EXPORTING_CMAKE
        else:
            content = PLAIN_SUBPROJECT_CMAKE.replace("@target@", target).replace("@source@", source)
        files[f"{name}/CMakeLists.txt"] = content
    for name in FIXTURE_SOURCES:
        files[name] = "int %s(void) { return 0; }\n" % "".join(part.title() for part in Path(name).stem.split("_"))
    for name, content in files.items():
        content = (content.replace("@MARKER@", MARKER).replace("@BACKEND_VARIABLE@", BACKEND_VARIABLE)
                   .replace("@BACKEND_VALUE@", BACKEND_VALUE))
        path = Path(directory) / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")


def configure(cmake, source, build, generator, directories, timeout, verbose):
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join(directories + [environment.get("PATH", "")])
    command = [cmake, "-S", str(source), "-B", str(build)]
    if generator:
        command += ["-G", generator]
    try:
        result = subprocess.run(command, capture_output=True, text=True, errors="replace",
                                env=environment, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, f"{source.name}: cmake timed out after {timeout} seconds"
    output = f"{result.stdout}\n{result.stderr}"
    if verbose:
        print(output)
    return result.returncode, output


def link_lines(output):
    lines = {}
    for line in output.splitlines():
        line = line.removeprefix("-- ").strip()
        if not line.startswith(f"{MARKER} "):
            continue
        parts = line.split(" ", 2)
        value = parts[2] if len(parts) == 3 else ""
        lines[parts[1]] = "" if value.endswith("-NOTFOUND") else value
    return lines


def normalized(path):
    return Path(path.replace("\\", "/")).as_posix().lower()


def all_statement(text):
    lines = iter(text.splitlines())
    for line in lines:
        if line.startswith("build all: phony"):
            statement = line
            break
    else:
        return None
    while statement.endswith("\\"):
        statement = statement[:-1].strip() + " " + next(lines, "").strip()
    return statement.split(" ", 3)[3] if len(statement.split(" ", 3)) == 4 else ""


def all_entries(statement):
    entries = []
    for token in statement.replace("$ ", "\x00").split():
        entry = token.strip('"').replace("\x00", " ")
        for pattern, replacement in (("$$", "$"), ("$:", ":"), ("$#", "#"), ("$;", ";")):
            entry = entry.replace(pattern, replacement)
        entries.append(entry)
    return entries


ARTIFACT = re.compile(r"^(?:lib)?(?P<target>.+)\.(?:a|lib|dll|so)$")


def target_blocks(text, target):
    rule = re.compile(rf":[ \t]+\w*LINKER__{re.escape(target)}_[ \t]")
    blocks = []
    current = None
    for line in text.splitlines():
        if line.startswith("build ") and rule.search(line):
            current = [line]
            blocks.append(current)
        elif current is not None and line.startswith("  "):
            current.append(line)
        else:
            current = None
    return ["\n".join(block) for block in blocks]


def statement_entries(statement):
    return [entry.replace("\\", "/") for entry in all_entries(statement)]


def built_artifacts(statement):
    names = set()
    for entry in statement_entries(statement):
        match = ARTIFACT.match(Path(entry).name.lower())
        if match:
            names.add(match.group("target"))
    return names


def built_directories(statement):
    directories = set()
    for entry in statement_entries(statement):
        directories.update(part.lower() for part in PurePosixPath(entry.lower()).parts[:-1])
    return directories


def check_build_data(build):
    problems = []
    ninja = build / "build.ninja"
    if not ninja.is_file():
        return [f"{build} has no build.ninja, the EXCLUDE_FROM_ALL and link rule checks need a Ninja generator"]
    text = ninja.read_text(encoding="utf-8", errors="replace")
    statement = all_statement(text)
    if statement is None:
        return ["the generated build data has no all target, EXCLUDE_FROM_ALL was not verified"]
    built = built_artifacts(statement)
    for name in (*ANYS5_TARGETS, EMPTY_TARGET):
        if name not in built:
            problems.append(f"{name} is not built by the all target of the generated build data: {statement}")
    excluded = built_directories(statement)
    for directory in BUNDLED_DIRS:
        if directory.lower() in excluded:
            problems.append(f"EXCLUDE_FROM_ALL was not preserved, the all target builds an artifact under "
                            f"{directory}: {statement}")
    for target in ANYS5_TARGETS:
        blocks = target_blocks(text, target)
        if not blocks:
            problems.append(f"the generated build data has no link rule for {target}")
            continue
        for injection in INJECTIONS:
            if not all(f"{injection}.dir" in block for block in blocks):
                problems.append(f"{target} lost the object file of the injected {injection}: {blocks[0]}")
    shared = target_blocks(text, "anyps5_shared")
    if shared and f"--undefined={OPTION}" not in shared[0]:
        problems.append(f"anyps5_shared lost the injected link option: {shared[0]}")
    empty = target_blocks(text, EMPTY_TARGET)
    if any(injection in block for injection in INJECTIONS for block in empty):
        problems.append(f"{EMPTY_TARGET} links injected objects although LINK_LIBRARIES was empty: {empty[0]}")
    for target in BUNDLED_TARGETS:
        blocks = target_blocks(text, target)
        if not blocks:
            problems.append(f"the generated build data has no rule for the bundled target {target}")
            continue
        for block in blocks:
            if any(injection in block for injection in INJECTIONS) or OPTION in block:
                problems.append(f"{target} inherited the injection in the generated build data: {block}")
    return problems


def check_without_helper(root, cmake, directories, generator, timeout, verbose):
    source = root / "without"
    build = root / "without-build"
    if build.exists():
        shutil.rmtree(build)
    write_fixture(source, None, PLAIN)
    result = configure(cmake, source, build, generator, directories, timeout, verbose)
    if result[0] is None:
        return [result[1]]
    code, output = result
    if code == 0:
        return ["without the helper the fixture configured successfully, the inherited link libraries no longer "
                "reach the bundled export set"]
    if not EXPORT_ERROR.search(" ".join(output.split())):
        return [f"without the helper the fixture did not fail on the export set membership of "
                f"{' or '.join(INJECTIONS)}:\n{output.strip()}"]
    if not GENERATE_FAILED.search(output):
        return [f"without the helper the export set error did not fail the CMake Generate step:\n{output.strip()}"]
    lines = link_lines(output)
    inherited = [name for name in BUNDLED_TARGETS if any(injection in lines.get(name, "") for injection in INJECTIONS)]
    if not inherited:
        return ["without the helper none of the bundled targets inherited the injection"]
    for name in ANYS5_TARGETS:
        if lines.get(name, "").split(";") != EXPECTED_LINKS:
            return [f"without the helper {name} lost its link libraries: {lines.get(name, 'missing')}"]
    return []


def check_with_helper(root, cmake, directories, generator, timeout, verbose):
    source = root / "with"
    build = root / "with-build"
    if build.exists():
        shutil.rmtree(build)
    write_fixture(source, ROOT / MODULE, HELPER)
    result = configure(cmake, source, build, generator, directories, timeout, verbose)
    if result[0] is None:
        return [result[1]]
    code, output = result
    if code != 0:
        return [f"with the helper the fixture failed:\n{output.strip()}"]
    lines = link_lines(output)
    problems = []
    for name in (*BUNDLED_TARGETS, EMPTY_TARGET):
        if lines.get(name, ""):
            problems.append(f"{name} inherited the link libraries: {lines[name]}")
    for name in (*ANYS5_TARGETS, "DIRECTORY_BEFORE", "DIRECTORY_AFTER_FIRST", "DIRECTORY_AFTER_THIRD"):
        if lines.get(name, "").split(";") != EXPECTED_LINKS:
            problems.append(f"{name} does not keep the full link libraries {EXPECTED_LINKS}: "
                            f"{lines.get(name, 'missing')}")
    if lines.get("DIRECTORY_AFTER_EMPTY", "unset"):
        problems.append(f"the empty LINK_LIBRARIES were not kept around the mount: {lines['DIRECTORY_AFTER_EMPTY']}")
    for name in ("BACKEND_AFTER", "BACKEND_INNER"):
        if lines.get(name, "missing") != BACKEND_VALUE:
            problems.append(f"the {BACKEND_VARIABLE} backend configuration did not survive the mount: "
                            f"{name}={lines.get(name, 'missing')}")
    if normalized(lines.get("SOURCE_DIR", "")) != normalized((source / SPACED).as_posix()):
        problems.append(f"the quoted source directory with a space was not preserved: {lines.get('SOURCE_DIR', '')}")
    if normalized(lines.get("BINARY_DIR", "")) != normalized((build / SPACED).as_posix()):
        problems.append(f"the binary directory of a quoted path with a space was not preserved: "
                        f"{lines.get('BINARY_DIR', '')}")
    for name in ("EXCLUDED_SPACED", "EXCLUDED_SECOND"):
        if lines.get(name, "").upper() not in ("ON", "TRUE", "1"):
            problems.append(f"EXCLUDE_FROM_ALL was not applied to the bundled mount: {name}={lines.get(name, 'missing')}")
    if lines.get("EXCLUDED_ROOT", ""):
        problems.append(f"the mount directory stayed excluded from all: {lines['EXCLUDED_ROOT']}")
    problems += check_build_data(build)
    return problems


def main():
    parser = argparse.ArgumentParser(description="keep injected link libraries out of bundled subprojects")
    parser.add_argument("--cmake", help="cmake executable to use for the fixtures")
    parser.add_argument("--generator", help="CMake generator for the fixtures, the Ninja generator is required "
                                            "for the generated build data checks")
    parser.add_argument("--build", default=str(ROOT / "build"),
                        help="configured build directory to read CMAKE_COMMAND from")
    parser.add_argument("--fixtures", help="directory for the two fixture projects, it is removed and recreated; "
                                          "defaults to the link-scope directory inside --build")
    parser.add_argument("--timeout", type=int, default=180, help="seconds each fixture may configure")
    parser.add_argument("--verbose", action="store_true", help="print the cmake output of both fixtures")
    arguments = parser.parse_args()

    mounts, problems = scan_repository()
    for problem in problems:
        print(f"link scope: {problem}")
    if problems:
        return 1
    print(f"link scope: {len(mounts)} bundled mounts under core/libs use {HELPER}: "
          + ", ".join(f"{name} at {label}:{line}" for label, line, name in sorted(mounts)))

    cmake = find_cmake(arguments.cmake, arguments.build)
    if not cmake:
        print("link scope: cmake was not found, pass --cmake or put cmake on the PATH")
        return 2
    directories = tool_directories(cmake)
    generator = arguments.generator
    if generator is None and has_ninja(directories):
        generator = "Ninja"
    if generator is None or "ninja" not in generator.lower():
        print(f"link scope: the generated build data checks need a Ninja generator and the Ninja tool "
              f"({'requested: {generator}' if arguments.generator else 'no Ninja tool found'}); "
              "install Ninja or pass --generator Ninja")
        return 2
    print(f"link scope: using {cmake} with generator {generator}")

    root = Path(arguments.fixtures) if arguments.fixtures else Path(arguments.build) / "link-scope"
    problems = check_without_helper(root, cmake, directories, generator, arguments.timeout, arguments.verbose)
    if problems:
        for problem in problems:
            print(f"link scope: {problem}")
        return 1
    print("link scope: without the helper Generate failed because the injected targets are not in the "
          "bundled export set")
    problems = check_with_helper(root, cmake, directories, generator, arguments.timeout, arguments.verbose)
    if problems:
        for problem in problems:
            print(f"link scope: {problem}")
        return 1
    print(f"link scope: with {HELPER} the bundled targets keep no injected link libraries and the AnyPS5 targets "
          f"keep {';'.join(EXPECTED_LINKS)} through every mount, including nested and excluded ones")
    return 0


if __name__ == "__main__":
    sys.exit(main())
