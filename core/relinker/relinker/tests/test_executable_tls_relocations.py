"""Initial-exec TLS relocations in the executable (R_X86_64_TPOFF64, DTPOFF64, DTPMOD64), as a PS5 eboot
has them in its GOT. The Linux ELF keeps them dynamic and symbolless, with the variable's offset in the
TLS block as the addend, for the dynamic loader to resolve against where it puts the block; a named one
against a defined STT_TLS symbol is folded the same way, and one against an import stays named, typed
STT_TLS. The Windows and macOS images resolve TPOFF64 and DTPOFF64 statically against their block,
AlignUp(p_memsz, p_align) below the thread pointer. Windows also resolves the executable's own DTPMOD64
(symbol 0, addend 0) to the address of the module's PE `_tls_index`, the module word __tls_get_addr
reads, and rejects one inside the PT_TLS initialization template or crossing its boundary, as the guest
module writer does; macOS still rejects DTPMOD64. Every target rejects the link-time types GOT32, PLT32,
GOTPCREL, 32S, GOTPCRELX and REX_GOTPCRELX, which glibc's ld.so does not apply; the Windows and macOS
images also reject other types they cannot apply, such as PC32 and 32. Every target rejects a non-TLS
relocation against an STT_TLS symbol, defined or imported."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_load_alignment import fixture
from test_macos_output import MachO
from test_tls_function_coverage import pe_bytes_at

PT_LOAD = 1
PT_TLS = 7
DTPMOD64, DTPOFF64, TPOFF64, TPOFF32 = 16, 17, 18, 23
LINK_TIME = {3: "got32", 4: "plt32", 9: "gotpcrel", 11: "32s", 41: "gotpcrelx", 42: "rex-gotpcrelx"}
TARGETS = {(): "linux", ("--lazy-binding",): "lazy", ("--windows",): "windows", ("--macos",): "macos"}
TLS_MEMSZ, TLS_ALIGN = 0x24, 16
TLS_BLOCK = 0x30
DEFAULT_BASE = 0x400000
STRINGS = b"\0Tls\0Data\0ImportedTls\0Function\0libFunction.prx\0"
TLS_NAME, DATA_NAME, IMPORT_NAME = 1, 5, 10
FUNCTION_NAME, LIBRARY_NAME = 22, 31

NULL, TLS_SYMBOL, DATA_SYMBOL, IMPORTED, FUNCTION = 0, 1, 2, 3, 4


def image(relocations, tls=True, symbol_bytes=None, tls_memsz=TLS_MEMSZ):
    """The load-alignment fixture with a writable segment (vaddr 0x1000) holding a GOT and a PT_TLS of
    tls_memsz bytes with an up to 8-byte template, a symbol table and the given (offset, type, symbol,
    addend) relocations."""
    data = fixture()
    struct.pack_into("<Q", data, 24, 0x200)
    data[0x4200:0x4206] = b"\xb8\x2a\x00\x00\x00\xc3"
    struct.pack_into("<IIQQQQQQ", data, 64 + 2 * 56, PT_LOAD, 6, 0x5000, 0x1000, 0x1000, 0x1000, 0x1000, 0x4000)
    if tls:
        struct.pack_into("<IIQQQQQQ", data, 64 + 3 * 56, PT_TLS, 4, 0x5800, 0x1800, 0x1800, min(8, tls_memsz), tls_memsz, TLS_ALIGN)
    data[0x5800:0x5808] = bytes(range(1, 9))
    data[0x4600:0x4600 + len(STRINGS)] = STRINGS
    struct.pack_into("<IBBHQQ", data, 0x4900 + TLS_SYMBOL * 24, TLS_NAME, 0x16, 0, 1, 0x10, 4)
    struct.pack_into("<IBBHQQ", data, 0x4900 + DATA_SYMBOL * 24, DATA_NAME, 0x11, 0, 1, 0x1100, 8)
    struct.pack_into("<IBBHQQ", data, 0x4900 + IMPORTED * 24, IMPORT_NAME, 0x16, 0, 0, 0, 0)
    struct.pack_into("<IBBHQQ", data, 0x4900 + FUNCTION * 24, FUNCTION_NAME, 0x12, 0, 0, 0, 0)
    for index, (offset, kind, symbol, addend) in enumerate(relocations):
        struct.pack_into("<QQq", data, 0x4700 + index * 24, offset, symbol << 32 | kind, addend)
    tags = [(5, 0x600), (10, len(STRINGS)), (6, 0x900), (11, 24), (7, 0x700), (8, len(relocations) * 24), (9, 24), (0, 0)]
    if symbol_bytes is not None:
        tags.insert(-1, (0x6100003F, symbol_bytes))
    struct.pack_into("<IIQQQQQQ", data, 120, 2, 6, 0x4800, 0x800, 0x800, len(tags) * 16, len(tags) * 16, 8)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", data, 0x4800 + index * 16, *tag)
    return data


def needing(data):
    """The image with DT_NEEDED libFunction.prx first in its dynamic table, which the Windows entry stub
    requires to bind imports."""
    tags = [(1, LIBRARY_NAME)]
    for position in range(0x4800, 0x4900, 16):
        tags.append(struct.unpack_from("<qQ", data, position))
        if tags[-1][0] == 0:
            break
    struct.pack_into("<QQ", data, 120 + 32, len(tags) * 16, len(tags) * 16)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", data, 0x4800 + index * 16, *tag)
    return data


def relink(relinker, directory, name, data, *flags):
    source = Path(directory) / (name + ".elf")
    output = Path(directory) / (name + ".out")
    source.write_bytes(data)
    result = subprocess.run([str(relinker), "--skip-sce-module", *flags, str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    return result, output


def dynamic_relocations(elf):
    """The output's (offset, type, symbol name, symbol type, addend) relocations from DT_RELA."""
    phoff, = struct.unpack_from("<Q", elf, 0x20)
    phentsize, phnum = struct.unpack_from("<HH", elf, 0x36)
    headers = [struct.unpack_from("<IIQQQQQQ", elf, phoff + index * phentsize) for index in range(phnum)]

    def offset(address):
        for kind, _, file_offset, vaddr, _, size, _, _ in headers:
            if kind == PT_LOAD and vaddr <= address < vaddr + size:
                return file_offset + address - vaddr
        raise AssertionError(f"unmapped address {address:#x}")

    dynamic = next(header for header in headers if header[0] == 2)
    tags = {}
    for position in range(dynamic[2], dynamic[2] + dynamic[5], 16):
        tag, value = struct.unpack_from("<qQ", elf, position)
        if tag == 0:
            break
        tags.setdefault(tag, value)
    symbols, strings = offset(tags[6]), offset(tags[5])
    result = []
    for position in range(offset(tags[7]), offset(tags[7]) + tags[8], 24):
        target, info, addend = struct.unpack_from("<QQq", elf, position)
        name, symbol_info = struct.unpack_from("<IB", elf, symbols + (info >> 32) * 24)
        end = elf.index(b"\0", strings + name)
        result.append((target, info & 0xffffffff, elf[strings + name:end].decode(), symbol_info & 15, addend))
    return result


def macho_bytes_at(data, address, size):
    for segment in MachO(data).segments:
        if segment["vmaddr"] <= address and address + size <= segment["vmaddr"] + segment["filesize"]:
            offset = segment["fileoff"] + address - segment["vmaddr"]
            return data[offset:offset + size]
    raise AssertionError(f"unmapped address {address:#x}")


def pe_directories(pe):
    """The image base and the data directory table of a PE32 or PE32+ image."""
    header = struct.unpack_from("<I", pe, 0x3c)[0]
    optional = header + 24
    magic = struct.unpack_from("<H", pe, optional)[0]
    if magic == 0x20b:
        return struct.unpack_from("<Q", pe, optional + 24)[0], optional + 112
    assert magic == 0x10b, hex(magic)
    return struct.unpack_from("<I", pe, optional + 28)[0], optional + 96


def pe_tls_index_address(pe):
    """The virtual address of the module's `_tls_index` from the PE TLS directory."""
    _, directories = pe_directories(pe)
    address, size = struct.unpack_from("<II", pe, directories + 9 * 8)
    assert address != 0 and size >= 40, "the Windows image must have a TLS directory"
    return struct.unpack_from("<Q", pe_bytes_at(pe, address, 40), 16)[0]


def pe_base_relocation_targets(pe):
    """Every DIR64 target RVA of the PE base relocation table."""
    _, directories = pe_directories(pe)
    address, size = struct.unpack_from("<II", pe, directories + 5 * 8)
    assert address != 0 and size != 0, "the Windows image must have base relocations"
    data = pe_bytes_at(pe, address, size)
    targets = set()
    position = 0
    while position + 8 <= len(data):
        page, block = struct.unpack_from("<II", data, position)
        if block < 8:
            break
        for entry in range(position + 8, position + block, 2):
            value, = struct.unpack_from("<H", data, entry)
            if value >> 12 == 10:
                targets.add(page + (value & 0xfff))
        position += block
    return targets


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-exec-tls-") as directory:
        for section in (1, 0xff00, 0xfff1, 0xfff2, 0xffff):
            for binding in (0, 1):
                for kind in (DTPMOD64, DTPOFF64, TPOFF64):
                    for flags, target in TARGETS.items():
                        data = image([(0x1000, kind, TLS_SYMBOL, 0)])
                        struct.pack_into("<BBH", data, 0x4900 + TLS_SYMBOL * 24 + 4, binding << 4 | 6, 0, section)
                        name = f"tls-section-{target}-{section}-{binding}-{kind}"
                        result, output = relink(relinker, directory, name, data, *flags)
                        if section == 1 and kind == DTPMOD64 and target == "macos":
                            assert result.returncode == 2 and "DTPMOD64 relocation is not supported" in result.stderr, (
                                name, result.returncode, result.stderr)
                            assert not output.exists(), name
                        elif section == 1:
                            assert result.returncode == 0 and output.exists(), (name, result.stderr)
                        else:
                            assert result.returncode == 2 and "Unsupported executable TLS symbol section" in result.stderr, (
                                name, result.returncode, result.stderr)
                            assert not output.exists(), name

        local = [(0x1000, TPOFF64, NULL, 4), (0x1008, DTPOFF64, NULL, 8), (0x1010, TPOFF64, TLS_SYMBOL, 2)]
        result, output = relink(relinker, directory, "linux", image(local + [(0x1018, DTPMOD64, NULL, 0),
                                                                              (0x1020, TPOFF64, IMPORTED, 0)]))
        assert result.returncode == 0, (result.stdout, result.stderr)
        elf = output.read_bytes()
        relocations = dynamic_relocations(elf)
        tls = sorted(relocation for relocation in relocations if relocation[1] in (DTPMOD64, DTPOFF64, TPOFF64))
        assert tls == [(DEFAULT_BASE + 0x1000, TPOFF64, "", 0, 4), (DEFAULT_BASE + 0x1008, DTPOFF64, "", 0, 8),
                       (DEFAULT_BASE + 0x1010, TPOFF64, "", 0, 0x12), (DEFAULT_BASE + 0x1018, DTPMOD64, "", 0, 0),
                       (DEFAULT_BASE + 0x1020, TPOFF64, "ImportedTls", 6, 0)], relocations
        assert any(struct.unpack_from("<I", elf, 64 + index * 56)[0] == PT_TLS
                   for index in range(struct.unpack_from("<H", elf, 0x38)[0])), "PT_TLS must be kept"

        for value, size in ((TLS_MEMSZ - 4, 4), (TLS_MEMSZ, 0), (TLS_MEMSZ + 1, 0), (TLS_MEMSZ - 1, 2), (0xffffffffffffffff, 1)):
            for flags, target in TARGETS.items():
                data = image([(0x1000, TPOFF64, TLS_SYMBOL, 0)])
                struct.pack_into("<QQ", data, 0x4900 + TLS_SYMBOL * 24 + 8, value, size)
                result, output = relink(relinker, directory, f"tls-bounds-{target}-{value}-{size}", data, *flags)
                if value <= TLS_MEMSZ and size <= TLS_MEMSZ - value:
                    assert result.returncode == 0 and output.exists(), (target, value, size, result.stderr)
                else:
                    assert result.returncode == 2 and "TLS symbol exceeds TLS segment" in result.stderr, (
                        target, value, size, result.returncode, result.stderr)
                    assert not output.exists()

        result, output = relink(relinker, directory, "windows", image(local), "--windows")
        assert result.returncode == 0, (result.stdout, result.stderr)
        pe = output.read_bytes()
        values = [struct.unpack("<q", pe_bytes_at(pe, 0x10000 + address, 8))[0] for address in (0x1000, 0x1008, 0x1010)]
        assert values == [4 - TLS_BLOCK, 8, 0x12 - TLS_BLOCK], values

        result, output = relink(relinker, directory, "windows-dtpmod",
                                image([(0x1000, DTPMOD64, NULL, 0), (0x1028, DTPMOD64, TLS_SYMBOL, 0)]), "--windows")
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
        pe = output.read_bytes()
        image_base, _ = pe_directories(pe)
        tls_index = pe_tls_index_address(pe)
        assert pe_bytes_at(pe, tls_index - image_base, 4), "the TLS index must be mapped"
        modules = [struct.unpack("<Q", pe_bytes_at(pe, 0x10000 + address, 8))[0] for address in (0x1000, 0x1028)]
        assert modules == [tls_index, tls_index], [hex(value) for value in modules]
        assert {0x10000 + 0x1000, 0x10000 + 0x1028} <= pe_base_relocation_targets(pe), "the module word needs a base relocation"

        result, output = relink(relinker, directory, "macos", image(local), "--macos")
        assert result.returncode == 0, (result.stdout, result.stderr)
        macho = output.read_bytes()
        values = [struct.unpack("<q", macho_bytes_at(macho, 0x140010000 + address, 8))[0] for address in (0x1000, 0x1008, 0x1010)]
        assert values == [4 - TLS_BLOCK, 8, 0x12 - TLS_BLOCK], values

        for kind in (DTPMOD64, DTPOFF64, TPOFF64):
            for symbol, declared_bytes in ((TLS_SYMBOL, 24), (IMPORTED, 72)):
                name = f"symbol-bounds-{kind}-{symbol}"
                result, output = relink(relinker, directory, name,
                                       image([(0x1000, kind, symbol, 0)], symbol_bytes=declared_bytes))
                assert result.returncode == 2 and "Symbol table entry out of bounds" in result.stderr, (
                    name, result.returncode, result.stdout, result.stderr)
                assert not output.exists(), name

        non_tls = "FAIL: Non-TLS relocation names an STT_TLS symbol (offset 0x4700)"
        for kind in (1, 2, 6, 10):
            for symbol in (TLS_SYMBOL, IMPORTED):
                for flags, target in TARGETS.items():
                    name = f"{target}-non-tls-{kind}-{symbol}"
                    data = image([(0x1000, kind, symbol, 0)], symbol_bytes=96)
                    result, output = relink(relinker, directory, name, data, *flags)
                    assert result.returncode == 2 and non_tls in result.stderr, (name, result.returncode, result.stderr)
                    assert not output.exists(), name
            data = image([(0x1000, kind, DATA_SYMBOL, 0)], symbol_bytes=96)
            result, output = relink(relinker, directory, f"non-tls-{kind}-data", data)
            assert result.returncode == 0 and output.exists(), (kind, result.returncode, result.stderr)

        bound = "Runtime-bound executable TLS template relocation is not supported"
        for target, kind in ((0x1800, 1), (0x1800, 6), (0x1804, 1), (0x17fc, 6)):
            data = needing(image([(target, kind, FUNCTION, 0)], symbol_bytes=120))
            for flags in (("--windows",), ("--windows", "--lazy-binding")):
                name = f"template-import-{target:x}-{kind}-{len(flags)}"
                result, output = relink(relinker, directory, name, data, *flags)
                assert result.returncode == 2 and f"FAIL: {bound} (offset {target:#x})" in result.stderr, (
                    name, result.returncode, result.stderr)
                assert not output.exists(), name
            for flags, label in (((), "linux"), (("--lazy-binding",), "lazy"), (("--macos",), "macos")):
                name = f"template-import-{target:x}-{kind}-{label}"
                result, output = relink(relinker, directory, name, data, *flags)
                assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)
        for target in (0x1100, 0x17f8, 0x1808):
            data = needing(image([(target, 1, FUNCTION, 0)], symbol_bytes=120))
            for flags in (("--windows",), ("--windows", "--lazy-binding")):
                name = f"outside-template-import-{target:x}-{len(flags)}"
                result, output = relink(relinker, directory, name, data, *flags)
                assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)
        for target in (0x17f8, 0x1808):
            name = f"outside-template-dtpmod-{target:x}"
            result, output = relink(relinker, directory, name, image([(target, DTPMOD64, NULL, 0)]), "--windows")
            assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stderr)

        failures = [
            ("tpoff32", [(0x1000, TPOFF32, NULL, 0)], True, (), "Unsupported relocation type 0x17"),
            ("tpoff32-invalid-sym", [(0x1000, TPOFF32, 0x1234, 0)], True, (), "Unsupported relocation type 0x17"),
            ("unknown-reloc-invalid-sym", [(0x1000, 0x99, 0x1234, 0)], True, (), "Unsupported relocation type 0x99"),
            ("not-tls-symbol", [(0x1000, TPOFF64, DATA_SYMBOL, 0)], True, (), "not STT_TLS"),
            ("no-tls-segment", [(0x1000, TPOFF64, NULL, 0)], False, (), "without a non-empty PT_TLS segment"),
            ("dtpmod-addend", [(0x1000, DTPMOD64, NULL, 1)], True, (), "DTPMOD64 relocation has a nonzero addend"),
            ("windows-dtpmod-in-template", [(0x1800, DTPMOD64, NULL, 0)], True, ("--windows",),
             "Runtime-bound executable TLS template relocation is not supported"),
            ("windows-dtpmod-crosses-template-start", [(0x17fc, DTPMOD64, NULL, 0)], True, ("--windows",),
             "Executable relocation crosses the TLS template boundary"),
            ("windows-dtpmod-crosses-template-end", [(0x1804, DTPMOD64, NULL, 0)], True, ("--windows",),
             "Executable relocation crosses the TLS template boundary"),
            ("windows-import", [(0x1000, TPOFF64, IMPORTED, 0)], True, ("--windows",), "Imported TLS relocation"),
            ("windows-pc32", [(0x1000, 2, NULL, 0)], True, ("--windows",), "Relocation type 2 is not supported for this target"),
            ("windows-32", [(0x1000, 10, NULL, 0)], True, ("--windows",), "Relocation type 10 is not supported for this target"),
            ("macos-pc32", [(0x1000, 2, NULL, 0)], True, ("--macos",), "Relocation type 2 is not supported for this target"),
            ("macos-32", [(0x1000, 10, NULL, 0)], True, ("--macos",), "Relocation type 10 is not supported for this target"),
            ("macos-dtpmod", [(0x1000, DTPMOD64, NULL, 0)], True, ("--macos",), "DTPMOD64 relocation is not supported"),
            ("macos-import", [(0x1000, TPOFF64, IMPORTED, 0)], True, ("--macos",), "Imported TLS relocation"),
        ] + [(f"{target}-{name}", [(0x1000, kind, NULL, 0)], True, flags, f"Unsupported relocation type {kind:#x} (offset 0x4700)")
             for kind, name in LINK_TIME.items() for flags, target in TARGETS.items()]
        for name, relocations, has_tls, flags, error in failures:
            result, output = relink(relinker, directory, name, image(relocations, has_tls), *flags)
            assert result.returncode == 2 and error in result.stderr, (name, result.returncode, result.stderr)
            assert not output.exists(), name

        undefined = "FAIL: TLS relocation names an undefined symbol that is not STT_TLS (offset 0x4700)"
        for kind in (DTPMOD64, DTPOFF64, TPOFF64):
            for info, symbol_name in ((0x10, IMPORT_NAME), (0x11, IMPORT_NAME), (0x12, IMPORT_NAME), (0x13, IMPORT_NAME), (0x1a, IMPORT_NAME), (0x1e, IMPORT_NAME), (0x03, 0)):
                for flags, target in TARGETS.items():
                    name = f"{target}-undefined-{kind}-{info:x}"
                    data = image([(0x1000, kind, IMPORTED, 0)])
                    struct.pack_into("<IB", data, 0x4900 + IMPORTED * 24, symbol_name, info)
                    result, output = relink(relinker, directory, name, data, *flags)
                    assert result.returncode == 2 and undefined in result.stderr, (name, result.returncode, result.stderr)
                    assert not output.exists(), name

        empty_tls = "FAIL: TLS relocation in an executable without a non-empty PT_TLS segment (offset 0x4700)"
        for kind, addend in ((TPOFF64, 4), (DTPOFF64, 4), (DTPMOD64, 0)):
            for symbol in (NULL, TLS_SYMBOL):
                for flags, target in TARGETS.items():
                    name = f"{target}-empty-tls-{kind}-{symbol}"
                    data = image([(0x1000, kind, symbol, addend)], tls_memsz=0)
                    result, output = relink(relinker, directory, name, data, *flags)
                    assert result.returncode == 2 and empty_tls in result.stderr, (name, result.returncode, result.stderr)
                    assert not output.exists(), name

        result, output = relink(relinker, directory, "linux-empty-tls-import", image([(0x1000, TPOFF64, IMPORTED, 0)], tls_memsz=0))
        assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
    print("Executable TLS relocation test passed")


if __name__ == "__main__":
    main()
