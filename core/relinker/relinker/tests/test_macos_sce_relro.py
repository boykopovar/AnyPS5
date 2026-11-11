"""macOS output makes every PT_SCE_RELRO page read-only after dyld fixups and import tracing, before guest code runs."""

import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from test_guest_symbol_names import guest_module
from test_guest_intel_trampolines import main_fixture
from test_macos_output import MachO
from test_macos_trace_imports import HOOK, PROVIDER, library, run_binary
from test_sce_relro import PT_LOAD, PT_SCE_RELRO, fixture

START = "#include <unistd.h>\nvoid Aps5StartGuest_nid_no_patch(int (*entry)(void)) { _exit(entry()); }\n"
PLAIN = "int Unrelated(void) { return 0; }\n"
FAULTS = (-10, -11)


def header(image, index, values):
    struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, *values)


def write_then_jump(image, at, target, slot):
    image[at:at + 7] = b"\xc6\x05" + struct.pack("<i", target - (at + 7)) + b"\x01"
    image[at + 7:at + 13] = b"\xff\x25" + struct.pack("<i", slot - (at + 13))


def relro_fixture(covered, write=None):
    relro = (PT_SCE_RELRO, 4, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)
    extra = [(PT_LOAD, 6, 0x2000, 0x2000, 0x1000, 0x1000, 0x1000)]
    extra += [(PT_LOAD, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000), relro] if covered else [relro]
    image = fixture(extra)
    if write is not None:
        write_then_jump(image, 0x200, write, 0x1000)
    return image


def short_relro_fixture():
    return fixture([(PT_LOAD, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000),
                    (PT_SCE_RELRO, 4, 0x1100, 0x1100, 0x800, 0x800, 0x1000)])


def traced_fixture(write):
    image = executable("a.prx", extra_dependencies=("libc.prx",))
    header(image, 2, (PT_LOAD, 6, 0x6000, 0x6000, 0x6000, 0x1000, 0x1000, 0x1000))
    header(image, 3, (PT_SCE_RELRO, 4, 0x6000, 0x6000, 0x6000, 0x1000, 0x1000, 0x1000))
    struct.pack_into("<Q", image, 0x4900, 0x6020)
    if write:
        image[0x4000:0x4007] = b"\xc6\x05" + struct.pack("<i", 0x6100 - 0x4007) + b"\x01"
        image[0x4007:0x400e] = b"\xff\x15" + struct.pack("<i", 0x6020 - 0x400d) + b"\xc3"
    else:
        image[0x4000:0x4007] = b"\xff\x15" + struct.pack("<i", 0x6020 - 0x4006) + b"\xc3"
    return image


def relro_module():
    image = guest_module([("Read", True), ("Write", True)])
    struct.pack_into("<Q", image, 0x8B0 + 8, 0x1010)
    image[0x400:0x406] = b"\xff\x25" + struct.pack("<i", 0x2380 - 0x1006)
    image[0x410:0x417] = b"\xc6\x05" + struct.pack("<i", 0x2388 - 0x1017) + b"\x01"
    image[0x417:0x41d] = bytes.fromhex("b809000000c3")
    image[0x420:0x426] = bytes.fromhex("b807000000c3")
    struct.pack_into("<Q", image, 0x668, 24)
    struct.pack_into("<QQq", image, 0x900, 0x2380, 8, 0x1020)
    struct.pack_into("<H", image, 56, 4)
    header(image, 1, (PT_LOAD, 6, 0x600, 0x2000, 0x2000, 0x400, 0x1000, 0x100))
    header(image, 3, (PT_SCE_RELRO, 4, 0x600, 0x2000, 0x2000, 0x400, 0x1000, 0x1000))
    return image


def relink(relinker, source, output, *options):
    result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", *options, str(source), str(output)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (source.name, result.stdout, result.stderr)
    return output.read_bytes()


def binds_mprotect(data):
    machine = MachO(data)
    bind_offset, bind_size = machine.dyld_info[2], machine.dyld_info[3]
    return b"_mprotect\0" in data[bind_offset:bind_offset + bind_size]


def runnable():
    if sys.platform != "darwin" or platform.machine() not in ("x86_64", "arm64"):
        return False
    probe = subprocess.run((["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []) + ["/usr/bin/true"],
                           capture_output=True, text=True, timeout=10)
    if probe.returncode != 0:
        assert os.environ.get("GITHUB_ACTIONS") != "true", "macOS CI must execute x86_64 RELRO checks"
        return False
    return True


def link_libraries(case, libraries, **names):
    (case / "libs").mkdir()
    for target, source in names.items():
        os.link(libraries[source], case / "libs" / (target + ".prx"))


def check_executables(relinker, work, native):
    libraries = {}
    if native:
        libraries = {name: library(work, name, code) for name, code in (
            ("a", PROVIDER), ("hook", HOOK), ("plain", PLAIN), ("start", START))}
    cases = []
    for covered in (False, True):
        layout = "covered" if covered else "standalone"
        cases += [(f"{layout}-read", relro_fixture(covered), (), 42),
                  (f"{layout}-write-relro", relro_fixture(covered, 0x1008), (), FAULTS),
                  (f"{layout}-write-data", relro_fixture(covered, 0x2008), (), 42)]
    cases += [("got-untraced", traced_fixture(False), (), 11),
              ("got-traced", traced_fixture(False), ("--trace-imports",), 77),
              ("got-traced-write", traced_fixture(True), ("--trace-imports",), FAULTS)]
    for name, image, options, expected in cases:
        case = work / name
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(image)
        data = relink(relinker, source, case / "output", *options)
        assert binds_mprotect(data), name
        if not native:
            continue
        if name.startswith("got"):
            link_libraries(case, libraries, a="a", libc="hook" if options else "plain", libkernel="start")
        else:
            link_libraries(case, libraries, libc="plain", libkernel="start")
        (case / "output").chmod(0o755)
        run = run_binary(case / "output")
        allowed = expected if isinstance(expected, tuple) else (expected,)
        assert run.returncode in allowed, (name, run.returncode, run.stdout, run.stderr)

    for name, image in (("no-relro", fixture([(PT_LOAD, 6, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000)])),
                        ("short-relro", short_relro_fixture())):
        case = work / name
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(image)
        assert not binds_mprotect(relink(relinker, source, case / "output")), name


def check_module(relinker, work, native):
    case = work / "module"
    (case / "sce_module").mkdir(parents=True)
    (case / "sce_module" / "relro.prx").write_bytes(relro_module())
    source = case / "input.elf"
    source.write_bytes(main_fixture())
    result = subprocess.run([str(relinker), "--macos", str(source), str(case / "output")],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    converted = case / "app0" / "sce_module" / "relro.prx.guest.prx"
    assert binds_mprotect(converted.read_bytes())
    if not native:
        return
    probe = work / "probe"
    (work / "probe.c").write_text(
        "#include <dlfcn.h>\n#include <stdio.h>\n#include <string.h>\n"
        "int main(int argc, char **argv) {\n"
        "    void *module = dlopen(argv[1], RTLD_NOW);\n"
        "    if (!module) { puts(dlerror()); return 1; }\n"
        "    int (*function)(void) = (int (*)(void))dlsym(module, strcmp(argv[2], \"read\") == 0 ? \"Read#guest\" : \"Write#guest\");\n"
        "    if (!function) return 2;\n"
        "    return function();\n}\n")
    subprocess.run(["xcrun", "clang", "-arch", "x86_64", str(work / "probe.c"), "-o", str(probe)],
                   check=True, capture_output=True, timeout=20)
    libraries = case / "libs"
    libraries.mkdir()
    os.link(library(work, "libkernel", PLAIN), libraries / "libkernel.prx")
    command = ["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []
    for mode, expected in (("read", (7,)), ("write", FAULTS)):
        run = subprocess.run([*command, str(probe), str(converted), mode], capture_output=True, text=True, timeout=30)
        assert run.returncode in expected, (mode, run.returncode, run.stdout, run.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    native = runnable()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-sce-relro-") as directory:
        work = Path(directory)
        check_executables(relinker, work, native)
        check_module(relinker, work, native)
    print("macOS PT_SCE_RELRO tests passed" + ("" if native else " (Mach-O structure only)"))


if __name__ == "__main__":
    main()
