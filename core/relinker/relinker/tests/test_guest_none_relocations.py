from pathlib import Path
import platform
import struct
import subprocess
import sys
import tempfile

from test_guest_intel_trampolines import main_fixture, pe_sections
from test_guest_module_directories import module_with_symbol
from test_guest_symbol_names import dynamic_tags, file_offset
from test_macos_output import MachO


TARGET = 0x2500
FUNCTION = 0x1000
UNCHANGED = 0x8877665544332211
NOOP = (0, 0, 0)
IGNORED = (0xffffffffffffffff, 0xffffffff00000000, -1)
OVERLAP = (TARGET + 1, 0xffffffff00000000, -1)
RELATIVE = (TARGET, 8, FUNCTION)
RELATIVE_DEFINED = (TARGET, (1 << 32) | 8, FUNCTION)
JUMP_SLOT = (TARGET, (1 << 32) | 7, 0)


def module(rela=(), plt=(), sce=False, lifecycle=False, initial=UNCHANGED, exported=True, symbol_info=0x12):
    image = module_with_symbol(exported)
    image[0x89c] = symbol_info
    image.extend(bytes(0xE00 - len(image)))
    struct.pack_into("<QQ", image, 120 + 32, 0x800, 0x800)
    image[0x400:0x406] = bytes.fromhex("b82a000000c3")
    struct.pack_into("<Q", image, 0xB00, initial)
    tags = [(5, 0x2200), (10, 12), (6, 0x2280), (11, 24), (4, 0x2240),
            (7, 0x2300), (8, len(rela) * 24), (9, 24)]
    if plt:
        tags += [(23, 0x2400), (2, len(plt) * 24), (20, 7), (3, TARGET)]
    if lifecycle:
        tags += [(25, TARGET), (27, 8), (26, TARGET), (28, 8)]
    if sce:
        struct.pack_into("<H", image, 56, 4)
        struct.pack_into("<IIQQQQQQ", image, 232, 0x61000000, 0, 0x800, 0, 0, 0x600, 0x600, 1)
        variants = {5: 0x61000035, 10: 0x61000037, 6: 0x61000039, 11: 0x6100003b,
                    7: 0x6100002f, 8: 0x61000031, 9: 0x61000033,
                    23: 0x61000029, 2: 0x6100002d, 20: 0x6100002b, 3: 0x61000027}
        tags = [(variants.get(tag, tag), value - 0x2200 if tag in (5, 6, 7, 23) else value)
                for tag, value in tags]
    tags.append((0, 0))
    struct.pack_into("<QQ", image, 176 + 32, len(tags) * 16, len(tags) * 16)
    image[0x600:0x800] = bytes(0x200)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x600 + index * 16, *tag)
    for offset, entries in ((0x900, rela), (0xA00, plt)):
        for index, entry in enumerate(entries):
            struct.pack_into("<QQq", image, offset + index * 24, *entry)
    aligned = bytearray(0x2800)
    aligned[:0x400] = image[:0x400]
    aligned[0x1000:0x1100] = image[0x400:0x500]
    aligned[0x2000:0x2800] = image[0x600:0xE00]
    for header, offset in ((64, 0x1000), (120, 0x2000), (176, 0x2000)):
        struct.pack_into("<Q", aligned, header + 8, offset)
    for header in (64, 120):
        struct.pack_into("<Q", aligned, header + 48, 0x1000)
    if sce:
        struct.pack_into("<Q", aligned, 232 + 8, 0x2200)
    return aligned


def change_tag(image, tag, value=None, replacement=None):
    for position in range(0x2000, 0x2200, 16):
        current, _ = struct.unpack_from("<qQ", image, position)
        if current == tag:
            if value is not None:
                struct.pack_into("<Q", image, position + 8, value)
            if replacement is not None:
                struct.pack_into("<q", image, position, replacement)
            return image
    raise AssertionError(f"Missing dynamic tag {tag:#x}")


def unsized_lifecycle(rela, sce, initial):
    image = module(rela, sce=sce, lifecycle=True, initial=initial)
    return change_tag(change_tag(image, 27, replacement=0x61000001), 28, replacement=0x61000001)


def check_output(data, system, rela, plt, initial):
    expected_rela = [(entry[0], entry[1] & 0xffffffff, entry[2])
                     if (entry[1] & 0xffffffff) == 8 and entry[1] >> 32 else entry
                     for entry in rela if entry[1] & 0xffffffff]
    expected_plt = [entry for entry in plt if entry[1] & 0xffffffff]
    relocated = bool(expected_rela or expected_plt)
    if system == "linux":
        assert data.startswith(b"\x7fELF")
        tags = dynamic_tags(data)
        for address, size, expected in ((7, 8, expected_rela), (23, 2, expected_plt)):
            actual = []
            if tags.get(size, 0):
                start = file_offset(data, tags[address])
                actual = [struct.unpack_from("<QQq", data, position)
                          for position in range(start, start + tags[size], 24)]
            assert actual == expected, (actual, expected)
        assert struct.unpack_from("<Q", data, file_offset(data, TARGET))[0] == initial
    elif system == "windows":
        assert data.startswith(b"MZ")
        sections = pe_sections(data)
        code = next(section for section in sections if section[0] == b".elf0")
        writable = next(section for section in sections if section[0] == b".elf1")
        pe_header, = struct.unpack_from("<I", data, 0x3C)
        base, = struct.unpack_from("<Q", data, pe_header + 24 + 24)
        value, = struct.unpack_from("<Q", data, writable[3] + TARGET - 0x2000)
        assert value == (base + code[1] if relocated else initial), hex(value)
    else:
        assert data.startswith(b"\xcf\xfa\xed\xfe")
        macho = MachO(data)
        code = macho.segment("__ELF0")
        writable = macho.segment("__ELF1")
        value, = struct.unpack_from("<Q", data, writable["fileoff"] + TARGET - 0x2000)
        assert value == (code["vmaddr"] if relocated else initial), hex(value)


def invalid_cases(sce):
    yield "relative-symbol-mismatched-value", module([(TARGET, (1 << 32) | 8, FUNCTION + 8)], sce=sce), "RELATIVE relocation has a symbol"
    yield "relative-symbol-undefined", module([(TARGET, (1 << 32) | 8, 0)], sce=sce, exported=False), "RELATIVE relocation has a symbol"
    yield "invalid-symbol", module([NOOP, (TARGET, (99 << 32) | 6, 0)], sce=sce), "Unsupported relocation or symbol index"
    yield "invalid-target", module([NOOP, (0xffffffffffffffff, 8, FUNCTION)], sce=sce), "Unmapped or inaccessible guest address"
    yield "overlapping-real", module([NOOP, RELATIVE, RELATIVE], sce=sce), "Overlapping guest relocations"
    yield "invalid-plt-kind", module(plt=[NOOP, RELATIVE], sce=sce), "Unsupported relocation or symbol index"
    yield "unrelocated-lifecycle", module([(TARGET, 0, FUNCTION)], sce=sce, lifecycle=True, initial=FUNCTION), "Unrelocated guest lifecycle pointer"
    size = 0x61000031 if sce else 8
    entry = 0x61000033 if sce else 9
    address = 0x6100002f if sce else 7
    plt_format = 0x6100002b if sce else 20
    yield "lifecycle-without-address", change_tag(module([RELATIVE], sce=sce, lifecycle=True, initial=0), 25, replacement=0x61000001), "Incomplete guest lifecycle array"
    yield "partial-entry", change_tag(module([NOOP], sce=sce), size, 23), "Invalid relocation table size"
    yield "entry-size", change_tag(module([NOOP], sce=sce), entry, 16), "Invalid RELA entry size"
    yield "unmapped-table", change_tag(module([NOOP], sce=sce), address, 0xffffffffffffffff), ("SCE table exceeds" if sce else "Unmapped ELF address")
    yield "plt-format", change_tag(module(plt=[NOOP], sce=sce), plt_format, 17), "Unsupported PLT relocation format"
    yield "missing-plt-format", change_tag(module(plt=[NOOP], sce=sce), plt_format, replacement=0x61000001), "Missing or ambiguous dynamic tag"
    ambiguous = module(plt=[NOOP], sce=sce)
    yield "ambiguous-plt-format", change_tag(ambiguous, 0x61000027 if sce else 3, value=7,
                                            replacement=20 if sce else 0x6100002b), "Missing or ambiguous dynamic tag"


def main():
    if sys.argv[1] == "--load":
        import ctypes
        library = ctypes.CDLL(sys.argv[2])
        function = getattr(library, "shared#guest")
        function.restype = ctypes.c_int
        assert function() == 42
        address = ctypes.cast(function, ctypes.c_void_p).value
        pointer = ctypes.c_uint64.from_address(address + TARGET - FUNCTION).value
        assert pointer == address, (hex(pointer), hex(address))
        assert ctypes.CFUNCTYPE(ctypes.c_int)(pointer)() == 42
        return
    relinker = Path(sys.argv[1]).resolve()
    cases = (("baseline", [RELATIVE], [], False, UNCHANGED),
             ("relative-defined-symbol", [RELATIVE_DEFINED], [], False, UNCHANGED),
             ("relative-defined-object", [RELATIVE_DEFINED], [], False, UNCHANGED),
             ("mixed", [NOOP, OVERLAP, RELATIVE, IGNORED, (TARGET, 0, -1)], [], False, UNCHANGED),
             ("only-none", [NOOP, IGNORED, OVERLAP], [], False, UNCHANGED),
             ("plt-mixed", [], [NOOP, OVERLAP, JUMP_SLOT, IGNORED], False, UNCHANGED),
             ("plt-only-none", [], [NOOP, IGNORED, OVERLAP], False, UNCHANGED),
             ("lifecycle-zero", [(TARGET, 0xffffffff00000000, -1)], [], True, 0),
             ("lifecycle-relative", [(TARGET, 0, -1), RELATIVE, OVERLAP], [], True, 0))
    with tempfile.TemporaryDirectory(prefix="anyps5-guest-none-") as directory:
        work = Path(directory)

        def convert(name, system, image):
            case = work / name / system
            modules = case / "sce_module"
            modules.mkdir(parents=True)
            guest = modules / "module.prx"
            guest.write_bytes(image)
            source = case / "input.elf"
            source.write_bytes(main_fixture())
            output = case / ("output.exe" if system == "windows" else "output.elf")
            flags = {"linux": [], "windows": ["--windows"], "macos": ["--macos"]}[system]
            result = subprocess.run([str(relinker), *flags, str(source), str(output)],
                                    capture_output=True, text=True, timeout=10)
            assert guest.read_bytes() == image
            return result, case / "app0" / "sce_module" / "module.prx.guest.prx", output

        for sce in (False, True):
            for name, rela, plt, lifecycle, initial in cases:
                symbol_info = 0x11 if name == "relative-defined-object" else 0x12
                image = module(rela, plt, sce, lifecycle, initial, symbol_info=symbol_info)
                for system in ("linux", "windows", "macos"):
                    result, artifact, output = convert(f"{sce}-{name}", system, image)
                    assert result.returncode == 0, (sce, name, system, result.stdout, result.stderr)
                    check_output(artifact.read_bytes(), system, rela, plt, initial)
                    if name in ("mixed", "plt-mixed"):
                        if system == "linux" and sys.platform == "linux" and platform.machine() == "x86_64":
                            executed = subprocess.run([sys.executable, __file__, "--load", str(artifact)],
                                                      capture_output=True, text=True, timeout=10)
                            assert executed.returncode == 0, (executed.stdout, executed.stderr)
            for name, rela, initial in (("unsized-lifecycle", [RELATIVE], UNCHANGED),
                                        ("unsized-lifecycle-unread", [(TARGET, 0, FUNCTION)], FUNCTION)):
                image = unsized_lifecycle(rela, sce, initial)
                for system in ("linux", "windows", "macos"):
                    result, artifact, output = convert(f"{sce}-{name}", system, image)
                    assert result.returncode == 0, (sce, name, system, result.stdout, result.stderr)
                    check_output(artifact.read_bytes(), system, rela, [], initial)
                    if system == "linux":
                        assert not {25, 26, 27, 28} & set(dynamic_tags(artifact.read_bytes())), name
            for name, image, error in invalid_cases(sce):
                result, artifact, output = convert(f"{sce}-{name}", "linux", image)
                assert result.returncode == 2 and error in result.stderr, (sce, name, result.stderr)
                assert not artifact.exists() and not output.exists(), name
    print("Guest no-op relocation tests passed")


if __name__ == "__main__":
    main()
