#!/usr/bin/env python3
"""Generate ps5lift's import bridges for AnyPS5's libraries, build its runtime and run its tests.

`ps5lift.py bridges` generates the bridges a lifted program calls AnyPS5's libraries through and `runtime` builds the
runtime for a target. The other commands are the test drivers ctest runs and the bridge survey."""
import argparse
import base64
import collections
import concurrent.futures
import difflib
import fcntl
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ANYPS5 = ROOT.parent
LIBS = ANYPS5 / "core" / "libs"
FIXTURES = ROOT / "tests" / "fixtures"
SOFTFLOAT = ANYPS5 / "3rdparty" / "softfloat"
BUILD = ROOT / "build"

GUEST_FLAGS = [
    "--target=x86_64-unknown-linux-gnu",
    "-march=znver2",
    "-fno-stack-protector",
    "-fno-builtin",
    "-ffreestanding",
    "-fcf-protection=none",
    "-nostdlib",
    "-fuse-ld=lld",
    "-DPS5LIFT_GUEST",
]

GUEST_BASE = 0xFC00000000
CONSOLE_OUTPUT = {"cpu"}
SSE4A_FIXTURES = {"basic"}
PROGRAM_TIMEOUT = 300
GUEST_MODULES = {"modules": ("guestmodule", "module.c")}
HOST_LIBRARIES = {"threads": ("hostthread", "host_thread.c"), "callbacks": ("hostcallbacks", "host_callbacks.c"), "x87": ("hostx87", "host_x87.c"), "setjmp": ("hostsetjmp", "host_setjmp.c")}


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode != 0:
        sys.stderr.write(f"$ {' '.join(map(str, command))}\n{result.stdout}{result.stderr}")
        raise SystemExit(f"command failed with exit code {result.returncode}")
    return result


def host_triple():
    machine = platform.machine()
    if machine in ("aarch64", "arm64"):
        return "aarch64-unknown-linux-gnu"
    if machine in ("x86_64", "AMD64"):
        return "x86_64-unknown-linux-gnu"
    raise SystemExit(f"unsupported host {machine}")


def runner_for(triple):
    if triple.startswith(platform.machine()) or (triple.startswith("aarch64") and platform.machine() == "arm64"):
        return []
    if triple.startswith("aarch64"):
        qemu = shutil.which("qemu-aarch64") or shutil.which("qemu-aarch64-static")
        if qemu is None:
            raise SystemExit("running aarch64 code on this host needs qemu-aarch64")
        sysroot = os.environ.get("PS5LIFT_AARCH64_SYSROOT", "/usr/aarch64-linux-gnu")
        return [qemu, "-one-insn-per-tb", "-L", sysroot]
    raise SystemExit(f"cannot run {triple} on {platform.machine()}")


def host_has_sse4a():
    for line in Path("/proc/cpuinfo").read_text().splitlines():
        if line.startswith("flags"):
            return "sse4a" in line.split(":", 1)[1].split()
    return False


def lifted_cpu(runner, cpu):
    return "native" if cpu is None and not runner else cpu


def lifter_target(triple, runner, cpu):
    cpu = lifted_cpu(runner, cpu)
    flags = ["--cpu", cpu] if cpu else []
    if runner and triple.startswith("aarch64"):
        flags += ["--features", "-rcpc-immo"]
    return flags


def compare(expected, actual, label):
    if expected == actual:
        return True
    sys.stderr.write(f"{label}: output differs\n")
    sys.stderr.writelines(difflib.unified_diff(expected.splitlines(True), actual.splitlines(True), "expected", label))
    return False


DEPENDENCY_OPTIONS = {"-o", "-MF", "-MT", "-MQ"}


class AnyPS5Build:
    def __init__(self, directory):
        self.directory = Path(directory).resolve()
        commands = self.directory / "compile_commands.json"
        if not commands.exists():
            raise SystemExit(f"{commands} does not exist: configure AnyPS5 with CMAKE_EXPORT_COMPILE_COMMANDS=ON")
        self.entries = json.loads(commands.read_text())
        self.libraries_directory = self.directory / "core" / "libs" / "libs" / "unpatched"

    def library(self, name):
        path = self.libraries_directory / f"{name}.prx"
        if not path.exists():
            raise SystemExit(f"{path} does not exist: build AnyPS5's libs target")
        return path

    def linked_libraries(self, names):
        return [*(self.library(name) for name in names), f"-Wl,-rpath,{self.libraries_directory}", "-latomic", "-ldl"]

    def library_exports(self, name):
        data = self.library(name).read_bytes()
        offset, = struct.unpack_from("<Q", data, 0x28)
        size, count = struct.unpack_from("<HH", data, 0x3a)
        sections = [struct.unpack_from("<IIQQQQIIQQ", data, offset + index * size) for index in range(count)]
        for _, kind, _, _, symbols, length, link, _, _, _ in sections:
            if kind != 11:
                continue
            strings = sections[link][4]
            names = set()
            for symbol in range(symbols + 24, symbols + length, 24):
                name, info, _, section = struct.unpack_from("<IBBH", data, symbol)
                if section != 0 and info >> 4 != 0:
                    names.add(data[strings + name:data.index(b"\0", strings + name)].decode())
            return names
        raise SystemExit(f"{self.library(name)} has no dynamic symbol table")

    def library_sources(self, name):
        target = f"/CMakeFiles/{name}.dir/"
        sources = {Path(entry["file"]): entry for entry in self.entries
                   if target in "/" + entry["output"] and entry["file"].endswith(".cpp") and not is_test(Path(entry["file"]))}
        if not sources:
            raise SystemExit(f"AnyPS5's build has no target {name}")
        return sorted(sources.items())

    def source(self, path):
        entries = [entry for entry in self.entries if Path(entry["file"]) == Path(path)]
        if len(entries) != 1:
            raise SystemExit(f"AnyPS5's build compiles {path} {len(entries)} times, expected once")
        return entries[0]


def is_test(path):
    return path.is_relative_to(ANYPS5) and "tests" in path.relative_to(ANYPS5).parts


def compiler_arguments(entry):
    words = shlex.split(entry["command"])[1:]
    arguments = []
    skip = False
    for word in words:
        if skip:
            skip = False
        elif word in DEPENDENCY_OPTIONS:
            skip = True
        elif word not in ("-c", "-MD", "-MMD", entry["file"]):
            arguments.append(word)
    return arguments


RUNTIME_SOURCES = ["runtime/src/Runtime.cpp", "runtime/src/GuestCpu.cpp", "runtime/src/X87.cpp", "runtime/src/Jump.cpp", "runtime/src/Cpuid.cpp", "runtime/src/StringCompare.cpp", "runtime/src/NativeCall.cpp"]
MAIN_SOURCE = "runtime/src/Main.cpp"
SOFTFLOAT_FLAGS = [
    "-DSOFTFLOAT_FAST_INT64", "-DSOFTFLOAT_ROUND_ODD", "-DINLINE_LEVEL=5", "-DSOFTFLOAT_FAST_DIV32TO16", "-DSOFTFLOAT_FAST_DIV64TO32",
    "-DTHREAD_LOCAL=_Thread_local", f"-I{SOFTFLOAT / 'build' / 'Linux-x86_64-GCC'}", f"-I{SOFTFLOAT / 'source' / '8086-SSE'}", f"-I{SOFTFLOAT / 'source' / 'include'}",
]

DEFAULT_CPUS = {"aarch64-unknown-linux-gnu": "neoverse-v2"}


def softfloat_sources():
    return [ANYPS5 / line.strip() for line in (ROOT / "runtime" / "softfloat_sources.txt").read_text().splitlines() if line.strip()]


def runtime_directory(build_root, triple, cpu):
    return Path(build_root) / (f"runtime-{triple}" if cpu is None else f"runtime-{triple}-{cpu}")


def fingerprint(*parts):
    return hashlib.sha256("\0".join(map(str, parts)).encode()).hexdigest()


def dependencies(depfile):
    body = depfile.read_text().replace("\\\n", " ").split(":", 1)[1]
    return [Path(token.replace("\\ ", " ")) for token in re.split(r"(?<!\\)\s+", body.strip()) if token]


def needs_build(record, output, depfile):
    if not output.exists() or not depfile.exists() or not output.with_suffix(".cmd").exists():
        return True
    if output.with_suffix(".cmd").read_text() != record:
        return True
    built = output.stat().st_mtime
    return any(not path.exists() or path.stat().st_mtime > built for path in dependencies(depfile))


def compile_one(command, source, output, record):
    output.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([*command, "-MMD", "-MF", str(output.with_suffix(".d")), "-c", str(source), "-o", str(output)], capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(f"compiling {source} failed:\n{result.stderr}")
    output.with_suffix(".cmd").write_text(record)


def compiler_version(compiler):
    return subprocess.run([compiler, "--version"], capture_output=True, text=True, check=True).stdout


def cpu_flags(triple, cpu):
    cpu = DEFAULT_CPUS.get(triple) if cpu is None else cpu
    if cpu is None or cpu == "generic":
        return []
    return [f"-march={cpu}"] if triple.startswith("x86_64") else [f"-mcpu={cpu}"]


def build_runtime(triple, cpu, output_directory, clang, clangxx, jobs):
    output_directory = Path(output_directory)
    library = output_directory / "libps5lift_runtime.so"
    main_library = output_directory / "libps5lift_main.a"
    target = [f"--target={triple}", "-O2", "-fPIC", *cpu_flags(triple, cpu)]
    units = []
    for path in softfloat_sources():
        units.append(([clang, *target, *SOFTFLOAT_FLAGS], path, output_directory / "softfloat" / (path.stem + ".o")))
    for relative in [*RUNTIME_SOURCES, MAIN_SOURCE]:
        path = ROOT / relative
        units.append(([clangxx, *target, "-std=c++20", f"-I{ROOT / 'runtime' / 'include'}", f"-I{SOFTFLOAT / 'source' / 'include'}", "-DTHREAD_LOCAL=thread_local", "-DSOFTFLOAT_FAST_INT64", "-DLITTLEENDIAN=1"], path, output_directory / "runtime" / (path.stem + ".o")))
    versions = {compiler: compiler_version(compiler) for compiler in (clang, clangxx)}
    units = [(command, source, output, fingerprint(versions[command[0]], *command, source)) for command, source, output in units]
    shared = [clangxx, f"--target={triple}", "-shared", "-fuse-ld=lld", f"-Wl,-soname,{library.name}", "-pthread", "-ldl"]
    archiver = subprocess.run([clang, "--print-prog-name=llvm-ar"], capture_output=True, text=True, check=True).stdout.strip()
    outputs = ((library, units[:-1], lambda temporary, objects: [*shared, *objects, "-o", temporary]),
               (main_library, units[-1:], lambda temporary, objects: [archiver, "crs", temporary, *objects]))
    output_directory.mkdir(parents=True, exist_ok=True)
    with open(output_directory / ".lock", "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        stale = [unit for unit in units if needs_build(unit[3], unit[2], unit[2].with_suffix(".d"))]
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            list(pool.map(lambda unit: compile_one(*unit), stale))
        for output, members, command in outputs:
            objects = [str(unit[2]) for unit in members]
            record = fingerprint(*command("", objects))
            listing = output.with_name(output.name + ".cmd")
            if output.exists() and listing.exists() and listing.read_text() == record and max(unit[2].stat().st_mtime for unit in members) < output.stat().st_mtime:
                continue
            temporary = output.with_name(output.name + ".tmp")
            temporary.unlink(missing_ok=True)
            subprocess.run(command(str(temporary), objects), check=True)
            temporary.replace(output)
            listing.write_text(record)
    return library, main_library


def runtime_command(args):
    for path in build_runtime(args.triple, args.cpu, args.output, args.clang, args.clangxx, args.jobs):
        print(path)
    return 0


def runtime_relink_command(args):
    output = Path(args.output)
    library, _ = build_runtime(args.triple, None, output, args.clang, args.clangxx, args.jobs)
    member = output / "runtime" / "Runtime.o"
    linked_before = member.stat().st_mtime_ns - 1_000_000_000
    os.utime(library, ns=(linked_before, linked_before))
    build_runtime(args.triple, None, output, args.clang, args.clangxx, args.jobs)
    relinked = library.stat().st_mtime_ns > member.stat().st_mtime_ns
    print(f"{'PASS' if relinked else 'FAIL'} {library.name}, older than its member {member.name} as a failed or interrupted link leaves it, "
          f"{'is' if relinked else 'is not'} linked again")
    return 0 if relinked else 1


SVE_REGISTER = re.compile(r"\b[zp]\d+\b")
SVE_SCALAR = re.compile(r"rdvl|addvl|addpl|cnt[bhwd]|(?:inc|dec)[bhwd]|[su]q(?:inc|dec)[bhwd]|setffr|rdffr|wrffr")


def sve_instructions(listing):
    found = []
    for line in listing.splitlines():
        fields = line.split("\t")
        if len(fields) < 2 or not fields[0].strip().endswith(":"):
            continue
        operands = re.split(r" <|//", fields[2])[0] if len(fields) > 2 else ""
        if SVE_SCALAR.fullmatch(fields[1].strip()) or SVE_REGISTER.search(operands):
            found.append(line.strip())
    return found


def runtime_sve_command(args):
    if not args.triple.startswith("aarch64"):
        raise SystemExit(f"SVE is an aarch64 extension, so runtime-sve checks aarch64 runtimes only, not {args.triple}")
    library, _ = build_runtime(args.triple, args.cpu, args.output, args.clang, args.clangxx, args.jobs)
    objdump = subprocess.run([args.clang, "--print-prog-name=llvm-objdump"], capture_output=True, text=True, check=True).stdout.strip()
    found = sve_instructions(run([objdump, "-d", "--no-show-raw-insn", library]).stdout)
    for line in found[:20]:
        print(line)
    print(f"{'FAIL' if found else 'PASS'} {library.name} built for {args.cpu} has {len(found)} SVE instructions")
    return 1 if found else 0


def link_arguments(library, main_library=None):
    return [*([str(main_library)] if main_library else []), str(library), f"-Wl,-rpath,{library.parent}"]


def runtime_link_arguments(ps5lift, triple, cpu, clang, clangxx):
    return link_arguments(*build_runtime(triple, cpu, runtime_directory(Path(ps5lift).resolve().parent.parent, triple, cpu), clang, clangxx, os.cpu_count()))


NID_SUFFIX = bytes([0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1, 0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30])
POSTFIX = "_nid_postfix"
UNPATCHED = "_nid_no_patch"
VARIADIC_SIBLINGS = {"libc_printf": "vprintf"}
GUEST_STATE_EXPORTS = {"setjmp", "longjmp", "sigsetjmp", "siglongjmp", "_setjmp", "_longjmp"}
EXPORT_PATTERN = re.compile(r'APS5_EXPORT\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)')
GUEST_LONG_DOUBLE = "LibcDetail::GuestLongDouble"
BRIDGE_LONG_DOUBLE = "Ps5Lift::Bridge::LongDouble"
DISAMBIGUATION = re.compile(r"_nid_disambig\d+$")

cindex = None


def bindings(libclang=None):
    global cindex, K, INTEGER_KINDS, FLOAT_KINDS, POINTER_KINDS, FUNCTION_KINDS, Kind
    if cindex is not None:
        return cindex
    try:
        import clang.cindex as module
    except ImportError:
        raise SystemExit("the bridges need libclang's Python bindings (python3-clang-18 on Ubuntu)")
    if libclang is not None:
        module.Config.set_library_file(libclang)
    cindex = module
    K = cindex.TypeKind
    INTEGER_KINDS = {
        K.BOOL, K.CHAR_U, K.UCHAR, K.CHAR16, K.CHAR32, K.USHORT, K.UINT, K.ULONG, K.ULONGLONG,
        K.CHAR_S, K.SCHAR, K.WCHAR, K.SHORT, K.INT, K.LONG, K.LONGLONG,
    }
    FLOAT_KINDS = {K.FLOAT, K.DOUBLE}
    POINTER_KINDS = {K.POINTER, K.LVALUEREFERENCE, K.RVALUEREFERENCE}
    FUNCTION_KINDS = {K.FUNCTIONPROTO, K.FUNCTIONNOPROTO}
    Kind = collections.namedtuple("Kind", "translation_unit linkage_spec namespace unexposed_decl function_decl var_decl base_specifier union_decl")(
        *(getattr(cindex.CursorKind, name).value for name in ("TRANSLATION_UNIT", "LINKAGE_SPEC", "NAMESPACE", "UNEXPOSED_DECL", "FUNCTION_DECL", "VAR_DECL",
                                                              "CXX_BASE_SPECIFIER", "UNION_DECL")))
    return cindex


def kind_of(cursor):
    return cursor._kind_id


def compute_nid(name):
    digest = hashlib.sha1(name.encode() + NID_SUFFIX).digest()
    reversed_bytes = digest[:8][::-1]
    encoded = base64.b64encode(reversed_bytes).decode().rstrip("=")
    return encoded.translate(str.maketrans("+/", "+-"))[:11]


def bridge_symbol(name):
    symbol = "ps5lift_import_"
    for character in name:
        if character == "_":
            symbol += "__"
        elif character == "+":
            symbol += "_p"
        elif character == "-":
            symbol += "_m"
        elif character == "#":
            symbol += "_h"
        elif character.isascii() and character.isalnum():
            symbol += character
        else:
            raise ValueError(f"import name {name} has a character a bridge symbol cannot carry")
    return symbol


class Unsupported(Exception):
    pass


Record = collections.namedtuple("Record", "name definitions eightbytes size")


def scalar(canonical):
    if canonical.kind in INTEGER_KINDS or canonical.kind in (K.INT128, K.UINT128):
        return canonical.spelling.replace("const ", "").replace("volatile ", ""), "integer"
    if canonical.kind in FLOAT_KINDS:
        return canonical.spelling.replace("const ", "").replace("volatile ", ""), "sse"
    if canonical.kind == K.ENUM:
        return canonical.get_declaration().enum_type.get_canonical().spelling, "integer"
    if canonical.kind in POINTER_KINDS:
        if canonical.get_pointee().get_canonical().kind in FUNCTION_KINDS:
            raise Unsupported("a record holding a function pointer")
        return "void*", "integer"
    raise Unsupported(f"a record holding a {canonical.kind.spelling} field")


def fields(canonical):
    for field in canonical.get_fields():
        if field.is_bitfield():
            raise Unsupported("a record with bit-fields")
        yield field, field.type.get_canonical(), field.get_field_offsetof() // 8


def leaves(canonical, offset):
    if canonical.kind == K.RECORD:
        for _, field_type, field_offset in fields(canonical):
            yield from leaves(field_type, offset + field_offset)
    elif canonical.kind == K.CONSTANTARRAY:
        element = canonical.element_type.get_canonical()
        for index in range(canonical.element_count):
            yield from leaves(element, offset + index * element.get_size())
    else:
        yield offset, canonical.get_size(), canonical.get_align(), scalar(canonical)[1]


def eightbytes(canonical):
    size = canonical.get_size()
    if size > 16:
        return "memory"
    classes = [None] * ((size + 7) // 8)
    for offset, length, alignment, kind in leaves(canonical, 0):
        if offset % alignment:
            return "memory"
        for eightbyte in range(offset // 8, (offset + length - 1) // 8 + 1):
            if classes[eightbyte] != "integer":
                classes[eightbyte] = kind
    if None in classes:
        raise Unsupported("a record with an eightbyte of padding only")
    return tuple(classes)


def declarator(canonical, definitions):
    if canonical.kind == K.RECORD:
        nested = record(canonical)
        definitions.extend(definition for definition in nested.definitions if definition not in definitions)
        return nested.name, ""
    if canonical.kind == K.CONSTANTARRAY:
        if canonical.element_count == 0:
            raise Unsupported("a record with a zero-length array")
        spelling, extent = declarator(canonical.element_type.get_canonical(), definitions)
        return spelling, f"[{canonical.element_count}]{extent}"
    return scalar(canonical)[0], ""


def record(type_):
    canonical = type_.get_canonical()
    declaration = canonical.get_declaration()
    size, alignment = canonical.get_size(), canonical.get_align()
    if size < 0 or alignment < 0:
        raise Unsupported("a record of unknown layout")
    if not canonical.is_pod():
        raise Unsupported("a record that is not plain old data")
    if any(kind_of(child) == Kind.base_specifier for child in declaration.get_children()):
        raise Unsupported("a record with a base class")
    union = kind_of(declaration) == Kind.union_decl
    definitions = []
    members = []
    offsets = []
    position = 0
    for index, (_, field_type, offset) in enumerate(fields(canonical)):
        spelling, extent = declarator(field_type, definitions)
        field_alignment = field_type.get_align()
        if field_alignment > alignment or offset % field_alignment or (not union and offset < position):
            raise Unsupported("a packed record")
        if not union and offset != (position + field_alignment - 1) // field_alignment * field_alignment:
            members.append(f"unsigned char padding{index}[{offset - position}];")
        members.append(f"{spelling} field{index}{extent};")
        offsets.append((f"field{index}", offset))
        position = max(position, offset + field_type.get_size())
    if not members:
        raise Unsupported("an empty record")
    if (position + alignment - 1) // alignment * alignment != size:
        members.append(f"unsigned char padding[{size if union else size - position}];")
    keyword = "union" if union else "struct"
    body = " ".join(members)
    name = "Ps5LiftRecord" + hashlib.sha1(f"{keyword} {alignment} {body}".encode()).hexdigest()[:12]
    checks = [f"sizeof({name}) == {size}", f"alignof({name}) == {alignment}"] + [f"offsetof({name}, {field}) == {offset}" for field, offset in offsets]
    definitions.append("\n".join([f"{keyword} alignas({alignment}) {name} {{ {body} }};"] + [f"static_assert({check});" for check in checks]))
    return Record(name, tuple(definitions), eightbytes(canonical), size)


def classify(type_, role):
    canonical = type_.get_canonical()
    kind = canonical.kind
    if kind == K.VOID and role.endswith("return"):
        return ("void", "void")
    if kind in INTEGER_KINDS:
        return ("integer", canonical.spelling.replace("const ", "").replace("volatile ", ""))
    if kind == K.ENUM:
        underlying = canonical.get_declaration().enum_type.get_canonical()
        return ("integer", underlying.spelling)
    if kind in FLOAT_KINDS:
        return ("float", canonical.spelling.replace("const ", ""))
    if kind in POINTER_KINDS:
        pointee = canonical.get_pointee().get_canonical()
        if pointee.kind in FUNCTION_KINDS:
            if role == "parameter":
                return ("callback", callback_signature(pointee))
            if role == "return":
                return ("function", callback_signature(pointee))
            raise Unsupported(f"function pointer as a {role}")
        return ("pointer", "void*")
    if kind == K.LONGDOUBLE:
        return ("extended", "long double")
    if kind == K.RECORD and canonical.spelling == GUEST_LONG_DOUBLE:
        return ("extended", BRIDGE_LONG_DOUBLE)
    if kind in (K.INCOMPLETEARRAY, K.CONSTANTARRAY) and role == "parameter":
        return ("pointer", "void*")
    if kind == K.RECORD:
        try:
            return ("record", record(canonical))
        except Unsupported as reason:
            raise Unsupported(f"{role} passed by value as {reason}")
    if kind in (K.INT128, K.UINT128):
        return ("integer128", "unsigned __int128" if kind == K.UINT128 else "__int128")
    raise Unsupported(f"{role} of kind {kind.spelling}")


def callback_signature(function):
    if function.kind == K.FUNCTIONPROTO and function.is_function_variadic():
        raise Unsupported("variadic callback")
    result_class, result = classify(function.get_result(), "callback return")
    if result_class not in ("void", "integer", "pointer", "float"):
        raise Unsupported(f"callback returning {result_class}")
    arguments = []
    for argument in function.argument_types():
        argument_class, spelling = classify(argument, "callback argument")
        if argument_class not in ("integer", "pointer", "float"):
            raise Unsupported(f"callback taking {argument_class}")
        arguments.append(spelling)
    return f"{result}({', '.join(arguments)})"


def describe(cursor, symbol):
    variadic = cursor.type.kind == K.FUNCTIONPROTO and cursor.type.is_function_variadic()
    return_class = classify(cursor.result_type, "return")
    records = list(return_class[1].definitions) if return_class[0] == "record" else []
    parameters = []
    integers = 1 if return_class[0] == "record" and return_class[1].eightbytes == "memory" else 0
    floats = 0
    stack = 0
    for argument in cursor.get_arguments():
        parameter_class, spelling = classify(argument.type, "parameter")
        if parameter_class == "record":
            argument_record = spelling
            spelling = argument_record.name
            records.extend(definition for definition in argument_record.definitions if definition not in records)
            classes = argument_record.eightbytes
            in_registers = classes != "memory" and integers + classes.count("integer") <= 6 and floats + classes.count("sse") <= 8
            if in_registers:
                sources = []
                for kind in classes:
                    if kind == "integer":
                        sources.append(f"Integer<std::uint64_t>(state, {integers})")
                        integers += 1
                    else:
                        sources.append(f"Floating<std::uint64_t>(state, {floats})")
                        floats += 1
                access = f"Record<{spelling}>({', '.join(sources)})"
            else:
                stack += stack % 2 if argument.type.get_canonical().get_align() > 8 else 0
                access = f"StackRecord<{spelling}>(state, {stack})"
                stack += (argument_record.size + 7) // 8
        elif parameter_class == "extended":
            stack += stack % 2
            access = f"HostExtended(state, {stack})" if spelling == "long double" else f"Extended<{spelling}>(state, {stack})"
            stack += 2
        elif parameter_class == "integer128":
            if integers + 2 <= 6:
                access = f"Integer128<{spelling}>(state, {integers})"
                integers += 2
            else:
                stack += stack % 2
                access = f"Stack128<{spelling}>(state, {stack})"
                stack += 2
        elif parameter_class == "float":
            access = f"Floating<{spelling}>(state, {floats})" if floats < 8 else f"Stack<{spelling}>(state, {stack})"
            if floats < 8:
                floats += 1
            else:
                stack += 1
        elif parameter_class == "callback":
            source = f"Integer<std::uint64_t>(state, {integers})" if integers < 6 else f"Stack<std::uint64_t>(state, {stack})"
            access = f"reinterpret_cast<void*>(Callback<{spelling}>({source}))"
            spelling = "void*"
            if integers < 6:
                integers += 1
            else:
                stack += 1
        else:
            access = f"Integer<{spelling}>(state, {integers})" if integers < 6 else f"Stack<{spelling}>(state, {stack})"
            if integers < 6:
                integers += 1
            else:
                stack += 1
        parameters.append((spelling, access))
    if return_class[0] == "record":
        return_class = ("record", (return_class[1].name, return_class[1].eightbytes))
    return {"symbol": symbol, "return": return_class, "parameters": parameters, "records": records, "variadic": (integers, floats, stack) if variadic else None}


def export_base(name):
    if name.endswith(POSTFIX):
        name = name[: -len(POSTFIX)]
    return DISAMBIGUATION.sub("", name)


class Definitions:
    def __init__(self):
        self.texts = {}

    def __call__(self, cursor):
        location = cursor.location.file
        if location is None or not str(location).endswith(".cpp"):
            return False
        text = self.texts.setdefault(str(location), Path(str(location)).read_bytes())
        if kind_of(cursor) == Kind.var_decl:
            declaration = text[cursor.extent.start.offset:cursor.extent.end.offset]
            return not declaration.lstrip().startswith(b"extern") or b"=" in declaration
        position = cursor.extent.end.offset
        while position < len(text) and text[position:position + 1] not in (b"{", b";", b"}"):
            position += 1
        return text[position:position + 1] == b"{"


def scan_source(job):
    bindings()
    source, arguments, explicit = job
    unit = cindex.Index.create().parse(str(source), args=arguments, options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
    errors = [f"{diagnostic.location.file}:{diagnostic.location.line}: {diagnostic.spelling}" for diagnostic in unit.diagnostics if diagnostic.severity >= cindex.Diagnostic.Error]
    defined = Definitions()
    candidates = []
    stack = [unit.cursor]
    while stack:
        cursor = stack.pop()
        kind = kind_of(cursor)
        if kind in (Kind.translation_unit, Kind.linkage_spec, Kind.namespace, Kind.unexposed_decl):
            stack.extend(cursor.get_children())
            continue
        if kind not in (Kind.function_decl, Kind.var_decl) or cursor.linkage != cindex.LinkageKind.EXTERNAL:
            continue
        name = cursor.spelling
        c_linkage = cursor.mangled_name in (name, "_" + name)
        unpatched = name.endswith(UNPATCHED)
        if name.endswith(POSTFIX) or name in explicit:
            marked = True
        elif not name.startswith("SDL_") and c_linkage and defined(cursor):
            marked = False
        else:
            continue
        candidate = {"name": name, "marked": marked, "unpatched": unpatched, "c_linkage": c_linkage, "data": kind == Kind.var_decl}
        if c_linkage and not candidate["data"]:
            try:
                candidate["export"] = describe(cursor, name)
            except Unsupported as reason:
                candidate["reason"] = str(reason)
        candidates.append(candidate)
    return errors, candidates


def collect(sources, exported, pool):
    explicit = {}
    for source, _ in sources:
        for nid, function in EXPORT_PATTERN.findall(source.read_text(errors="replace")):
            explicit[function] = nid

    exports = {}
    data = {}
    unsupported = {}
    parse_errors = []
    candidates = []
    for errors, found in pool.map(scan_source, [(source, compiler_arguments(entry), explicit) for source, entry in sources]):
        parse_errors.extend(errors)
        candidates.extend(candidate for candidate in found if not candidate["c_linkage"] or candidate["name"] in exported)
    named = {candidate["name"] for candidate in candidates}
    for candidate in candidates:
        name = candidate["name"]
        if name in explicit:
            nid = explicit[name]
        elif candidate["unpatched"]:
            nid = name
        elif candidate["marked"]:
            nid = compute_nid(export_base(name))
        elif name + POSTFIX in named:
            continue
        else:
            nid = compute_nid(export_base(name))
        if not candidate["c_linkage"]:
            unsupported.setdefault(nid, (name, "C++ linkage"))
            continue
        if candidate["data"]:
            data.setdefault(nid, name)
            continue
        if nid in exports or nid in unsupported:
            continue
        if export_base(name) in GUEST_STATE_EXPORTS:
            unsupported[nid] = (name, "saves or restores the guest's own registers")
            continue
        if "export" in candidate:
            exports[nid] = dict(candidate["export"], name=name)
        else:
            unsupported[nid] = (name, candidate["reason"])
    by_name = {export["name"]: export for export in exports.values()}
    for nid in [nid for nid, export in exports.items() if export["variadic"]]:
        export = exports.pop(nid)
        base = export["name"][: -len(POSTFIX)] if export["name"].endswith(POSTFIX) else export["name"]
        sibling = by_name.get(VARIADIC_SIBLINGS.get(base, "v" + base) + POSTFIX)
        named = [spelling for spelling, _ in export["parameters"]]
        if sibling is not None and not sibling["variadic"] and [spelling for spelling, _ in sibling["parameters"]] == named + ["void*"]:
            export["target"] = sibling["symbol"]
        elif export["variadic"][0] < 6:
            export["forward"] = True
        else:
            unsupported[nid] = (export["name"], "variadic without a va_list sibling or a free integer register")
            continue
        exports[nid] = export
    return exports, data, unsupported, parse_errors


def data_symbol(nid):
    return "ps5lift_data_" + bridge_symbol(nid)[len("ps5lift_import_"):]


def emit(exports, data):
    lines = [f"#include <ps5lift/Bridge.hpp>", "", "#include <cstddef>", "#include <cstdint>", "#include <type_traits>", "", "using namespace Ps5Lift::Bridge;", ""]
    records = []
    for nid in sorted(exports):
        records.extend(definition for definition in exports[nid]["records"] if definition not in records)
    lines.extend(records + ([""] if records else []))
    for nid in sorted(exports):
        export = exports[nid]
        if export["variadic"] and not export.get("forward"):
            continue
        return_class, return_spelling = export["return"]
        if return_class == "function":
            return_spelling = f"std::add_pointer_t<{return_spelling}>"
        elif return_class == "record":
            return_spelling = return_spelling[0]
        declaration = ", ".join([spelling for spelling, _ in export["parameters"]] + (["..."] if export.get("forward") else []))
        lines.append(f'extern "C" {return_spelling} {export["symbol"]}({declaration});')
    for nid in sorted(data):
        lines.append(f'extern "C" char {data[nid]};')
        lines.append(f'extern "C" void* const {data_symbol(nid)} = &{data[nid]};')
    lines.append("")
    for nid in sorted(exports):
        export = exports[nid]
        return_class, return_spelling = export["return"]
        accesses = [access for _, access in export["parameters"]]
        if export.get("forward"):
            accesses += [f"Integer<std::uint64_t>(state, {index})" for index in range(export["variadic"][0], 6)]
        elif export["variadic"]:
            accesses.append("variadic.List()")
        call = f'{export.get("target", export["symbol"])}({", ".join(accesses)})'
        lines.append(f'extern "C" void {bridge_symbol(nid)}(Ps5LiftState* state) {{')
        lines.append("    try {")
        if export["variadic"] and not export.get("forward"):
            integers, floats, stack = export["variadic"]
            lines.append(f"        VariadicArguments variadic(state, {integers}, {floats}, {stack});")
        if return_spelling == "long double" or any(access.startswith("HostExtended(") for _, access in export["parameters"]):
            lines.append("        const ExtendedScope scope(state);")
        if return_class == "void":
            lines.append(f"        {call};")
        elif return_class == "extended":
            lines.append(f"        ReturnExtended(state, {call});")
        elif return_class == "function":
            lines.append(f"        Return(state, GuestFunction<{return_spelling}>({call}));")
        elif return_class == "record" and return_spelling[1] == "memory":
            lines.append("        const auto result = Integer<std::uint64_t>(state, 0);")
            lines.append(f"        ReturnIndirect(state, result, {call});")
        elif return_class == "record":
            classes = ", ".join("Eightbyte::Integer" if kind == "integer" else "Eightbyte::Sse" for kind in return_spelling[1])
            lines.append(f"        ReturnRecord<{classes}>(state, {call});")
        else:
            lines.append(f"        Return(state, {call});")
        lines.append("    } catch (const GuestUnwind&) {")
        lines.append("    }")
        lines.append("}")
        lines.append("")
    return "\n".join(lines)


def generate_bridges(libraries, anyps5_build, output, libclang=None, jobs=None):
    bindings(libclang)
    exports, data, unsupported, parse_errors = {}, {}, {}, []
    build = AnyPS5Build(anyps5_build)
    with concurrent.futures.ProcessPoolExecutor(jobs or os.cpu_count(), initializer=bindings, initargs=(libclang,)) as pool:
        for name in libraries:
            library_exports, library_data, library_unsupported, library_errors = collect(build.library_sources(name), build.library_exports(name), pool)
            for nid, export in library_exports.items():
                exports.setdefault(nid, export)
            for nid, symbol in library_data.items():
                data.setdefault(nid, symbol)
            for nid, entry in library_unsupported.items():
                unsupported.setdefault(nid, entry)
            parse_errors.extend(library_errors)
    if parse_errors:
        sys.stderr.writelines(f"{error}\n" for error in parse_errors)
        raise SystemExit(f"{len(parse_errors)} parse errors: libclang's recovered declarations would give wrong prototypes, so no bridges were written")
    for nid in list(unsupported):
        if nid in exports or nid in data:
            del unsupported[nid]
    Path(output).write_text(emit(exports, data))
    return exports, data, unsupported


def bridges_command(args):
    if args.libclang is not None and not args.libclang.is_file():
        raise SystemExit(f"--libclang: {args.libclang} does not exist")
    exports, data, unsupported = generate_bridges(args.library, args.anyps5_build, args.output, str(args.libclang) if args.libclang else None, args.jobs)
    reasons = collections.Counter(re.sub(r"\d+-byte", "N-byte", reason) for _, reason in unsupported.values())
    print(f"{' '.join(args.library)}: {len(exports)} bridges, {len(data)} data exports, {len(unsupported)} exports without one")
    for reason, count in reasons.most_common():
        print(f"  {count:5} {reason}")
    if args.report:
        for nid, (name, reason) in sorted(unsupported.items(), key=lambda item: item[1]):
            print(f"  {nid} {name}: {reason}")
    return 0


def lift(ps5lift, guest, lifted, triple, flags):
    return run([ps5lift, guest, "-o", lifted, "--triple", triple, *flags]).stderr.strip()


def link_lifted(clangxx, triple, program, inputs, runtime):
    run([clangxx, f"--target={triple}", "-std=c++20", "-O1", f"-I{ROOT / 'runtime' / 'include'}", "-fuse-ld=lld", *inputs, *runtime, "-pthread", "-o", program])


def run_lifted(runner, program, label, timeout=PROGRAM_TIMEOUT, cwd=None):
    try:
        return subprocess.run([*runner, program], capture_output=True, text=True, timeout=timeout, cwd=cwd)
    except subprocess.TimeoutExpired:
        sys.stderr.write(f"{label}: still running after {timeout} s\n")
        return None


def build_host_library(clang, work, cross=()):
    library = work / "libhost.so"
    if not library.exists():
        run([clang, *GUEST_FLAGS, *cross, "-O2", "-fPIC", "-shared", FIXTURES / "host_stub.c", "-o", library])


def build_guest(clang, work, name, optimization, defines=(), vectorize=False, suffix="", sysroot=None):
    cross = [f"--sysroot={sysroot}"] if sysroot else []
    build_host_library(clang, work, cross)
    extra = []
    if name in HOST_LIBRARIES:
        library_name, source = HOST_LIBRARIES[name]
        extra_library = work / f"lib{library_name}.so"
        if not extra_library.exists():
            run([clang, "--target=x86_64-unknown-linux-gnu", *cross, "-O2", "-fPIC", "-shared", "-fuse-ld=lld", FIXTURES / source, "-pthread", "-o", extra_library])
        extra = [f"-l{library_name}"]
    if name in GUEST_MODULES:
        module_name, source = GUEST_MODULES[name]
        module = work / f"lib{module_name}.so"
        if not module.exists():
            run([clang, *GUEST_FLAGS, *cross, "-O2", "-fPIC", "-shared", "-Wl,-z,pack-relative-relocs", f"-Wl,-soname,$ORIGIN/lib{module_name}.so", FIXTURES / source, f"-L{work}", "-lhost", "-o", module])
        extra = [f"-l{module_name}", "-lc", "-Wl,--allow-shlib-undefined"]
    guest = work / f"{name}{optimization}{suffix}"
    vector_flags = [] if vectorize else ["-fno-vectorize", "-fno-slp-vectorize"]
    run([clang, optimization, *GUEST_FLAGS, *cross, *vector_flags, *defines, "-fpie", "-no-pie", f"-Wl,--image-base={GUEST_BASE:#x}",
         "-Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2", FIXTURES / f"{name}.c", f"-L{work}", "-lhost", *extra, "-o", guest])
    return guest


def fixture_command(args):
    optimizations = args.optimization or ["-O0", "-O1", "-O2", "-O3"]
    memories = args.memory or ["tso", "relaxed"]

    def expected_for(optimization):
        specific = FIXTURES / f"{args.fixture}{optimization}.expected"
        return (specific if specific.exists() else FIXTURES / f"{args.fixture}.expected").read_text()

    if args.lift_only:
        runner = None
        target = ["--cpu", args.cpu] if args.cpu else []
    else:
        runner = runner_for(args.triple)
        target = lifter_target(args.triple, runner, args.cpu)
    work = Path(tempfile.mkdtemp(prefix="ps5lift-"))
    failures = 0
    try:
        for optimization in optimizations:
            expected = expected_for(optimization)
            guest = build_guest(args.clang, work, args.fixture, optimization, ["-DPS5LIFT_EMULATED"] if runner else [], sysroot=args.guest_sysroot)
            if not args.lift_only and platform.machine() == "x86_64" and args.fixture not in CONSOLE_OUTPUT and (args.fixture not in SSE4A_FIXTURES or host_has_sse4a()):
                native = run([guest], env={**os.environ, "LD_LIBRARY_PATH": str(work)}).stdout
                failures += not compare(expected, native, f"native {optimization}")
            for memory in memories:
                label = f"{args.triple} {optimization} {memory}{' unoptimized lift' if args.unoptimized_lift else ''}"
                lifted = work / f"{guest.name}-{memory}.a"
                report = lift(args.ps5lift, guest, lifted, args.triple, ["--memory", memory, *target, *(["-O0"] if args.unoptimized_lift else [])])
                if args.lift_only:
                    print(f"PASS {label}, lifted only: {report}")
                    continue
                program = work / f"{guest.name}-{memory}"
                link_lifted(args.clangxx, args.triple, program, [FIXTURES / "host_bridge.cpp", lifted], runtime_link_arguments(args.ps5lift, args.triple, lifted_cpu(runner, args.cpu), args.clang, args.clangxx))
                result = run_lifted(runner, program, label)
                ok = result is not None and result.returncode == 0 and compare(expected, result.stdout, label)
                if result is not None and result.returncode != 0:
                    sys.stderr.write(f"{label}: exit code {result.returncode}\n{result.stderr}")
                print(f"{'PASS' if ok else 'FAIL'} {label}: {report}")
                failures += not ok
    finally:
        if args.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work)
    return 1 if failures else 0


LIBC_IMPORTS = ["printf", "snprintf", "vsnprintf", "sscanf", "puts", "strtold", "strtod", "modfl", "fabsl", "fprintf", "div", "ldiv", "lldiv", "imaxdiv", "feclearexcept", "fetestexcept",
                "acosl", "lrint", "remainderl"]
LIBC_DATA_IMPORTS = ["_Stdout"]
KERNEL_IMPORTS = ["printf", "exit", "scePthreadCreate", "scePthreadJoin", "scePthreadMutexInit", "scePthreadMutexLock", "scePthreadMutexUnlock",
                  "scePthreadMutexDestroy", "sceKernelUsleep", "sceKernelGetProcessTime", "sceKernelReadTsc", "sceKernelGetTscFrequency", "signal", "raise",
                  "sigprocmask", "sigsetjmp", "siglongjmp", "scePthreadExit",
                  "scePthreadAttrInit", "scePthreadAttrDestroy", "scePthreadAttrSetstacksize", "scePthreadAttrGet", "scePthreadAttrGetstack", "scePthreadSelf",
                  "sceKernelInstallExceptionHandler", "sceKernelRemoveExceptionHandler", "sceKernelRaiseException",
                  "sceKernelLoadStartModule", "sceKernelDlsym", "sceKernelGetModuleInfoFromAddr", "sceKernelGetModuleInfoForUnwind",
                  "scePthreadAttrSetcreatesuspend", "scePthreadResume", "sceKernelMprotect"]
FIBER_IMPORTS = ["printf", "fflush", "exit", "_sceFiberInitializeImpl", "sceFiberFinalize", "sceFiberRun", "sceFiberSwitch", "sceFiberReturnToThread", "sceFiberGetSelf",
                 "scePthreadCreate", "scePthreadJoin"]
LOADER_IMPORTS = ["printf", "exit", "dlopen", "sceKernelGetExecutableModuleHandle", "sceKernelLoadStartModule", "sceKernelDlsym"]
LOG_LINE = re.compile(r"^\[[^\]\s]+:\d+ [^\]]+\] .*\n", re.MULTILINE)
GUEST_LINK = ["-no-pie", f"-Wl,--image-base={GUEST_BASE:#x}", "-Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2"]
LIBRARY_PROGRAM_TIMEOUT = 600


def build_libc_guest(args, work, optimization):
    build_host_library(args.clang, work)
    guest = build_stubbed_guest(args, work, optimization, "libc", LIBC_IMPORTS, LIBC_DATA_IMPORTS, ["host"])[0]
    return guest, False, [FIXTURES / "host_bridge.cpp"], []


def build_exceptions_guest(args, work, optimization):
    guest = work / f"exceptions{optimization}"
    run([args.clangxx, optimization, *GUEST_FLAGS, "-fno-vectorize", "-fno-slp-vectorize", "-fexceptions", "-fpie", *GUEST_LINK,
         FIXTURES / "exceptions.cpp", "-lstdc++", "-lc", "-lgcc_s", "-o", guest])
    return guest, True, [], ["--import-nids"]


def build_stubbed_guest(args, work, optimization, fixture, imports, data_imports=(), libraries=()):
    (work / f"{fixture}_nids.h").write_text("".join(f'#define NID_{name} "{compute_nid(name)}"\n' for name in [*imports, *data_imports]))
    stub = work / f"{fixture}_stub.c"
    stub.write_text("".join(f'void stub_{index}(void) __asm__("{compute_nid(name)}");\nvoid stub_{index}(void) {{}}\n' for index, name in enumerate(imports))
                    + "".join(f'char data_{index}[8] __asm__("{compute_nid(name)}");\n' for index, name in enumerate(data_imports)))
    library = work / f"lib{fixture}stub.so"
    if not library.exists():
        run([args.clang, *GUEST_FLAGS, "-O2", "-fPIC", "-shared", stub, "-o", library])
    guest = work / f"{fixture}{optimization}"
    run([args.clang, optimization, *GUEST_FLAGS, "-fno-vectorize", "-fno-slp-vectorize", f"-I{work}", "-fpie", *GUEST_LINK,
         FIXTURES / f"{fixture}.c", f"-L{work}", f"-l{fixture}stub", *(f"-l{library}" for library in libraries), "-o", guest])
    return guest, False, [], []


def build_kernel_guest(args, work, optimization):
    module = work / "libkernelmodule.so"
    if not module.exists():
        run([args.clang, *GUEST_FLAGS, "-O2", "-fPIC", "-shared", "-Wl,-soname,$ORIGIN/libkernelmodule.so", FIXTURES / "kernel_module.c", "-o", module])
    return build_stubbed_guest(args, work, optimization, "kernel", KERNEL_IMPORTS, libraries=["kernelmodule"])


def build_fiber_guest(args, work, optimization):
    return build_stubbed_guest(args, work, optimization, "fiber", FIBER_IMPORTS)


def build_loader_guest(args, work, optimization):
    module = work / "app0" / "sce_module" / "libSceLoaderFixture.prx.guest.prx"
    system = work / "libs" / "libSceLoaderFixture.prx"
    relative = module.relative_to(work).as_posix()
    if not module.exists():
        module.parent.mkdir(parents=True)
        run([args.clang, *GUEST_FLAGS, "-O2", "-fPIC", "-shared", f"-Wl,-soname,$ORIGIN/{relative}", FIXTURES / "loader_module.c", "-o", module])
    if not system.exists():
        system.parent.mkdir()
        run([args.clang, "-O2", "-fPIC", "-shared", FIXTURES / "loader_system.c", "-o", system])
    return build_stubbed_guest(args, work, optimization, "loader", LOADER_IMPORTS, libraries=[f":{relative}"])


LIBRARY_GUESTS = {"libc": build_libc_guest, "exceptions": build_exceptions_guest, "kernel": build_kernel_guest, "fiber": build_fiber_guest, "loader": build_loader_guest}
GUEST_LIBRARIES = {"libc": ["libc"], "exceptions": ["libc"], "kernel": ["libc", "libkernel", "libkernel_sys"], "fiber": ["libc", "libkernel", "libSceFiber"], "loader": ["libc", "libkernel"]}


def libc_fixture_command(args):
    anyps5 = AnyPS5Build(args.anyps5_build)
    triple = host_triple()
    runner = runner_for(triple)
    cpu = lifted_cpu(runner, None)
    expected = (FIXTURES / f"{args.fixture}.expected").read_text()
    stop_file = FIXTURES / f"{args.fixture}.stop"
    stop = stop_file.read_text().strip() if stop_file.exists() else None
    work = Path(tempfile.mkdtemp(prefix=f"ps5lift-{args.fixture}-"))
    failures = 0
    try:
        if args.bridges:
            bridges = args.bridges.resolve()
        else:
            bridges = work / "bridges.cpp"
            generate_bridges(GUEST_LIBRARIES[args.fixture], args.anyps5_build, bridges)
        linked = [bridges, *anyps5.linked_libraries(GUEST_LIBRARIES[args.fixture])]
        runtime = runtime_link_arguments(args.ps5lift, triple, cpu, args.clang, args.clangxx)
        for optimization in args.optimization or ["-O0", "-O2"]:
            guest, native, host_sources, lift_flags = LIBRARY_GUESTS[args.fixture](args, work, optimization)
            if native and platform.machine() == "x86_64":
                failures += not compare(expected, run([guest], env={**os.environ, "LD_LIBRARY_PATH": str(work)}).stdout, f"native {optimization}")
            label = f"{triple} {optimization}"
            lifted = work / f"{guest.name}.a"
            report = lift(args.ps5lift, guest, lifted, triple, [*lift_flags, *lifter_target(triple, runner, cpu)])
            program = work / f"{guest.name}-lifted"
            link_lifted(args.clangxx, triple, program, [lifted, *host_sources, *linked], runtime)
            result = run_lifted(runner, program, label, LIBRARY_PROGRAM_TIMEOUT, work)
            ended = result is not None and (result.returncode == 0 if stop is None else result.returncode != 0 and stop in result.stderr)
            ok = ended and compare(expected, LOG_LINE.sub("", result.stdout), label)
            if result is not None and not ended:
                expectation = "exit code 0" if stop is None else f"a stop with {stop!r}"
                sys.stderr.write(f"{label}: exit code {result.returncode}, expected {expectation}\n{result.stderr}")
            print(f"{'PASS' if ok else 'FAIL'} {args.fixture} {label}: {report}")
            failures += not ok
    finally:
        if args.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work)
    return 1 if failures else 0


PS5_GENERATORS = ANYPS5 / "core" / "relinker" / "relinker" / "tests" / "macos"
PS5_EXPECTED = {
    "import": ("hello from a PS5 ELF on macOS\n", 0),
    "tls": ("", 0x1288 & 0xff),
}


def ps5_fixture_command(args):
    runner = runner_for(args.triple)
    cpu = lifted_cpu(runner, None)
    expected_output, expected_code = PS5_EXPECTED[args.fixture]
    work = Path(tempfile.mkdtemp(prefix="ps5lift-ps5-"))
    failures = 0
    try:
        source = work / f"{args.fixture}.elf"
        run([sys.executable, "-I", PS5_GENERATORS / f"{args.fixture}_fixture.py", source])
        relinked = work / f"{args.fixture}.out"
        run([args.relinker, "--skip-sce-module", "--linux-fixed-base", f"{GUEST_BASE:#x}", source, relinked])
        for memory in ("tso", "relaxed"):
            lifted = work / f"{args.fixture}-{memory}.a"
            report = lift(args.ps5lift, relinked, lifted, args.triple, ["--memory", memory, *lifter_target(args.triple, runner, cpu)])
            program = work / f"{args.fixture}-{memory}"
            link_lifted(args.clangxx, args.triple, program, [FIXTURES / "ps5_bridge.cpp", lifted], runtime_link_arguments(args.ps5lift, args.triple, cpu, args.clang, args.clangxx))
            result = run_lifted(runner, program, f"{args.fixture} {memory}")
            ok = result is not None and result.stdout == expected_output and result.returncode == expected_code
            if result is not None and not ok:
                sys.stderr.write(f"{memory}: exit code {result.returncode} (expected {expected_code}), output {result.stdout!r} (expected {expected_output!r})\n{result.stderr}")
            print(f"{'PASS' if ok else 'FAIL'} {args.fixture} {args.triple} {memory}: {report}")
            failures += not ok
    finally:
        shutil.rmtree(work)
    return 1 if failures else 0


LIBC_TESTS = {
    "Formatting": ["libc"], "GuestStrings": ["libc", "libScePosixForWebKit"], "GuestWideFormatting": ["libc"], "GuestStdioVariadic": ["libc"],
    "GuestAllocatingStrings": ["libkernel", "libc"], "GuestSyslog": ["libkernel", "libc"], "GuestWideStreamScan": ["libc"], "HostExceptionPointer": ["libc"],
    "GuestHttp2": ["libSceHttp2", "libkernel", "libc"], "GuestNpManager": ["libSceNpManager", "libc"], "GuestHttp2Transport": ["libSceHttp2", "libSceHttp", "libSceNet", "libSceSsl", "libkernel", "libc"],
    "GuestSocketOptions": ["libkernel", "libc"], "GuestResolver": ["libScePosixForWebKit", "libSceNet", "libc"], "ResolverRaw": ["libScePosixForWebKit", "libc"],
}
LIBC_TEST_RUNS = {"GuestHttp2Transport": [[], ["--early"]]}
LIBC_TEST_ARCHIVES = {"GuestHttp2Transport": ["3rdparty/nghttp2/lib/libnghttp2.a", "3rdparty/mbedtls/library/libmbedtls.a", "3rdparty/mbedtls/library/libmbedx509.a",
                                              "3rdparty/mbedtls/library/libmbedcrypto.a"]}
EMPTY_PROGRAM = ANYPS5 / "core" / "libs" / "tests" / "LiftedEmptyProgram.cpp"


def up_to_date(output):
    depfile = output.with_suffix(".d")
    if not output.exists() or not depfile.exists():
        return False
    built = output.stat().st_mtime
    return all(path.exists() and path.stat().st_mtime <= built for path in dependencies(depfile))


def compile_all(clangxx, units):
    def one(unit):
        source, arguments, output = unit
        if up_to_date(output):
            return None
        output.parent.mkdir(parents=True, exist_ok=True)
        command = [clangxx, *arguments, "-MMD", "-MF", str(output.with_suffix(".d")), "-c", str(source), "-o", str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        return None if result.returncode == 0 else f"{source}:\n{result.stderr}"
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count()) as pool:
        errors = [error for error in pool.map(one, units) if error]
    if errors:
        raise SystemExit("\n".join(errors))


def libc_tests_command(args):
    anyps5 = AnyPS5Build(args.anyps5_build)
    triple = host_triple()
    work = Path(args.build) / f"libc-tests-{triple.split('-')[0]}"
    tests = args.test or list(LIBC_TESTS)
    empty_program = work / "empty_program.o"
    units = [(EMPTY_PROGRAM, ["-std=c++20", "-O1", "-fPIC", f"-I{ROOT / 'runtime' / 'include'}"], empty_program)]
    for test in tests:
        source = LIBS / "tests" / f"{test}.cpp"
        units.append((source, compiler_arguments(anyps5.source(source)), work / f"{test}.o"))
    compile_all(args.clangxx, units)
    runtime, _ = build_runtime(triple, None, runtime_directory(args.build, triple, None), args.clang, args.clangxx, os.cpu_count())

    failures = 0
    for test in tests:
        program = work / test
        archives = [anyps5.directory / archive for archive in LIBC_TEST_ARCHIVES.get(test, [])]
        link = [args.clangxx, "-fuse-ld=lld", work / f"{test}.o", empty_program, *anyps5.linked_libraries(LIBC_TESTS[test]), *archives, *link_arguments(runtime), "-pthread", "-o", program]
        linked = subprocess.run(link, capture_output=True, text=True)
        if linked.returncode != 0:
            print(f"FAIL {test}: link\n{linked.stderr[-2000:]}")
            failures += 1
            continue
        for arguments in LIBC_TEST_RUNS.get(test, [[]]):
            result = subprocess.run([str(program), *arguments], capture_output=True, text=True, cwd=work, timeout=600)
            ok = result.returncode == 0
            print(f"{'PASS' if ok else 'FAIL'} {' '.join([test, *arguments])}")
            if not ok:
                sys.stdout.write(f"exit code {result.returncode}\n{result.stdout[-2000:]}{result.stderr[-2000:]}")
            failures += not ok
    return 1 if failures else 0


RECORD_SOURCE = FIXTURES / "records.cpp"
RECORD_FLAGS = ["-x", "c++", "-std=c++20", "--target=x86_64-unknown-linux-gnu"]
RECORD_HOST_TARGETS = ["x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu"]
RECORD_REFUSALS = {
    "Packed": "a packed record",
    "Bits": "a record with bit-fields",
    "HoldsFunction": "a record holding a function pointer",
    "PaddedEightbyte": "a record with an eightbyte of padding only",
}
LOWERED_DECLARATION = re.compile(r"^declare (?P<result>.+?) @(?P<name>(?:take|give)_\w+)\((?P<parameters>.*)\)", re.MULTILINE)


def ir_classes(text):
    if "byval(" in text or "sret(" in text:
        return "memory"
    classes = []
    for part in re.split(r",\s*(?![^<]*>)", text.strip().strip("{}").strip()):
        match = re.search(r"<2 x float>|\bi(\d+)\b|\b(ptr)\b|\bfloat\b|\bdouble\b", part)
        if match is None:
            raise ValueError(f"unexpected lowered type {part!r} in {text!r}")
        if match.group(1):
            classes += ["integer"] * ((int(match.group(1)) + 63) // 64)
        else:
            classes.append("integer" if match.group(2) else "sse")
    return tuple(classes)


def lowered_records(clang):
    ir = subprocess.run([clang, *RECORD_FLAGS, "-O0", "-S", "-emit-llvm", "-o", "-", RECORD_SOURCE], check=True, capture_output=True, text=True).stdout
    result = {}
    for match in LOWERED_DECLARATION.finditer(ir):
        kind, name = match.group("name").split("_", 1)
        if kind == "take":
            result[("parameter", name)] = ir_classes(match.group("parameters"))
        elif match.group("result").strip() == "void":
            result[("return", name)] = ir_classes(match.group("parameters"))
        else:
            result[("return", name)] = ir_classes(match.group("result"))
    return result


def classified_records():
    bindings()
    unit = cindex.Index.create().parse(str(RECORD_SOURCE), args=RECORD_FLAGS)
    errors = [diagnostic.spelling for diagnostic in unit.diagnostics if diagnostic.severity >= cindex.Diagnostic.Error]
    if errors:
        raise SystemExit(f"{RECORD_SOURCE}: {errors}")
    result = {}
    stack = list(unit.cursor.get_children())
    while stack:
        cursor = stack.pop()
        kind = kind_of(cursor)
        if kind in (Kind.linkage_spec, Kind.unexposed_decl):
            stack.extend(cursor.get_children())
            continue
        if kind != Kind.function_decl or not cursor.spelling.startswith(("take_", "give_")):
            continue
        direction, name = cursor.spelling.split("_", 1)
        role = "parameter" if direction == "take" else "return"
        type_ = next(cursor.get_arguments()).type if direction == "take" else cursor.result_type
        try:
            result[(role, name)] = record(type_)
        except Unsupported as reason:
            result[(role, name)] = str(reason)
    return result


def record_bridges_command(args):
    expected = lowered_records(args.clang)
    actual = classified_records()
    failures = [f"{name} as a {role}: lowered by clang but not classified" for role, name in sorted(set(expected) - set(actual))]
    definitions = []
    for key in sorted(actual):
        role, name = key
        result = actual[key]
        if name in RECORD_REFUSALS:
            if result != RECORD_REFUSALS[name]:
                failures.append(f"{name} as a {role}: expected refusal {RECORD_REFUSALS[name]!r}, got {result!r}")
            continue
        if isinstance(result, str):
            failures.append(f"{name} as a {role}: refused ({result})")
            continue
        if key not in expected:
            failures.append(f"{name} as a {role}: clang lowered no declaration")
        elif result.eightbytes != expected[key]:
            failures.append(f"{name} as a {role}: classified {result.eightbytes}, clang passes {expected[key]}")
        definitions.extend(definition for definition in result.definitions if definition not in definitions)
    with tempfile.TemporaryDirectory() as work:
        layouts = Path(work) / "layouts.cpp"
        layouts.write_text("#include <cstddef>\n\n" + "\n".join(definitions) + "\n")
        for triple in RECORD_HOST_TARGETS:
            compiled = subprocess.run([args.clang, "-x", "c++", "-std=c++20", f"--target={triple}", "-fsyntax-only", layouts], capture_output=True, text=True)
            if compiled.returncode != 0:
                failures.append(f"generated layouts do not compile for {triple}:\n{compiled.stderr}")
    for failure in failures:
        print(f"FAIL {failure}")
    checked = sum(1 for name in {name for _, name in actual} if name not in RECORD_REFUSALS)
    if not checked:
        failures.append("no record was checked")
        print("FAIL no record was checked")
    print(f"{'FAIL' if failures else 'PASS'} records: {checked} shapes as parameters and returns, {len(RECORD_REFUSALS)} refused")
    return 1 if failures else 0


def parse_bench(output):
    results = {}
    times = {}
    for line in output.splitlines():
        kind, name, value = line.split()
        (results if kind == "result" else times)[name] = int(value)
    return results, times


def bench_command(args):
    runner = runner_for(args.triple)
    work = Path(tempfile.mkdtemp(prefix="ps5lift-bench-"))
    try:
        scale = [f"-DBENCH_SCALE={args.scale}"]
        guests = {"": build_guest(args.clang, work, "bench", "-O2", scale), "-vec": build_guest(args.clang, work, "bench", "-O2", scale, vectorize=True, suffix="-vec")}
        programs = {}
        for vectorize in (True, False):
            name = "native" if vectorize else "native-novec"
            flags = [] if vectorize else ["-fno-vectorize", "-fno-slp-vectorize"]
            cpu_flag = [] if runner else cpu_flags(args.triple, "native")
            programs[name] = work / name
            run([args.clang, f"--target={args.triple}", "-O2", *cpu_flag, *flags, *scale, FIXTURES / "bench.c", FIXTURES / "host_native.c", "-fuse-ld=lld", "-o", programs[name]])
        for memory, suffix in (("relaxed", ""), ("tso", ""), ("tso", "-vec")):
            name = f"lifted-{memory}{suffix}"
            lifted = work / f"{name}.a"
            report = lift(args.ps5lift, guests[suffix], lifted, args.triple, ["--memory", memory, *lifter_target(args.triple, runner, args.cpu)])
            print(f"{name}: {report}")
            programs[name] = work / name
            link_lifted(args.clangxx, args.triple, programs[name], [FIXTURES / "host_bridge.cpp", lifted], runtime_link_arguments(args.ps5lift, args.triple, lifted_cpu(runner, args.cpu), args.clang, args.clangxx))

        reference = None
        best = {}
        for name, program in programs.items():
            for _ in range(args.repeat):
                results, times = parse_bench(run([*runner, program]).stdout)
                if reference is None:
                    reference = results
                elif results != reference:
                    raise SystemExit(f"{name} computed different results: {results} != {reference}")
                for kernel, value in times.items():
                    best[(name, kernel)] = min(best.get((name, kernel), value), value)

        if runner:
            print(f"note: {args.triple} runs under {Path(runner[0]).name}; the times measure the emulator, not the hardware")
        print(f"host {platform.machine()} {platform.node()}, triple {args.triple}, scale {args.scale}, best of {args.repeat}")
        columns = list(programs)
        print(f"{'kernel':8}" + "".join(f"{name:>16}" for name in columns) + f"{'tso/relaxed':>14}" + f"{'tso/native':>12}" + f"{'vec/native':>12}")
        for kernel in reference:
            row = f"{kernel:8}" + "".join(f"{best[(name, kernel)] / 1e6:14.1f}ms" for name in columns)
            row += f"{best[('lifted-tso', kernel)] / best[('lifted-relaxed', kernel)]:13.2f}x"
            row += f"{best[('lifted-tso', kernel)] / best[('native-novec', kernel)]:11.2f}x"
            row += f"{best[('lifted-tso-vec', kernel)] / best[('native', kernel)]:11.2f}x"
            print(row)
    finally:
        shutil.rmtree(work)
    return 0


def survey_command(args):
    libraries = args.library or sorted(path.name for path in (LIBS / "prx").iterdir() if path.is_dir())
    results = {}
    with tempfile.TemporaryDirectory() as work:
        for library in libraries:
            try:
                exports, data, unsupported = generate_bridges([library], args.anyps5_build, Path(work) / "bridges.cpp")
                entry = {"bridges": len(exports), "data": len(data), "unbridged": len(unsupported),
                         "reasons": [f"{count:5} {reason}" for reason, count in collections.Counter(re.sub(r"\d+-byte", "N-byte", reason) for _, reason in unsupported.values()).most_common()]}
                print(f"{library}: {entry['bridges']} bridges, {entry['data']} data, {entry['unbridged']} unbridged", flush=True)
            except SystemExit as error:
                entry = {"error": str(error)}
                print(f"{library}: {entry['error']}", flush=True)
            results[library] = entry
    if args.json:
        Path(args.json).write_text(json.dumps(results, indent=1, sort_keys=True))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    bridges = commands.add_parser("bridges", help="generate import bridges for AnyPS5 libraries", description="Generate SysV-to-native import bridges for AnyPS5 libraries")
    bridges.add_argument("library", nargs="+", help="AnyPS5 library targets, such as libc; for a NID that several export, the first one's export gets the bridge")
    bridges.add_argument("--anyps5-build", required=True, help="AnyPS5's build directory, configured with CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    bridges.add_argument("-o", "--output", required=True)
    bridges.add_argument("--libclang", type=Path, help="the libclang library to parse with; the bindings find one by default")
    bridges.add_argument("--report", action="store_true", help="list every export without a bridge and why")
    bridges.add_argument("--jobs", type=int)
    bridges.set_defaults(handler=bridges_command)

    runtime = commands.add_parser("runtime", help="build the runtime for a target", description="Build the ps5lift runtime and SoftFloat as a shared library for a target, and its main as an archive")
    runtime.add_argument("--triple", required=True)
    runtime.add_argument("--cpu", help="CPU of the lifted code the runtime serves, as the lifter's --cpu takes it, generic and native included; the lifter's default CPU by default")
    runtime.add_argument("--output", required=True)
    runtime.add_argument("--clang", default="clang")
    runtime.add_argument("--clangxx", default="clang++")
    runtime.add_argument("--jobs", type=int, default=os.cpu_count())
    runtime.set_defaults(handler=runtime_command)

    runtime_relink = commands.add_parser("runtime-relink", help="test: check that the runtime links a library again that is older than its objects",
                                         description="Build the runtime into OUTPUT, date its shared library before one of its objects as a failed or interrupted link leaves it, "
                                                     "and check that building again links the library again")
    runtime_relink.add_argument("--triple", required=True)
    runtime_relink.add_argument("--output", required=True)
    runtime_relink.add_argument("--clang", required=True)
    runtime_relink.add_argument("--clangxx", required=True)
    runtime_relink.add_argument("--jobs", type=int, default=os.cpu_count())
    runtime_relink.set_defaults(handler=runtime_relink_command)

    runtime_sve = commands.add_parser("runtime-sve", help="test: check that the runtime built for a CPU without SVE has no SVE instructions",
                                      description="Build the runtime for an aarch64 CPU without SVE into OUTPUT and check that its shared library has no SVE instructions, "
                                                  "which would stop a program lifted for that CPU with SIGILL")
    runtime_sve.add_argument("--triple", required=True)
    runtime_sve.add_argument("--cpu", required=True, help="an aarch64 CPU without SVE, as the lifter's --cpu takes it")
    runtime_sve.add_argument("--output", required=True)
    runtime_sve.add_argument("--clang", required=True)
    runtime_sve.add_argument("--clangxx", required=True)
    runtime_sve.add_argument("--jobs", type=int, default=os.cpu_count())
    runtime_sve.set_defaults(handler=runtime_sve_command)

    fixture = commands.add_parser("fixture", help="test: lift a fixture and compare its output", description="Lift an x86-64 fixture, build it for a target and compare its output, or only lift it")
    fixture.add_argument("--ps5lift", required=True)
    fixture.add_argument("--clang", required=True)
    fixture.add_argument("--clangxx", required=True)
    fixture.add_argument("--triple", default=host_triple())
    fixture.add_argument("--fixture", default="basic")
    fixture.add_argument("--optimization", action="append")
    fixture.add_argument("--memory", action="append", choices=["tso", "relaxed"])
    fixture.add_argument("--cpu", help="CPU for the lifted code; native on the host by default")
    fixture.add_argument("--unoptimized-lift", action="store_true", help="lift with ps5lift -O0, which skips LLVM's optimizations")
    fixture.add_argument("--guest-sysroot", help="x86-64 Linux sysroot to build the guest against; the compiler's own by default")
    fixture.add_argument("--lift-only", action="store_true", help="lift for --triple without linking or running the result, on a host that cannot run it")
    fixture.add_argument("--keep", action="store_true")
    fixture.set_defaults(handler=fixture_command)

    libc_fixture = commands.add_parser("libc-fixture", help="test: lift a guest that calls AnyPS5's libraries", description="Lift a guest that calls AnyPS5's libraries through generated bridges and compare its output")
    libc_fixture.add_argument("--ps5lift", required=True)
    libc_fixture.add_argument("--clang", required=True)
    libc_fixture.add_argument("--clangxx", required=True)
    libc_fixture.add_argument("--anyps5-build", required=True, help="AnyPS5's build directory, configured with ANYPS5_GUEST_LIFTED")
    libc_fixture.add_argument("--fixture", choices=sorted(LIBRARY_GUESTS), default="libc")
    libc_fixture.add_argument("--bridges", type=Path, help="bridges generated for the fixture's libraries; generated here when absent")
    libc_fixture.add_argument("--optimization", action="append")
    libc_fixture.add_argument("--keep", action="store_true")
    libc_fixture.set_defaults(handler=libc_fixture_command)

    ps5_fixture = commands.add_parser("ps5-fixture", help="test: relink and lift a PS5-style fixture", description="Relink an AnyPS5 PS5-style fixture, lift it and check its output and exit code")
    ps5_fixture.add_argument("--ps5lift", required=True)
    ps5_fixture.add_argument("--relinker", required=True)
    ps5_fixture.add_argument("--clang", required=True)
    ps5_fixture.add_argument("--clangxx", required=True)
    ps5_fixture.add_argument("--triple", default=host_triple())
    ps5_fixture.add_argument("--fixture", choices=sorted(PS5_EXPECTED), required=True)
    ps5_fixture.set_defaults(handler=ps5_fixture_command)

    libc_tests = commands.add_parser("libc-tests", help="test: run upstream's libc tests on the lifted libraries", description="Run upstream's libc tests against AnyPS5's libraries built for a lifted guest")
    libc_tests.add_argument("--anyps5-build", required=True, help="AnyPS5's build directory, configured with ANYPS5_GUEST_LIFTED and BUILD_TESTING")
    libc_tests.add_argument("--clang", required=True)
    libc_tests.add_argument("--clangxx", required=True)
    libc_tests.add_argument("--build", default=str(BUILD))
    libc_tests.add_argument("--test", action="append", choices=sorted(LIBC_TESTS))
    libc_tests.set_defaults(handler=libc_tests_command)

    record_bridges = commands.add_parser("record-bridges", help="test: check the bridges' record classification", description="Check the bridge generator's SysV classification of records against clang's own lowering")
    record_bridges.add_argument("--clang", required=True, help="clang, to lower calls for x86-64 and to compile the generated record layouts")
    record_bridges.set_defaults(handler=record_bridges_command)

    bench = commands.add_parser("bench", help="time the memory benchmark natively and lifted", description="Time the memory benchmark natively and lifted with each memory model")
    bench.add_argument("--ps5lift", required=True)
    bench.add_argument("--clang", default="clang")
    bench.add_argument("--clangxx", default="clang++")
    bench.add_argument("--triple", default=host_triple())
    bench.add_argument("--cpu", default=None, help="CPU for the lifted code; native on the host by default")
    bench.add_argument("--scale", type=int, default=16, help="working set multiplier; 16 uses about 128 MiB")
    bench.add_argument("--repeat", type=int, default=3)
    bench.set_defaults(handler=bench_command)

    survey = commands.add_parser("survey", help="count the bridges of every AnyPS5 library", description="Generate the bridges of every AnyPS5 library and report how many of its exports they cover")
    survey.add_argument("--anyps5-build", required=True, help="AnyPS5's build directory, configured with CMAKE_EXPORT_COMPILE_COMMANDS=ON")
    survey.add_argument("--library", action="append")
    survey.add_argument("--json", help="also write the results to this file")
    survey.set_defaults(handler=survey_command)

    args = parser.parse_args()
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())
