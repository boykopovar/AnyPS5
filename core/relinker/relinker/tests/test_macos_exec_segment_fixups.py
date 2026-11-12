"""macOS output applies pointer fixups into executable PT_LOAD pages at startup, not through dyld, and keeps their guest protection."""

import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable
from test_guest_intel_trampolines import main_fixture
from test_guest_symbol_names import guest_module
from test_macos_output import MachO, uleb
from test_macos_sce_relro import FAULTS, runnable
from test_macos_trace_imports import PLAIN, PROVIDER, START, library

PT_LOAD = 1
PROT_WRITE = 2
PROT_EXEC = 4


def program_header(image, index, values):
    struct.pack_into("<IIQQQQQQ", image, 64 + index * 56, *values)


def rip(at, length, target):
    return struct.pack("<i", target - (at + length))


def executable_fixture(layout):
    image = executable("a.prx")
    struct.pack_into("<Q", image, 0x4600 + 6 * 16 + 8, 48)
    struct.pack_into("<QQq", image, 0x4918, 0x4A28, 8, 0x4040)
    image[0x4000:0x4007] = b"\xc6\x05" + rip(0x4000, 7, 0x4B00) + b"\x01"
    image[0x4007:0x400D] = b"\xff\x15" + rip(0x4007, 6, 0x4A28)
    image[0x400D:0x4014] = b"\xff\x15" + rip(0x400D, 6, 0x4A20) + b"\xc3"
    image[0x4040] = 0xC3
    if layout == "shared":
        program_header(image, 0, (PT_LOAD, 5, 0x4000, 0x4000, 0x4000, 0xA00, 0xA00, 0x1000))
        program_header(image, 2, (PT_LOAD, 6, 0x4A00, 0x4A00, 0x4A00, 0x600, 0x600, 0x1000))
    elif layout == "rx":
        struct.pack_into("<I", image, 68, 5)
    elif layout == "rwx-two-pages":
        struct.pack_into("<Q", image, 64 + 40, 0x2000)
        image[0x4000:0x4007] = b"\xc6\x05" + rip(0x4000, 7, 0x5008) + b"\x01"
    return image


def module_fixture(layout):
    image = guest_module([("Read", True), ("Write", True)])
    image.extend(bytes(0x100))
    struct.pack_into("<Q", image, 0x8B0 + 8, 0x1010)
    image[0x400:0x406] = b"\xff\x25" + rip(0x1000, 6, 0x2380)
    image[0x410:0x417] = b"\xc6\x05" + rip(0x1010, 7, 0x2388) + b"\x01"
    image[0x417:0x41D] = bytes.fromhex("b809000000c3")
    struct.pack_into("<Q", image, 0x668, 24)
    if layout == "rwx":
        program_header(image, 1, (PT_LOAD, 7, 0x600, 0x2000, 0x2000, 0x400, 0x400, 0x100))
        image[0x990:0x996] = bytes.fromhex("b807000000c3")
        struct.pack_into("<QQq", image, 0x900, 0x2380, 8, 0x2390)
    elif layout == "shared":
        program_header(image, 1, (PT_LOAD, 6, 0x600, 0x2000, 0x2000, 0x400, 0x780, 0x100))
        program_header(image, 3, (PT_LOAD, 5, 0xA80, 0x2780, 0x2780, 0x80, 0x80, 0x80))
        struct.pack_into("<H", image, 56, 4)
        image[0xA80:0xA86] = bytes.fromhex("b807000000c3")
        struct.pack_into("<QQq", image, 0x900, 0x2380, 8, 0x2780)
    elif layout == "rx":
        struct.pack_into("<QQq", image, 0x900, 0x1080, 8, 0x1020)
    return image


def fixup_segments(data):
    machine = MachO(data)
    found = []
    offset, end = machine.dyld_info[0], machine.dyld_info[0] + machine.dyld_info[1]
    while offset < end and data[offset] != 0x00:
        opcode, immediate = data[offset] & 0xF0, data[offset] & 0x0F
        offset += 1
        if opcode == 0x20:
            _, offset = uleb(data, offset)
            found.append(machine.segments[immediate])
        elif opcode not in (0x10, 0x50):
            raise AssertionError(f"rebase opcode {opcode:#x} is not decoded")
    offset, end = machine.dyld_info[2], machine.dyld_info[2] + machine.dyld_info[3]
    while offset < end and data[offset] != 0x00:
        opcode, immediate = data[offset] & 0xF0, data[offset] & 0x0F
        offset += 1
        if opcode == 0x70:
            _, offset = uleb(data, offset)
            found.append(machine.segments[immediate])
        elif opcode == 0x40:
            offset = data.index(b"\0", offset) + 1
        elif opcode == 0x60:
            _, offset = uleb(data, offset)
        elif opcode not in (0x30, 0x50, 0x90):
            raise AssertionError(f"bind opcode {opcode:#x} is not decoded")
    return machine, found


def dyld_info_fixups(path):
    data = bytearray(path.read_bytes())
    machine = MachO(data)
    offset = 32
    for _ in range(struct.unpack_from("<I", data, 16)[0]):
        command, size = struct.unpack_from("<II", data, offset)
        if command == 0x19 and data[offset + 8:offset + 24].rstrip(b"\0") == b"__APS5DATA":
            section = offset + 72
            struct.pack_into("<Q", data, section + 40, machine.segment("__APS5DATA")["vmsize"])
        offset += size
    widened = path.with_suffix(".dyld_info")
    widened.write_bytes(data)
    return subprocess.run(["xcrun", "dyld_info", "-fixups", str(widened)], capture_output=True, text=True, timeout=30)


def check_output(name, path, guest_segment):
    data = path.read_bytes()
    machine, fixups = fixup_segments(data)
    assert fixups, name
    for segment in fixups:
        assert segment["initprot"] & PROT_EXEC == 0, (name, "dyld fixup into an executable segment", segment)
    patched = machine.segment(guest_segment)
    assert (patched["maxprot"], patched["initprot"]) == (7, 5), (name, patched)
    bind_offset, bind_size = machine.dyld_info[2], machine.dyld_info[3]
    assert b"_mprotect\0" in data[bind_offset:bind_offset + bind_size], name
    if sys.platform == "darwin":
        listing = dyld_info_fixups(path)
        assert listing.returncode == 0 and "executable segment" not in listing.stdout + listing.stderr, (name, listing.returncode, listing.stdout, listing.stderr)


def sign(path):
    subprocess.run(["codesign", "-s", "-", "-f", str(path)], check=True, capture_output=True, timeout=30)


def arch():
    return ["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []


def check_executables(relinker, work, native):
    libraries = {}
    if native:
        libraries = {name: library(work, name, code) for name, code in (("a", PROVIDER), ("plain", PLAIN), ("start", START))}
    for layout, segment in (("rwx", "__ELF0"), ("rwx-two-pages", "__ELF0"), ("shared", "__ELF0"), ("rx", None)):
        case = work / f"executable-{layout}"
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(executable_fixture(layout))
        output = case / "output"
        result = subprocess.run([str(relinker), "--macos", "--skip-sce-module", str(source), str(output)],
                                capture_output=True, text=True, timeout=30)
        if segment is None:
            assert result.returncode == 2 and "Runtime import target is not writable" in result.stderr, (layout, result.stdout, result.stderr)
            assert not output.exists(), layout
            continue
        assert result.returncode == 0, (layout, result.stdout, result.stderr)
        check_output(f"executable-{layout}", output, segment)
        if not native:
            continue
        (case / "libs").mkdir()
        for target, source_name in (("a", "a"), ("libc", "plain"), ("libkernel", "start")):
            os.link(libraries[source_name], case / "libs" / (target + ".prx"))
        output.chmod(0o755)
        sign(output)
        run = subprocess.run([*arch(), str(output)], capture_output=True, text=True, timeout=30)
        assert run.returncode == 11, (layout, run.returncode, run.stdout, run.stderr)


def check_modules(relinker, work, native):
    probe = work / "probe"
    if native:
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
        kernel = library(work, "libkernel", PLAIN)
    for layout, segment in (("rwx", "__ELF1"), ("shared", "__ELF1"), ("rx", None)):
        case = work / f"module-{layout}"
        (case / "sce_module").mkdir(parents=True)
        (case / "sce_module" / "fixup.prx").write_bytes(module_fixture(layout))
        source = case / "input.elf"
        source.write_bytes(main_fixture())
        result = subprocess.run([str(relinker), "--macos", str(source), str(case / "output")],
                                capture_output=True, text=True, timeout=30)
        converted = case / "app0" / "sce_module" / "fixup.prx.guest.prx"
        if segment is None:
            assert result.returncode == 2 and "Unmapped or inaccessible guest address 4224" in result.stderr, (layout, result.stdout, result.stderr)
            assert not converted.exists(), layout
            continue
        assert result.returncode == 0, (layout, result.stdout, result.stderr)
        check_output(f"module-{layout}", converted, segment)
        if not native:
            continue
        (case / "libs").mkdir()
        os.link(kernel, case / "libs" / "libkernel.prx")
        sign(converted)
        for mode, expected in (("read", 7), ("write", 9)):
            run = subprocess.run([*arch(), str(probe), str(converted), mode], capture_output=True, text=True, timeout=30)
            assert run.returncode == expected and run.returncode not in FAULTS, (layout, mode, run.returncode, run.stdout, run.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    native = runnable()
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-exec-fixups-") as directory:
        work = Path(directory)
        check_executables(relinker, work, native)
        check_modules(relinker, work, native)
    print("macOS executable-segment fixup tests passed" + ("" if native else " (Mach-O structure only)"))


if __name__ == "__main__":
    main()
