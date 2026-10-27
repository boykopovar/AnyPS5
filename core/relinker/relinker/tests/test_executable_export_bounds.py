"""An exported symbol of the executable must lie inside the image it is exported from, the rule
GuestImageReader applies to every symbol of a guest module and the macOS target applied to the executable
alone. A defined symbol that is not local, hidden or internal must lie inside a PT_LOAD segment that is
executable for STT_FUNC, together with its size and at least one byte; a non-TLS export in SHN_ABS is exempt,
any other special section index is rejected, and an STT_TLS export must lie inside PT_TLS with a regular
section index. Symbols that are not exports are not checked. The check runs before any target-specific code,
so an export it rejects fails with the same line, at the symbol entry's file offset, on every target."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture

PT_LOAD = 1
PT_TLS = 7
NOTYPE, OBJECT, FUNC, TLS = 0, 1, 2, 6
LOCAL, GLOBAL, WEAK = 0, 1, 2
SHN_ABS = 0xfff1
SHN_SPECIAL = 0xff01
TARGETS = {(): "linux", ("--lazy-binding",): "lazy", ("--windows",): "windows", ("--macos",): "macos"}
SYMBOL_OFFSET = 0x4900 + 24
TLS_MEMSZ, TLS_ALIGN = 0x24, 16


def case(name, kind, value, size, message=None, binding=GLOBAL, section=1, visibility=0, tls=False):
    return name, kind, value, size, message, binding, section, visibility, tls


CASES = [
    case("func_in_text", FUNC, 0x200, 6),
    case("object_in_data", OBJECT, 0x1100, 8),
    case("object_fills_load_end", OBJECT, 0x1ff8, 8),
    case("notype_zero_size_inside", NOTYPE, 0x1ff0, 0),
    case("func_last_byte", FUNC, 0xfff, 0),
    case("weak_object", OBJECT, 0x1100, 8, binding=WEAK),
    case("absolute", OBJECT, 0x123456789, 8, section=SHN_ABS),
    case("tls_inside", TLS, 0x10, 4, tls=True),
    case("hidden_outside_ignored", OBJECT, 0x9000, 8, visibility=2),
    case("local_outside_ignored", OBJECT, 0x9000, 8, binding=LOCAL),
    case("internal_outside_ignored", OBJECT, 0x9000, 8, visibility=1),
    case("tls_fills", TLS, 0x20, 4, tls=True),
    case("func_outside", FUNC, 0x9000, 6,
         "Executable export {} at 0x9000 (6 bytes) does not fit inside an executable PT_LOAD segment"),
    case("object_outside", OBJECT, 0x9000, 8,
         "Executable export {} at 0x9000 (8 bytes) does not fit inside a PT_LOAD segment"),
    case("object_past_end", OBJECT, 0x1ffc, 8,
         "Executable export {} at 0x1ffc (8 bytes) does not fit inside a PT_LOAD segment"),
    case("object_straddles_text_end", OBJECT, 0xffc, 8,
         "Executable export {} at 0xffc (8 bytes) does not fit inside a PT_LOAD segment"),
    case("protected_outside", OBJECT, 0x9000, 8,
         "Executable export {} at 0x9000 (8 bytes) does not fit inside a PT_LOAD segment", visibility=3),
    case("marker_at_end", NOTYPE, 0x2000, 0,
         "Executable export {} at 0x2000 is not inside a PT_LOAD segment"),
    case("func_in_data", FUNC, 0x1100, 6,
         "Executable export {} at 0x1100 (6 bytes) does not fit inside an executable PT_LOAD segment"),
    case("func_straddles_text_end", FUNC, 0xffc, 8,
         "Executable export {} at 0xffc (8 bytes) does not fit inside an executable PT_LOAD segment"),
    case("func_zero_in_data", FUNC, 0x1100, 0,
         "Executable export {} at 0x1100 is not inside an executable PT_LOAD segment"),
    case("special_section", OBJECT, 0x1100, 8,
         "Unsupported executable export section: {}", section=SHN_SPECIAL),
    case("tls_beyond", TLS, 0x20, 8,
         "Executable TLS export exceeds PT_TLS: {}", tls=True),
    case("tls_without_segment", TLS, 0x10, 4,
         "Executable TLS export has no PT_TLS segment: {}"),
    case("tls_special_section", TLS, 0x10, 4,
         "Unsupported executable TLS export section: {}", section=SHN_ABS, tls=True),
]


def image(name, kind, value, size, binding, section, visibility, tls):
    strings = b"\0" + name.encode("ascii") + b"\0"
    data = fixture()
    struct.pack_into("<Q", data, 24, 0x200)
    data[0x4200:0x4206] = b"\xb8\x2a\x00\x00\x00\xc3"
    data[0x4fff] = 0xc3
    struct.pack_into("<IIQQQQQQ", data, 64 + 2 * 56, PT_LOAD, 6, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x4000)
    if tls:
        struct.pack_into("<IIQQQQQQ", data, 64 + 3 * 56, PT_TLS, 4, 0x5800, 0x1800, 0x1800, 8, TLS_MEMSZ, TLS_ALIGN)
    data[0x4600:0x4600 + len(strings)] = strings
    struct.pack_into("<IBBHQQ", data, 0x4900 + 24, 1, binding << 4 | kind, visibility, section, value, size)
    tags = [(5, 0x600), (10, len(strings)), (6, 0x900), (11, 24), (7, 0x700), (8, 0), (9, 24), (0x6100003F, 48), (0, 0)]
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", data, 0x4800 + index * 16, *tag)
    return data


def relink(relinker, directory, name, data, flags):
    source = Path(directory) / (name + ".elf")
    output = Path(directory) / (name + ".out")
    source.write_bytes(data)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def fail_lines(stderr):
    return [line for line in stderr.splitlines() if line.startswith("FAIL: ")]


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-export-bounds-") as directory:
        for name, kind, value, size, message, binding, section, visibility, tls in CASES:
            data = image(name, kind, value, size, binding, section, visibility, tls)
            reported = {}
            for flags, target in TARGETS.items():
                run = f"{name}-{target}"
                result, output = relink(relinker, directory, run, data, flags)
                if message is None:
                    assert result.returncode == 0, (run, result.returncode, result.stdout, result.stderr)
                    assert output.exists(), run
                    output.unlink()
                    continue
                assert result.returncode == 2, (run, result.returncode, result.stdout, result.stderr)
                assert not output.exists(), run
                reported[target] = fail_lines(result.stderr)
            if message is not None:
                expected = [f"FAIL: {message.format(name)} (offset {SYMBOL_OFFSET:#x})"]
                assert reported == {target: expected for target in TARGETS.values()}, (name, reported)
    print("Executable export bounds test passed")


if __name__ == "__main__":
    main()
