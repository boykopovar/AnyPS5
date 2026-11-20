from pathlib import Path
import struct
import subprocess
import sys
import tempfile

from test_linux_dynamic_strings import dynamic_strings, file_offset
from test_linux_load_alignment import loads

DT_PLTRELSZ = 2
DT_RELA = 7
DT_RELASZ = 8
DT_STRTAB = 5
DT_SYMTAB = 6
DT_OS_SYMTAB = 0x61000039
DT_OS_SYMTABSZ = 0x6100003F
DT_OS_PLTGOT = 0x61000027
DT_OS_PLTREL = 0x6100002B
DT_OS_JMPREL = 0x61000029
DT_OS_PLTRELSZ = 0x6100002D

JUMP_SLOT_TARGET = 0x300
GLOB_DAT_TARGET = 0x308
ABSOLUTE_TARGET = 0x310
SLOT_ADDEND = 0x1234
ABSOLUTE_ADDEND = 0x55AA
DEFAULT_BASE = 0x400000
NOTE = "GLOB_DAT addends ignored per the x86-64 psABI: 1"
REJECTED = "JUMP_SLOT relocation has a nonzero addend"
TARGETS = (("linux", [], DEFAULT_BASE, b"\x7fELF"), ("fixed", ["--linux-fixed-base", "0x10000000"], 0x10000000, b"\x7fELF"),
           ("lazy", ["--lazy-binding"], 0, b"\x7fELF"), ("windows", ["--windows"], DEFAULT_BASE, b"MZ"),
           ("macos", ["--macos"], DEFAULT_BASE, b"\xcf\xfa\xed\xfe"))


def fixture(jump_slot_addend=SLOT_ADDEND, glob_dat_addend=SLOT_ADDEND, absolute_addend=ABSOLUTE_ADDEND):
    image = bytearray(0x1200)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x1000, 64, 0, 0, 64, 56, 8, 64, 0, 0)
    struct.pack_into("<HiBIB", image, 0x1000, 0x25ff, JUMP_SLOT_TARGET - 0x1006, 0x68, 0, 0xe9)
    tags = [(1, 1), (DT_STRTAB, 0x600), (10, 16), (DT_OS_SYMTAB, 0x620), (11, 24), (DT_OS_SYMTABSZ, 48),
            (DT_RELA, 0x7A0), (DT_RELASZ, 48), (9, 24), (DT_OS_PLTGOT, JUMP_SLOT_TARGET),
            (DT_OS_PLTREL, 7), (DT_OS_JMPREL, 0x700), (DT_OS_PLTRELSZ, 24), (0, 0)]
    struct.pack_into("<IIQQQQQQ", image, 64, 1, 6, 0, 0, 0, 0x1000, 0x1000, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120, 1, 5, 0x1000, 0x1000, 0x1000, 0x200, 0x200, 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 176, 2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 232, 0x61000000, 0, 0, 0, 0, 0x1000, 0x1000, 1)
    for index in range(4, 8):
        struct.pack_into("<I", image, 64 + index * 56, 0x6fffff01)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x700, JUMP_SLOT_TARGET, (1 << 32) | 7, jump_slot_addend)
    struct.pack_into("<QQq", image, 0x7A0, GLOB_DAT_TARGET, (1 << 32) | 6, glob_dat_addend)
    struct.pack_into("<QQq", image, 0x7B8, ABSOLUTE_TARGET, (1 << 32) | 1, absolute_addend)
    struct.pack_into("<IBBHQQ", image, 0x638, 8, 0x12, 0, 0, 0, 0)
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"
    return image


def output_entries(data, tags, tag, size_tag):
    entries = []
    if tags.get(size_tag, 0):
        start = file_offset(data, loads(data), tags[tag])
        entries = [struct.unpack_from("<QQq", data, position) for position in range(start, start + tags[size_tag], 24)]
    return entries


def convert(relinker, source, output, options):
    return subprocess.run([str(relinker), "--skip-sce-module", *options, str(source), str(output)],
                          capture_output=True, text=True, timeout=20)


def check_notes(result, note, tag):
    if note:
        assert note in result.stdout, (tag, note, result.stdout)
    else:
        assert "addends ignored" not in result.stdout, (tag, result.stdout)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-import-slot-addends-") as directory:
        work = Path(directory)
        source = work / "input-jump-slot.elf"
        source.write_bytes(fixture())
        for target, options, _, _ in TARGETS:
            output = work / f"output-jump-slot-{target}.out"
            result = convert(relinker, source, output, options)
            assert result.returncode == 2 and REJECTED in result.stderr, (target, result.returncode, result.stdout, result.stderr)
        cases = ((fixture(jump_slot_addend=0, glob_dat_addend=SLOT_ADDEND, absolute_addend=ABSOLUTE_ADDEND), NOTE, ABSOLUTE_ADDEND),
                 (fixture(jump_slot_addend=0, glob_dat_addend=0, absolute_addend=0), "", 0))
        for index, (source_image, note, absolute_addend) in enumerate(cases):
            source = work / f"input-{index}.elf"
            source.write_bytes(source_image)
            for target, options, base, magic in TARGETS:
                output = work / f"output-{index}-{target}.out"
                result = convert(relinker, source, output, options)
                assert result.returncode == 0, (index, target, result.stdout, result.stderr)
                check_notes(result, note, target)
                data = output.read_bytes()
                assert data.startswith(magic), (index, target)
                if not data.startswith(b"\x7fELF"):
                    continue
                tags, strings = dynamic_strings(data)
                assert [(entry[0], entry[1] & 0xffffffff, entry[2]) for entry in output_entries(data, tags, 23, DT_PLTRELSZ)] == \
                    [(base + JUMP_SLOT_TARGET, 7, 0)], (index, target, note)
                assert sorted((entry[0], entry[1] & 0xffffffff, entry[2]) for entry in output_entries(data, tags, DT_RELA, DT_RELASZ)) == \
                    [(base + GLOB_DAT_TARGET, 6, 0), (base + ABSOLUTE_TARGET, 1, absolute_addend)], (index, target, note)
                symbols = file_offset(data, loads(data), tags[6])
                name_offset, info, _, section, _, _ = struct.unpack_from("<IBBHQQ", data, symbols + 24)
                assert (info, section) == (0x12, 0), (index, target, note, info, section)
                name = strings[name_offset:strings.index(b"\0", name_offset)]
                assert name == b"symbol", (index, target, note, name)
            for level in (1, 2):
                output = work / f"output-{index}-filter{level}.out"
                result = convert(relinker, source, output, [f"unused-filter={level}"])
                assert result.returncode == 0, (index, level, result.stdout, result.stderr)
                check_notes(result, note, (index, level))
    print("Import slot addend tests passed")


if __name__ == "__main__":
    main()
