from pathlib import Path
import struct
import subprocess
import sys
import tempfile


DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_NEEDED = 1
DT_OS_STRTAB = 0x61000035
DT_OS_STRSZ = 0x61000037
DT_OS_SYMTAB = 0x61000039
DT_OS_SYMENT = 0x6100003B
DT_OS_SYMTABSZ = 0x6100003F
DT_OS_RELA = 0x6100002F
DT_OS_RELASZ = 0x61000031
DT_OS_RELAENT = 0x61000033


def fixture(os_tags=False, str_size=16, needed_offset=1, symbol_offset=8, table_bytes=None, outside_bytes=b""):
    image = bytearray(0x1000)
    image[:16] = b"\x7fELF\x02\x01\x01" + bytes(9)
    struct.pack_into("<HHIQQQIHHHHHH", image, 16,
                     3, 62, 1, 0x200, 64, 0, 0, 64, 56, 3, 64, 0, 0)
    image[0x200:0x206] = b"\xff\x25\xfa\x00\x00\x00"
    tags = [
        (DT_NEEDED, needed_offset),
        (DT_OS_STRTAB if os_tags else DT_STRTAB, 0x600),
        (DT_OS_STRSZ if os_tags else DT_STRSZ, str_size),
        (DT_OS_SYMTAB if os_tags else DT_SYMTAB, 0x620),
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
    struct.pack_into("<QQq", image, 0x700, 0x300, (1 << 32) | 6, 0)
    struct.pack_into("<I", image, 0x620 + 24, symbol_offset)
    if table_bytes is None:
        table_bytes = b"\x00lib.so\x00symbol\x00\x00"
    image[0x600:0x600 + len(table_bytes)] = table_bytes
    image[0x600 + len(table_bytes):0x600 + len(table_bytes) + len(outside_bytes)] = outside_bytes
    return image


def run(relinker, work, name, image, expected_error=None):
    source = work / (name + ".elf")
    output = work / (name + ".out")
    source.write_bytes(image)
    result = subprocess.run([str(relinker), "--skip-sce-module", "--windows", str(source), str(output)],
                            capture_output=True, text=True, timeout=20)
    if expected_error is None:
        assert result.returncode == 0 and output.exists(), (name, result.stdout, result.stderr)
    else:
        assert result.returncode == 2 and expected_error in result.stderr and not output.exists(), (
            name, result.returncode, result.stdout, result.stderr)


def main():
    relinker = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anyps5-string-table-") as directory:
        work = Path(directory)
        run(relinker, work, "sysv-valid", fixture())
        run(relinker, work, "os-valid", fixture(os_tags=True))
        run(relinker, work, "needed-offset", fixture(needed_offset=16, outside_bytes=b"lib.so\x00"),
            "Dynamic string offset is outside DT_STRSZ")
        run(relinker, work, "needed-offset-max", fixture(needed_offset=0xffffffffffffffff,
                                                            outside_bytes=b"lib.so\x00"),
            "Dynamic string offset is outside DT_STRSZ")
        run(relinker, work, "symbol-offset", fixture(symbol_offset=16, outside_bytes=b"symbol\x00"),
            "Dynamic string offset is outside DT_STRSZ")
        run(relinker, work, "zero-size", fixture(str_size=0),
            "Dynamic string offset is outside DT_STRSZ")
        run(relinker, work, "huge-size", fixture(str_size=0xffffffffffffffff),
            "Dynamic string table is out of bounds")
        run(relinker, work, "truncated-table", fixture(str_size=0xb00),
            "Dynamic string table is out of bounds")
        run(relinker, work, "termination-after-boundary",
            fixture(str_size=7, table_bytes=b"\x00lib.so\x00symbol\x00"),
            "Dynamic string is not NUL-terminated within DT_STRSZ")
        run(relinker, work, "symbol-termination-after-boundary",
            fixture(str_size=14, table_bytes=b"\x00lib.so\x00symbol"),
            "Dynamic string is not NUL-terminated within DT_STRSZ")
        run(relinker, work, "exact-boundary",
            fixture(str_size=8, symbol_offset=1, table_bytes=b"\x00lib.so\x00"))
    print("Dynamic string table bounds tests passed")


if __name__ == "__main__":
    main()
