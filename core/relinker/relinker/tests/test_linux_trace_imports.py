"""--trace-imports passes each Linux function import through libc.prx's TraceImport_nid_no_patch before the guest entry."""

import os
from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from guest_import_fixtures import executable


PROVIDER = "#include <unistd.h>\nint shared(void) { _exit(11); }\n"
HOOK = ("#include <string.h>\n"
        "#include <unistd.h>\n"
        "static int replacement(void) { _exit(77); }\n"
        "void* TraceImport_nid_no_patch(void* target, const char* name, const char* library) {\n"
        "    return strcmp(name, \"shared\") == 0 && strcmp(library, \"a.prx\") == 0 ? (void*)replacement : target;\n"
        "}\n")
PLAIN = "int Unrelated(void) { return 0; }\n"


def relink(relinker, source, output, *options):
    return subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                          capture_output=True, text=True, timeout=30)


def library(work, name, code):
    source = work / (name + ".c")
    source.write_text(code)
    output = work / (name + ".so")
    subprocess.run([os.environ.get("CC", "cc"), "-shared", "-fPIC", str(source), "-o", str(output)], check=True)
    return output.read_bytes()


PERMISSIONS_PROVIDER = r"""
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
static void permissions(void *address) {
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) _exit(92);
    char line[512], access[5];
    unsigned long start, end;
    while (fgets(line, sizeof(line), maps)) {
        if (sscanf(line, "%lx-%lx %4s", &start, &end, access) == 3
            && (uintptr_t)address >= start && (uintptr_t)address < end) {
            dprintf(1, "%s\n", access);
            fclose(maps);
            return;
        }
    }
    _exit(93);
}
int shared(void *slot, int last) {
    permissions(slot);
    permissions((char *)slot + 7);
    if (last) _exit(11);
    return 11;
}
"""
PERMISSIONS_HOOK = r"""
#include <string.h>
#include <unistd.h>
static int (*original)(void *, int);
static int replacement(void *slot, int last) {
    int result = original(slot, 0) + 66;
    if (last) _exit(result);
    return result;
}
void *TraceImport_nid_no_patch(void *target, const char *name, const char *library) {
    if (strcmp(name, "shared") != 0 || strcmp(library, "a.prx") != 0) _exit(94);
    original = target;
    write(2, "hook\n", 5);
    return replacement;
}
"""


def permissions_fixture(slots, segments):
    image = executable("a.prx", extra_dependencies=("libc.prx",),
                       extra_symbols=("shared#A#B",) * (len(slots) - 1))
    struct.pack_into("<I", image, 68, 5)
    for index, (start, size, flags) in enumerate(segments):
        struct.pack_into("<IIQQQQQQ", image, 176 + index * 56,
                         1, flags, start, start, start, size, size, 0x1000)
    code = bytearray(b"\x48\x83\xec\x08")
    for index, slot in enumerate(slots):
        struct.pack_into("<Q", image, 0x4900 + index * 24, slot)
        code += b"\x48\x8d\x3d" + struct.pack("<i", slot - (0x4000 + len(code) + 7))
        code += b"\xbe" + struct.pack("<I", index == len(slots) - 1)
        code += b"\xff\x15" + struct.pack("<i", slot - (0x4000 + len(code) + 6))
    code += b"\x0f\x0b"
    image[0x4000:0x4000 + len(code)] = code
    return image


def check_permissions(relinker, work, runnable):
    libraries = {}
    if runnable:
        libraries = {"a.prx": library(work, "permissions-provider", PERMISSIONS_PROVIDER),
                     "libc.prx": library(work, "permissions-hook", PERMISSIONS_HOOK)}
    failures = []
    cases = (
        ("data", (0x5000, 0x5008, 0x5ffc, 0x6010), ((0x5000, 0x2000, 6),), ("rw-p",) * 8),
        ("shared-code", (0x4a20, 0x4a28), (), ("rwxp",) * 4),
        ("flagless", (0x5000, 0x6010), ((0x5000, 0x2000, 0),), ("rwxp",) * 4),
        ("rounded-nx", (0x5000, 0x5800), ((0x5000, 0x800, 0), (0x5800, 0x1800, 6)), ("rw-p",) * 4),
        ("rounded-x", (0x5000, 0x5800), ((0x5000, 0x800, 6), (0x5800, 0x1800, 0)), ("rwxp",) * 4),
        ("overlap", (0x5000, 0x6000), ((0x5000, 0x1800, 0), (0x6000, 0x1000, 6)),
         ("rwxp", "rwxp", "rw-p", "rw-p")),
        ("cross-permissions", (0x5ffc,), ((0x5000, 0x1800, 0), (0x6000, 0x1000, 6)),
         ("rwxp", "rw-p")),
        ("relro", (0x5000, 0x6000), ((0x5000, 0x2000, 7),),
         ("r--p", "r--p", "rwxp", "rwxp"), (0x5000, 0x1000)),
        ("relro-rounded", (0x5000, 0x6000), ((0x5000, 0x2000, 7),),
         ("r--p", "r--p", "rwxp", "rwxp"), (0x5800, 0x1000)),
        ("relro-partial-page", (0x5000, 0x6000), ((0x5000, 0x2000, 7),),
         ("rwxp",) * 4, (0x5800, 0x700)),
    )
    for name, slots, segments, expected, *relro in cases:
        for base in (None, "0x400000", "0x100010000"):
            for trace in (False, True):
                label = f"{name}-{base or 'default'}-{'traced' if trace else 'untraced'}"
                case = work / label
                case.mkdir()
                image = permissions_fixture(slots, segments)
                if not segments:
                    struct.pack_into("<I", image, 68, 7)
                if relro:
                    start, size = relro[0]
                    struct.pack_into("<IIQQQQQQ", image, 64 + 3 * 56,
                                     0x6474e552, 4, start, start, start, size, size, 1)
                source = case / "input.elf"
                source.write_bytes(image)
                output = case / "output.elf"
                options = ["--trace-imports"] if trace else []
                if base is not None:
                    options += ["--linux-fixed-base", base]
                result = relink(relinker, source, output, *options)
                assert result.returncode == 0, (label, result.stdout, result.stderr)
                converted = output.read_bytes()
                assert struct.unpack_from("<H", converted, 16)[0] == 2, label
                if relro:
                    phoff = struct.unpack_from("<Q", converted, 32)[0]
                    entsize, count = struct.unpack_from("<HH", converted, 54)
                    headers = [struct.unpack_from("<IIQQQQQQ", converted, phoff + index * entsize)
                               for index in range(count)]
                    assert sum(header[0] == 0x6474e552 for header in headers) == 1, label
                if not runnable:
                    continue
                (case / "libs").mkdir()
                for filename, contents in libraries.items():
                    (case / "libs" / filename).write_bytes(contents)
                output.chmod(0o755)
                run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
                assert run.returncode == (77 if trace else 11), (label, run)
                assert run.stderr == "hook\n" * (len(slots) if trace else 0), (label, run.stderr)
                actual = tuple(run.stdout.splitlines())
                print(label, actual)
                expected_access = tuple("rw-p" if trace and access == "r--p" else access for access in expected)
                if actual != expected_access:
                    failures.append((label, expected_access, actual))
    assert not failures, ("Tracing changed effective slot page permissions", failures)


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform.startswith("linux") and platform.machine() in ("x86_64", "AMD64")
    with tempfile.TemporaryDirectory(prefix="anyps5-linux-trace-imports-") as directory:
        work = Path(directory)
        libraries = {}
        if runnable:
            libraries = {name: library(work, name, code) for name, code in (("a", PROVIDER), ("hook", HOOK), ("plain", PLAIN))}

        for name, options, libc, symbol_info, expected in (
                ("untraced", (), "hook", 0x12, 11),
                ("traced", ("--trace-imports",), "hook", 0x12, 77),
                ("fixed-base", ("--trace-imports", "--linux-fixed-base", "0x400000"), "hook", 0x12, 77),
                ("data", ("--trace-imports",), "hook", 0x11, 11),
                ("weak", ("--trace-imports",), "hook", 0x22, 11),
                ("unsupported", ("--trace-imports",), "plain", 0x12, None)):
            case = work / name
            case.mkdir()
            image = executable("a.prx", extra_dependencies=("libc.prx",))
            image[0x4898 + 4] = symbol_info
            source = case / "input.elf"
            source.write_bytes(image)
            output = case / "output.elf"
            result = relink(relinker, source, output, *options)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if not runnable:
                continue
            (case / "libs").mkdir()
            (case / "libs" / "a.prx").write_bytes(libraries["a"])
            (case / "libs" / "libc.prx").write_bytes(libraries[libc])
            output.chmod(0o755)
            run = subprocess.run([str(output)], capture_output=True, text=True, timeout=30)
            if expected is None:
                assert run.returncode != 0 and "TraceImport_nid_no_patch" in run.stderr, (name, run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)

        case = work / "without-libc"
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(executable("a.prx"))
        result = relink(relinker, source, case / "output.elf", "--trace-imports")
        assert result.returncode == 2 and "--trace-imports requires libc.prx" in result.stderr, (result.returncode, result.stderr)
        assert not (case / "output.elf").exists()
        result = relink(relinker, source, case / "output.elf", "--trace-imports", "--lazy-binding")
        assert result.returncode != 0 and "--trace-imports conflicts with --lazy-binding" in result.stderr, (result.returncode, result.stderr)
        result = relink(relinker, source, case / "output", "--trace-imports", "--macos")
        assert result.returncode == 2 and "--trace-imports requires libc.prx" in result.stderr, (result.returncode, result.stderr)

        check_permissions(relinker, work, runnable)

    print("Linux import tracing tests passed")


if __name__ == "__main__":
    main()
