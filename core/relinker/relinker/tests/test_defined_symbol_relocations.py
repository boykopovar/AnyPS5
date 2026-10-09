from pathlib import Path
import struct
import subprocess
import sys
import tempfile


DT_NEEDED = 1
DT_PLTRELSZ = 2
DT_PLTGOT = 3
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_PLTREL = 20
DT_JMPREL = 23
DT_OS_SYMTABSZ = 0x6100003F

R_X86_64_64 = 1
R_X86_64_GLOB_DAT = 6
R_X86_64_JUMP_SLOT = 7
R_X86_64_RELATIVE = 8

STT_FUNC = 2
STT_TLS = 6
STT_GNU_IFUNC = 10
STB_GLOBAL = 1
SHN_ABS = 0xFFF1
SHN_COMMON = 0xFFF2

SYMBOL_VALUE = 0x200
GOT = 0x300
RELA_TABLE = 0x7A0
JMPREL_TABLE = 0x700


def fixture(rela=(), jmprel=(), shndx=5, value=SYMBOL_VALUE, symbol_type=STT_FUNC, symtab_offset=0x620):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 7, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    tags = [
        (DT_NEEDED, 1),
        (DT_STRTAB, 0x600),
        (DT_STRSZ, 16),
        (DT_SYMTAB, symtab_offset),
        (DT_SYMENT, 24),
        (DT_OS_SYMTABSZ, 48),
        (DT_RELA, RELA_TABLE),
        (DT_RELASZ, len(rela) * 24),
        (DT_RELAENT, 24),
        (DT_PLTGOT, GOT),
        (DT_PLTREL, DT_RELA),
        (DT_JMPREL, JMPREL_TABLE),
        (DT_PLTRELSZ, len(jmprel) * 24),
        (0, 0),
    ]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     0x61000000, 0, 0, 0, 0, len(image), len(image), 1)
    for header in (232, 288, 344, 400):
        struct.pack_into("<IIQQQQQQ", image, header,
                         0x61000000, 0, 0, 0, 0, 0, 0, 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    for index, (target, kind, addend) in enumerate(jmprel):
        struct.pack_into("<QQq", image, JMPREL_TABLE + index * 24, target, (1 << 32) | kind, addend)
    for index, (target, kind, addend) in enumerate(rela):
        struct.pack_into("<QQq", image, RELA_TABLE + index * 24, target, (1 << 32) | kind, addend)
    entry = struct.pack("<IBBHQQ", 8, (STB_GLOBAL << 4) | symbol_type, 0, shndx, value, 6)
    start = symtab_offset + 24
    image[start:start + 24] = entry[:len(image) - start]
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"[:16]
    return image


def read_output_relocations(data):
    phoff, = struct.unpack_from("<Q", data, 32)
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    loads = []
    dynamic = None
    for index in range(phnum):
        kind, _, offset, vaddr, _, filesz, memsz, _ = struct.unpack_from("<IIQQQQQQ", data, phoff + index * phentsize)
        if kind == 1:
            loads.append((vaddr, offset, filesz))
        elif kind == 2:
            dynamic = (offset, filesz)

    def to_offset(address):
        for vaddr, offset, filesz in loads:
            if vaddr <= address < vaddr + filesz:
                return offset + address - vaddr
        raise AssertionError("address outside loaded segments: %#x" % address)

    tags = {}
    for position in range(dynamic[0], dynamic[0] + dynamic[1], 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == 0:
            break
        tags.setdefault(tag, value)
    symtab = to_offset(tags[DT_SYMTAB])
    strtab = to_offset(tags[DT_STRTAB])
    relocations = []
    for table, size in ((DT_RELA, DT_RELASZ), (DT_JMPREL, DT_PLTRELSZ)):
        if table not in tags:
            continue
        start = to_offset(tags[table])
        for position in range(start, start + tags[size], 24):
            target, info, addend = struct.unpack_from("<QQq", data, position)
            name = None
            if info >> 32:
                nameoff, = struct.unpack_from("<I", data, symtab + (info >> 32) * 24)
                name = data[strtab + nameoff:data.index(b"\0", strtab + nameoff)]
            relocations.append((target, info & 0xffffffff, addend, name))
    return relocations


def read_windows_pointer(data, address):
    header, = struct.unpack_from("<I", data, 0x3C)
    image_base, = struct.unpack_from("<Q", data, header + 48)
    count, = struct.unpack_from("<H", data, header + 6)
    optional_size, = struct.unpack_from("<H", data, header + 20)
    sections = {}
    for index in range(count):
        current = header + 24 + optional_size + index * 40
        name = data[current:current + 8].split(b"\0", 1)[0]
        sections[name] = struct.unpack_from("<IIII", data, current + 8)[1:]
    image_rva, _, image_offset = sections[b".elf0"]
    value, = struct.unpack_from("<Q", data, image_offset + address)
    reloc_rva, reloc_size = struct.unpack_from("<II", data, header + 24 + 112 + 5 * 8)
    reloc = next(offset + reloc_rva - rva for rva, size, offset in sections.values() if rva <= reloc_rva < rva + size)
    relocated = set()
    position = reloc
    while position < reloc + reloc_size:
        page, block = struct.unpack_from("<II", data, position)
        for entry in struct.unpack_from("<%dH" % ((block - 8) // 2), data, position + 8):
            if entry >> 12 == 10:
                relocated.add(page + (entry & 0xFFF))
        position += block
    return value - image_base - image_rva, image_rva + address in relocated


def convert(relinker, work, name, image, windows, level=0):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    command = [str(relinker), "--skip-sce-module"] + (["--windows"] if windows else []) + [
        "unused-filter=%d" % level, str(source), str(output)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    return result, output


def expect_failure(relinker, work, name, image, windows, message, level=0):
    result, output = convert(relinker, work, name, image, windows, level)
    assert result.returncode == 2 and message in result.stderr and not output.exists(), (
        name, result.returncode, result.stdout, result.stderr)


def expect_resolved(relinker, work, name, image, windows, target, resolved):
    result, output = convert(relinker, work, name, image, windows)
    assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stdout, result.stderr)
    assert "NID input: 0 references" in result.stdout, (name, result.stdout)
    if windows:
        assert read_windows_pointer(output.read_bytes(), target) == (resolved, True), name
    else:
        relocations = read_output_relocations(output.read_bytes())
        assert relocations == [(target, R_X86_64_RELATIVE, resolved, None)], (name, relocations)


def expect_import(relinker, work, name, image, windows, kind):
    result, output = convert(relinker, work, name, image, windows)
    assert result.returncode == 0 and output.exists(), (name, result.returncode, result.stdout, result.stderr)
    assert "NID input: 1 references" in result.stdout, (name, result.stdout)
    if not windows:
        relocations = read_output_relocations(output.read_bytes())
        assert relocations == [(GOT, kind, 0, b"symbol")], (name, relocations)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-defined-symbol-") as directory:
        work = Path(directory)
        for windows in (False, True):
            suffix = "-windows" if windows else "-linux"
            expect_resolved(relinker, work, "glob-dat" + suffix,
                            fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)]), windows, GOT, SYMBOL_VALUE)
            expect_resolved(relinker, work, "abs64-addend" + suffix,
                            fixture(rela=[(GOT, R_X86_64_64, 4)]), windows, GOT, SYMBOL_VALUE + 4)
            expect_failure(relinker, work, "jump-slot" + suffix,
                           fixture(jmprel=[(GOT, R_X86_64_JUMP_SLOT, 0)]), windows,
                           "JUMP_SLOT relocation against a defined symbol is not supported")
            expect_failure(relinker, work, "glob-dat-addend" + suffix,
                           fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 8)]), windows,
                           "GLOB_DAT relocation has a nonzero addend")
            expect_failure(relinker, work, "abs-section" + suffix,
                           fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)], shndx=SHN_ABS), windows,
                           "Relocation against a symbol with a special section index is not supported")
            expect_failure(relinker, work, "common-section" + suffix,
                           fixture(rela=[(GOT, R_X86_64_64, 0)], shndx=SHN_COMMON), windows,
                           "Relocation against a symbol with a special section index is not supported")
            expect_failure(relinker, work, "tls-symbol" + suffix,
                           fixture(rela=[(GOT, R_X86_64_64, 0)], symbol_type=STT_TLS), windows,
                           "Relocation against a defined TLS symbol is not supported")
            expect_failure(relinker, work, "ifunc-symbol" + suffix,
                           fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)], symbol_type=STT_GNU_IFUNC), windows,
                           "Relocation against a defined IFUNC symbol is not supported")
            expect_failure(relinker, work, "symbol-past-eof" + suffix,
                           fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)], symtab_offset=0xFE0), windows,
                           "Symbol table entry out of bounds")
            result, output = convert(relinker, work, "filter-level-1" + suffix,
                                     fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)]), windows, 1)
            assert result.returncode == 0 and output.exists(), (result.stdout, result.stderr)
            assert "CFG/GOT filtering skipped: relocations against defined symbols are not modeled" in result.stdout, result.stdout
            expect_failure(relinker, work, "filter-level-2" + suffix,
                           fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)]), windows,
                           "Strict NID filtering does not support relocations against defined symbols", 2)
            expect_import(relinker, work, "undefined-glob-dat" + suffix,
                          fixture(rela=[(GOT, R_X86_64_GLOB_DAT, 0)], shndx=0, value=0), windows, R_X86_64_GLOB_DAT)
            expect_import(relinker, work, "undefined-jump-slot" + suffix,
                          fixture(jmprel=[(GOT, R_X86_64_JUMP_SLOT, 0)], shndx=0, value=0), windows, R_X86_64_JUMP_SLOT)
    print("Defined symbol relocation tests passed")


if __name__ == "__main__":
    main()
