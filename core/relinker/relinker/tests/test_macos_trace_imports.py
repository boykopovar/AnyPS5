"""--trace-imports passes each macOS function import through libc.prx's TraceImport_nid_no_patch before the guest entry."""

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
        "    if (((unsigned long)__builtin_frame_address(0) & 15) != 0) _exit(13);\n"
        "    return strcmp(name, \"shared\") == 0 && strcmp(library, \"a.prx\") == 0 ? (void*)replacement : target;\n"
        "}\n")
WEAK_CALL = (b"\x48\x8b\x05" + struct.pack("<i", 0x4a20 - 0x4007)
             + b"\x48\x85\xc0\x74\x02\xff\xe0\xb8\x21\x00\x00\x00\xc3")
PLAIN = "int Unrelated(void) { return 0; }\n"
START = "#include <unistd.h>\nvoid Aps5StartGuest_nid_no_patch(int (*entry)(void)) { _exit(entry()); }"


def relink(relinker, source, output, *options):
    return subprocess.run([str(relinker), "--macos", "--skip-sce-module", *options, str(source), str(output)],
                          capture_output=True, text=True, timeout=30)


def library(work, name, code):
    source = work / (name + ".c")
    source.write_text(code)
    output = work / (name + ".dylib")
    subprocess.run(["xcrun", "clang", "-arch", "x86_64", "-dynamiclib", str(source),
                    "-Wl,-install_name,@rpath/" + name + ".prx", "-o", str(output)],
                   check=True, capture_output=True, timeout=20)
    return output


def trace_entries(image):
    commands, = struct.unpack_from("<I", image, 16)
    offset = 32
    for _ in range(commands):
        command, size = struct.unpack_from("<II", image, offset)
        if command == 0x19 and image[offset + 8:offset + 24].rstrip(b"\0") == b"__APS5TEXT":
            file_offset, file_size = struct.unpack_from("<QQ", image, offset + 40)
            data = image[file_offset:file_offset + file_size]
            for index in range(len(data) - 21):
                if data[index:index + 3] != b"\x48\x8d\x1d" or data[index + 7:index + 10] != b"\x4c\x8d\x25":
                    continue
                first, = struct.unpack_from("<i", data, index + 3)
                second, = struct.unpack_from("<i", data, index + 10)
                span = second - first + 7
                if span < 0 or span % 24 != 0:
                    continue
                assert b"\x48\x85\xff\x74" in data[index:index + 96]
                return span // 24
            raise AssertionError("Import trace prologue is missing")
        offset += size
    raise AssertionError("__APS5TEXT segment is missing")


def run_binary(output):
    command = (["/usr/bin/arch", "-x86_64"] if platform.machine() == "arm64" else []) + [str(output)]
    return subprocess.run(command, capture_output=True, text=True, timeout=30)


def main():
    relinker = Path(sys.argv[1]).resolve()
    runnable = sys.platform == "darwin" and platform.machine() in ("x86_64", "arm64")
    with tempfile.TemporaryDirectory(prefix="anyps5-macos-trace-imports-") as directory:
        work = Path(directory)
        libraries = {}
        if runnable:
            libraries = {name: library(work, name, code) for name, code in (
                ("a", PROVIDER), ("hook", HOOK), ("plain", PLAIN), ("libkernel", START))}

        for name, options, libc, symbol_info, symbol, relocation, expected, entries in (
                ("untraced", (), "hook", 0x12, "shared#A#B", 6, 11, 0),
                ("traced", ("--trace-imports",), "hook", 0x12, "shared#A#B", 6, 77, 1),
                ("fixed-base", ("--trace-imports", "--macos-fixed-base", "0x400000"), "hook", 0x12, "shared#A#B", 6, 77, 1),
                ("data", ("--trace-imports",), "hook", 0x11, "shared#A#B", 6, 11, 0),
                ("weak", ("--trace-imports",), "hook", 0x22, "shared#A#B", 6, 77, 1),
                ("pointer", ("--trace-imports",), "hook", 0x12, "shared#A#B", 1, 77, 1),
                ("pointer-data", ("--trace-imports",), "hook", 0x11, "shared#A#B", 1, 11, 0),
                ("weak-unresolved", ("--trace-imports",), "hook", 0x22, "absent#A#B", 6, 33, 1),
                ("unsupported", ("--trace-imports",), "plain", 0x12, "shared#A#B", 6, None, 0)):
            case = work / name
            case.mkdir()
            image = executable("a.prx", symbol=symbol, extra_dependencies=("libc.prx",))
            image[0x4898 + 4] = symbol_info
            struct.pack_into("<Q", image, 0x4908, (1 << 32) | relocation)
            if symbol != "shared#A#B":
                image[0x4000:0x4000 + len(WEAK_CALL)] = WEAK_CALL
            source = case / "input.elf"
            source.write_bytes(image)
            output = case / "output"
            result = relink(relinker, source, output, *options)
            assert result.returncode == 0, (name, result.stdout, result.stderr)
            if expected is not None and "--trace-imports" in options:
                assert trace_entries(output.read_bytes()) == entries, (name, entries)
            if not runnable:
                continue
            (case / "libs").mkdir()
            os.link(libraries["a"], case / "libs" / "a.prx")
            os.link(libraries[libc], case / "libs" / "libc.prx")
            os.link(libraries["libkernel"], case / "libs" / "libkernel.prx")
            output.chmod(0o755)
            run = run_binary(output)
            if run.returncode != expected and "Bad CPU type in executable" in run.stderr:
                assert os.environ.get("GITHUB_ACTIONS") != "true", "macOS CI must execute the x86_64 import trace"
                runnable = False
                continue
            if expected is None:
                assert run.returncode != 0 and "TraceImport_nid_no_patch" in run.stderr, (name, run.returncode, run.stdout, run.stderr)
            else:
                assert run.returncode == expected, (name, run.returncode, run.stdout, run.stderr)

        case = work / "without-libc"
        case.mkdir()
        source = case / "input.elf"
        source.write_bytes(executable("a.prx"))
        result = relink(relinker, source, case / "output", "--trace-imports")
        assert result.returncode == 2 and "--trace-imports requires libc.prx" in result.stderr, (result.returncode, result.stderr)
        assert not (case / "output").exists()
        result = relink(relinker, source, case / "output", "--trace-imports", "--lazy-binding")
        assert result.returncode != 0 and "--trace-imports conflicts with --lazy-binding" in result.stderr, (result.returncode, result.stderr)

    print("macOS import tracing tests passed")


if __name__ == "__main__":
    main()
