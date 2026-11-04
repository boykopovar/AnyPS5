"""An exported symbol of the executable must carry a name and attributes that GuestImageReader accepts for a
non-local symbol of a guest module. A defined symbol that is not local, hidden or internal needs a name that is
not empty after stripping the '#' suffixes, a global or weak binding, no st_other bits above the visibility, and
the type STT_NOTYPE, STT_OBJECT, STT_FUNC or STT_TLS. Two such symbols with the same stripped name must agree in
every byte after st_name; identical aliases stay accepted. Local, hidden and internal symbols are not checked.
The check runs before any target-specific code, so an export it rejects fails with the same line, at the
symbol entry's file offset, on every target."""

import struct
import tempfile
from pathlib import Path
import sys

from test_executable_export_bounds import FUNC, LOCAL, NOTYPE, SYMBOL_OFFSET, TARGETS, fail_lines, relink
from test_executable_export_bounds import image as single_symbol_image

GLOBAL_FUNC, GLOBAL_OBJECT, WEAK_OBJECT = 0x12, 0x11, 0x21
ATTRIBUTES = "Unsupported executable export attributes: {}"
CONFLICT = "Conflicting executable exports after stripping #: {}"
EMPTY = "Empty executable export name"

REJECTED = [
    ("empty_name", [("", GLOBAL_FUNC, 0, 0x200, 6)], EMPTY),
    ("hash_only_name", [("#A#B", GLOBAL_FUNC, 0, 0x200, 6)], EMPTY),
    ("ifunc", [("Indirect", 0x1A, 0, 0x200, 6)], ATTRIBUTES.format("Indirect")),
    ("file", [("File", 0x14, 0, 0x1100, 8)], ATTRIBUTES.format("File")),
    ("common", [("Common", 0x15, 0, 0x1100, 8)], ATTRIBUTES.format("Common")),
    ("global_section", [("Section", 0x13, 0, 0x1100, 8)], ATTRIBUTES.format("Section")),
    ("binding_3", [("Binding", 0x31, 0, 0x1100, 8)], ATTRIBUTES.format("Binding")),
    ("gnu_unique", [("Unique", 0xA1, 0, 0x1100, 8)], ATTRIBUTES.format("Unique")),
    ("other_high_bits", [("Other", GLOBAL_OBJECT, 0x10, 0x1100, 8)], ATTRIBUTES.format("Other")),
    ("different_values", [("foo", GLOBAL_OBJECT, 0, 0x1100, 8), ("foo", GLOBAL_OBJECT, 0, 0x1108, 8)],
     CONFLICT.format("foo")),
    ("suffix_different_value", [("foo", GLOBAL_OBJECT, 0, 0x1100, 8), ("foo#A#B", GLOBAL_OBJECT, 0, 0x1108, 8)],
     CONFLICT.format("foo")),
    ("global_and_weak", [("foo", GLOBAL_OBJECT, 0, 0x1100, 8), ("foo", WEAK_OBJECT, 0, 0x1100, 8)],
     CONFLICT.format("foo")),
    ("functions_apart", [("fn", GLOBAL_FUNC, 0, 0x200, 6), ("fn#A#B", GLOBAL_FUNC, 0, 0x206, 6)],
     CONFLICT.format("fn")),
]

ACCEPTED = [
    ("identical_functions", [("fn", GLOBAL_FUNC, 0, 0x200, 6), ("fn#A#B", GLOBAL_FUNC, 0, 0x200, 6)],
     "Duplicate Mach-O export"),
    ("identical_objects", [("obj", GLOBAL_OBJECT, 0, 0x1100, 8), ("obj", GLOBAL_OBJECT, 0, 0x1100, 8)],
     "Duplicate Mach-O export"),
    ("protected_object", [("Protected", GLOBAL_OBJECT, 3, 0x1100, 8)], None),
    ("local_ifunc", [("LocalIndirect", 0x0A, 0, 0x200, 6)], None),
]


def image(symbols):
    data = single_symbol_image("", NOTYPE, 0, 0, LOCAL, 0, 0, False)
    strings = b"\0"
    for index, (name, info, other, value, size) in enumerate(symbols):
        name_offset = len(strings) if name else 0
        if name:
            strings += name.encode("ascii") + b"\0"
        struct.pack_into("<IBBHQQ", data, SYMBOL_OFFSET + index * 24, name_offset, info, other, 1, value, size)
    data[0x4600:0x4600 + len(strings)] = strings
    struct.pack_into("<Q", data, 0x4800 + 1 * 16 + 8, len(strings))
    struct.pack_into("<Q", data, 0x4800 + 7 * 16 + 8, (len(symbols) + 1) * 24)
    return data


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-export-attributes-") as directory:
        for name, symbols, message in REJECTED:
            data = image(symbols)
            expected = [f"FAIL: {message} (offset {SYMBOL_OFFSET + (len(symbols) - 1) * 24:#x})"]
            for flags, target in TARGETS.items():
                run = f"{name}-{target}"
                result, output = relink(relinker, directory, run, data, flags)
                assert result.returncode == 2, (run, result.returncode, result.stdout, result.stderr)
                assert not output.exists(), run
                assert fail_lines(result.stderr) == expected, (run, result.stderr)
        for name, symbols, macos_error in ACCEPTED:
            data = image(symbols)
            for flags, target in TARGETS.items():
                run = f"{name}-{target}"
                result, output = relink(relinker, directory, run, data, flags)
                if target == "macos" and macos_error is not None:
                    assert result.returncode == 2 and macos_error in result.stderr, (run, result.returncode, result.stderr)
                    assert not output.exists(), run
                    continue
                assert result.returncode == 0, (run, result.returncode, result.stdout, result.stderr)
                assert output.exists(), run
                output.unlink()
    print("Executable export attributes test passed")


if __name__ == "__main__":
    main()
