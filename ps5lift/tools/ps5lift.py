#!/usr/bin/env python3
"""Build ps5lift's runtime and run ps5lift's tests.

`ps5lift.py runtime` builds the runtime for a target, `fixture` lifts a test program and compares its output with the
native build's, and `bench` times the memory benchmark natively and lifted."""
import argparse
import concurrent.futures
import difflib
import fcntl
import hashlib
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ANYPS5 = ROOT.parent
FIXTURES = ROOT / "tests" / "fixtures"
SOFTFLOAT = ANYPS5 / "3rdparty" / "softfloat"

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


def runtime_link_arguments(ps5lift, triple, clang, clangxx):
    return link_arguments(*build_runtime(triple, None, runtime_directory(Path(ps5lift).resolve().parent.parent, triple, None), clang, clangxx, os.cpu_count()))


def build_guest(clang, work, name, optimization, defines=(), vectorize=False, suffix="", sysroot=None):
    cross = [f"--sysroot={sysroot}"] if sysroot else []
    library = work / "libhost.so"
    if not library.exists():
        run([clang, *GUEST_FLAGS, *cross, "-O2", "-fPIC", "-shared", FIXTURES / "host_stub.c", "-o", library])
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
                report = run([args.ps5lift, guest, "-o", lifted, "--triple", args.triple, "--memory", memory, *target, *(["-O0"] if args.unoptimized_lift else [])]).stderr
                if args.lift_only:
                    print(f"PASS {label}, lifted only: {report.strip()}")
                    continue
                program = work / f"{guest.name}-{memory}"
                run([args.clangxx, f"--target={args.triple}", "-std=c++20", "-O2", f"-I{ROOT / 'runtime' / 'include'}", FIXTURES / "host_bridge.cpp", lifted, *runtime_link_arguments(args.ps5lift, args.triple, args.clang, args.clangxx), "-fuse-ld=lld", "-pthread", "-o", program])
                try:
                    result = subprocess.run([*runner, program], capture_output=True, text=True, timeout=PROGRAM_TIMEOUT)
                except subprocess.TimeoutExpired:
                    sys.stderr.write(f"{label}: still running after {PROGRAM_TIMEOUT} s\n")
                    print(f"FAIL {label}: timed out")
                    failures += 1
                    continue
                ok = result.returncode == 0 and compare(expected, result.stdout, label)
                if result.returncode != 0:
                    sys.stderr.write(f"{label}: exit code {result.returncode}\n{result.stderr}")
                print(f"{'PASS' if ok else 'FAIL'} {label}: {report.strip()}")
                failures += not ok
    finally:
        if args.keep:
            print(f"kept {work}")
        else:
            shutil.rmtree(work)
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
            report = run([args.ps5lift, guests[suffix], "-o", lifted, "--triple", args.triple, "--memory", memory, *lifter_target(args.triple, runner, args.cpu)]).stderr.strip()
            print(f"{name}: {report}")
            programs[name] = work / name
            run([args.clangxx, f"--target={args.triple}", "-std=c++20", "-O2", f"-I{ROOT / 'runtime' / 'include'}", FIXTURES / "host_bridge.cpp", lifted, *runtime_link_arguments(args.ps5lift, args.triple, args.clang, args.clangxx), "-fuse-ld=lld", "-pthread", "-o", programs[name]])

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


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

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

    bench = commands.add_parser("bench", help="time the memory benchmark natively and lifted", description="Time the memory benchmark natively and lifted with each memory model")
    bench.add_argument("--ps5lift", required=True)
    bench.add_argument("--clang", default="clang")
    bench.add_argument("--clangxx", default="clang++")
    bench.add_argument("--triple", default=host_triple())
    bench.add_argument("--cpu", default=None, help="CPU for the lifted code; native on the host by default")
    bench.add_argument("--scale", type=int, default=16, help="working set multiplier; 16 uses about 128 MiB")
    bench.add_argument("--repeat", type=int, default=3)
    bench.set_defaults(handler=bench_command)

    args = parser.parse_args()
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())
