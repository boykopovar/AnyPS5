"""Symbol-table file offsets must be rejected before a wrapping addition."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile


DT_NEEDED = 1
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_OS_STRTAB = 0x61000035
DT_OS_STRSZ = 0x61000037
DT_OS_SYMTAB = 0x61000039
DT_OS_SYMENT = 0x6100003B
DT_OS_SYMTABSZ = 0x6100003F
DT_OS_RELA = 0x6100002F
DT_OS_RELASZ = 0x61000031
DT_OS_RELAENT = 0x61000033
R_X86_64_GLOB_DAT = 6


def fixture(sym_offset=0x620, sym_index=1, os_tags=False, name_offset=8):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    tags = [
        (DT_NEEDED, 1),
        (DT_OS_STRTAB if os_tags else DT_STRTAB, 0x600),
        (DT_OS_STRSZ if os_tags else DT_STRSZ, 16),
        (DT_OS_SYMTAB if os_tags else DT_SYMTAB, sym_offset),
        (DT_OS_SYMENT if os_tags else DT_SYMENT, 24),
        (DT_OS_SYMTABSZ, 48),
        (DT_OS_RELA if os_tags else DT_RELA, 0x700),
        (DT_OS_RELASZ if os_tags else DT_RELASZ, 24),
        (DT_OS_RELAENT if os_tags else DT_RELAENT, 24),
        (0, 0),
    ]
    struct.pack_into("<IIQQQQQQ", image, 64,
                     1, 7, 0, 0, 0, len(image), len(image), 0x1000)
    struct.pack_into("<IIQQQQQQ", image, 120,
                     2, 6, 0x400, 0x400, 0x400, len(tags) * 16, len(tags) * 16, 8)
    struct.pack_into("<IIQQQQQQ", image, 176,
                     0x61000000, 0, 0, 0, 0, len(image), len(image), 1)
    for index, tag in enumerate(tags):
        struct.pack_into("<qQ", image, 0x400 + index * 16, *tag)
    struct.pack_into("<QQq", image, 0x700, 0x300, (sym_index << 32) | R_X86_64_GLOB_DAT, 0)
    image[0x600:0x610] = b"\x00lib.so\x00symbol\x00\x00"
    name_at = sym_offset + sym_index * 24
    if 0 <= name_at <= len(image) - 4:
        struct.pack_into("<I", image, name_at, name_offset)
    return image


def run(relinker, work, name, image, expected_error=None, windows=True):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    command = [str(relinker), "--skip-sce-module"]
    if windows:
        command.append("--windows")
    command.extend([str(source), str(output)])
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    if expected_error is None:
        magic = b"MZ" if windows else b"\x7fELF"
        produced = output.read_bytes() if output.exists() else b""
        if result.returncode != 0 or not produced.startswith(magic) or b"symbol" not in produced:
            raise AssertionError((name, result.returncode, result.stdout, result.stderr, produced[:2]))
        return
    if result.returncode != 2 or expected_error not in result.stderr or output.exists():
        raise AssertionError((name, result.returncode, result.stdout, result.stderr))


def main():
    relinker = Path(sys.argv[1]).resolve()
    error = "Symbol table entry out of bounds"
    with tempfile.TemporaryDirectory(prefix="anyps5-symbol-table-") as directory:
        work = Path(directory)
        run(relinker, work, "base-past-eof", fixture(sym_offset=0x1000, os_tags=True), error)
        run(relinker, work, "base-near-max", fixture(sym_offset=0xFFFFFFFFFFFFFFF0, os_tags=True), error)
        run(relinker, work, "wrapped-name-check", fixture(sym_offset=0xFFFFFFFFFFFFFFFC, sym_index=0, os_tags=True), error)
        run(relinker, work, "index-crosses-eof", fixture(sym_index=0x100000, os_tags=True), error)
        run(relinker, work, "truncated-name", fixture(sym_offset=0x1000 - 27, os_tags=True), error)
        run(relinker, work, "linux-base-near-max", fixture(sym_offset=0xFFFFFFFFFFFFFFF0, os_tags=True), error, windows=False)
        run(relinker, work, "sysv-valid", fixture())
        run(relinker, work, "os-valid", fixture(os_tags=True))
        run(relinker, work, "table-at-eof", fixture(sym_offset=0x1000 - 48, os_tags=True))
    print("Symbol table bounds tests passed")


if __name__ == "__main__":
    main()
